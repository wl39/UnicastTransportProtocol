#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <stdio.h>

#include <errno.h>
void perror(const char *s);

#include "CrudpSocket.h"

#define HEADER_SIZE ((uint32_t)12)
#define MAX_WINDOW_SIZE ((uint32_t)1388)
#define MIN_WINDOW_SIZE ((uint32_t)10)

// Random seed for Transmitter Sequence number
#define R_T ((unsigned int)160005106)
// Random seed for Receiver Sequence number
#define R_R ((unsigned int)23204)

// Global variables
uint32_t windowSize = 100;
uint32_t seqNumber = 0;
uint32_t ackNumber = 0;
uint32_t startSeq = 0;

UdpSocket_t *
setupUdpSocket_t(const char *hostname, const uint16_t port)
{
    int error = 0;
    struct in_addr addr;

    UdpSocket_t *udp = (UdpSocket_t *)calloc(1, sizeof(UdpSocket_t));

    /* local end-point, ephemeral port number */
    if ((hostname == (char *)0) && (port == 0))
    {
        udp->addr.sin_addr.s_addr = htonl(INADDR_ANY);
        udp->addr.sin_port = htons(INADDR_ANY);
    }

    /* local end-point, designated port number */
    else if ((hostname == (char *)0) && (port != 0))
    {
        udp->addr.sin_addr.s_addr = htonl(INADDR_ANY);
        udp->addr.sin_port = htons(port);
    }

    /* remote end-point */
    else if ((hostname != (char *)0) && (port != 0))
    {

        /* dot notation address */
        addr.s_addr = (in_addr_t)0;
        if (inet_aton(hostname, &addr) == 0)
        { // not dot notation

            /* try to resolve hostname */
            struct hostent *hp = gethostbyname(hostname);
            if (hp == (struct hostent *)0)
            {
                perror("setupUdpSocket_t(): gethostbyname()");
                error = 1; /* none or badly formed remote hostname */
            }

            else
            {
                memcpy((void *)&addr.s_addr,
                       (void *)*hp->h_addr_list, sizeof(addr.s_addr));
            }
        }

        if (addr.s_addr != (in_addr_t)0)
        {
            udp->addr.sin_family = AF_INET;
            udp->addr.sin_addr.s_addr = addr.s_addr;
            udp->addr.sin_port = htons(port);
        }
        else
        {
            error = 1;
        }
    }

    else
    {
        error = 1;
    }

    if (error)
    {
        free(udp);
        udp = (UdpSocket_t *)0;
    }

    return udp;
}

int openUdp(UdpSocket_t *udp)
{
    /* open a UDP socket */
    if ((udp->sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        perror("openUdp(): socket()");
        return -1;
    }

    if (bind(udp->sd, (struct sockaddr *)&udp->addr, sizeof(udp->addr)) < 0)
    {
        perror("openUdp(): bind()");
        return -1;
    }

    return 0;
}

int synSend(const UdpSocket_t *local, const UdpSocket_t *remote)
{
    /* Make a new header for sending SYN */
    CrudpHeader_t *header = (CrudpHeader_t *)calloc(1, HEADER_SIZE);

    /* Sequence Number has to be a random number  */
    srand(time(NULL) + R_T);
    startSeq = (int)(rand() % (int)pow(2, 8 * 32));
    seqNumber = startSeq;
    header->sn = seqNumber;

    header->an = 0;

    /* Default window size is 1 */
    header->wn = 0;

    /* SYN Flag */
    header->syn = 1;

    header->ack = 0;
    header->eod = 0;
    header->fin = 0;

    /* Make a new buffer which will contain header in the start of array */
    unsigned char newBuffer[HEADER_SIZE];

    /* Copy the header as byte array */
    for (unsigned int i = 0; i < HEADER_SIZE; i++)
    {
        newBuffer[i] = *((char *)header + i);
    }

    CrudpBuffer_t toSend;

    toSend.n = HEADER_SIZE;
    toSend.bytes = newBuffer;

    int r = sendCrudp(local, remote, &toSend);

    free(header);

    return r;
};

int synRecv(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpHeader_t *recvHeader)
{
    /* Make a new header for sending SYN ACK*/
    CrudpHeader_t *header = (CrudpHeader_t *)calloc(1, HEADER_SIZE);

    /* Sequence Number has to be a random number  */
    srand(time(NULL) + R_R);
    startSeq = (int)(rand() % (int)pow(2, 8 * 32));
    seqNumber = startSeq;
    header->sn = seqNumber;

    /* Sequence number from the transmitter has to be increment */
    ackNumber = recvHeader->sn;
    header->an = ++ackNumber;

    /* Default window size is 1 */
    header->wn = 0;

    /* SYN Flag */
    header->syn = 1;

    /* ACK Flag */
    header->ack = 1;
    header->eod = 0;
    header->fin = 0;

    /* Make a new buffer which will contain header in the start of array */
    unsigned char newBuffer[HEADER_SIZE];

    /* Copy the header as byte array */
    for (unsigned int i = 0; i < HEADER_SIZE; i++)
    {
        newBuffer[i] = *((char *)header + i);
    }

    CrudpBuffer_t toSend;

    toSend.n = HEADER_SIZE;
    toSend.bytes = newBuffer;

    int r = sendCrudp(local, remote, &toSend);

    seqNumber++;
    free(header);

    return r;
};

void ackChecker(const CrudpHeader_t *recvHeader)
{
    if (recvHeader->an != seqNumber)
    {
        printf("\033[1;31m");

        printf("** WRONG ACKNOWLEDGEMENT VALUE **\n");
        printf("Diff: \t%d\t%d\n", recvHeader->an, seqNumber);

        printf("\033[0;35m");
        seqNumber = recvHeader->an;
        printf("Sequence Number changed to: %d\n", seqNumber);
        printf("\033[0m");
    }
};

int estWait(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpHeader_t *recvHeader)
{
    /* Make a new header for sending SYN ACK*/
    CrudpHeader_t *header = (CrudpHeader_t *)calloc(1, HEADER_SIZE);

    ++seqNumber;

    /* Sequence number from the transmitter has to be increment */
    ackChecker(recvHeader);

    header->sn = recvHeader->an;

    ackNumber = recvHeader->sn;

    ackNumber++;
    header->an = ackNumber;

    /* Default window size is 1000 */
    header->wn = windowSize;
    header->syn = 0;

    /* ACK Flag */
    header->ack = 1;
    header->eod = recvHeader->eod;
    header->fin = 0;

    /* Make a new buffer which will contain header in the start of array */
    unsigned char newBuffer[HEADER_SIZE];

    /* Copy the header as byte array */
    for (unsigned int i = 0; i < HEADER_SIZE; i++)
    {
        newBuffer[i] = *((char *)header + i);
    }

    CrudpBuffer_t toSend;

    toSend.n = HEADER_SIZE;
    toSend.bytes = newBuffer;

    int r = sendCrudp(local, remote, &toSend);

    free(header);

    return r;
};

int sendData(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpHeader_t *recvHeader, const unsigned char *data, int eod)
{
    /* Make a new header for sending SYN ACK*/
    CrudpHeader_t *header = (CrudpHeader_t *)calloc(1, HEADER_SIZE);

    /* Sequence number from the transmitter has to be increment */
    ackChecker(recvHeader);

    header->sn = recvHeader->an;

    // ackNumber = recvHeader->sn;
    header->an = recvHeader->sn;
    /* Default window size is 1 */
    header->wn = recvHeader->wn;
    header->syn = 0;

    /* ACK Flag */
    header->ack = 1;
    header->eod = eod;
    header->fin = 0;

    /* Make a new buffer which will contain header in the start of array */
    unsigned int totalSize = HEADER_SIZE + header->wn;
    unsigned char newBuffer[totalSize];

    /* Copy the header as byte array */
    for (unsigned int i = 0; i < HEADER_SIZE; i++)
    {
        newBuffer[i] = *((char *)header + i);
    }

    for (unsigned int i = HEADER_SIZE, j = 0; i < totalSize; i++, j++)
    {
        newBuffer[i] = *((char *)data + j);
    }

    CrudpBuffer_t toSend;

    toSend.n = totalSize;
    toSend.bytes = newBuffer;

    int r = sendCrudp(local, remote, &toSend);

    seqNumber += recvHeader->wn;

    free(header);

    return r;
};

int recvData(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpHeader_t *recvHeader, int rto_incr)
{
    /* Make a new header for sending SYN ACK*/
    CrudpHeader_t *header = (CrudpHeader_t *)calloc(1, HEADER_SIZE);

    /* Sequence number from the transmitter has to be increment */
    ackChecker(recvHeader);

    header->sn = recvHeader->an;

    ackNumber = recvHeader->sn + recvHeader->wn;
    header->an = ackNumber;

    /* Default window size is 1 */
    header->wn = recvHeader->wn;
    if (rto_incr)
    {
        header->wn -= 100;
        if (header->wn < 10 || header->wn > 1388)
            header->wn = 10;
    }
    else
    {
        header->wn = header->wn << 1;
        if (header->wn >= 1388)
        {
            header->wn = 1388;
        }
    }
    header->syn = 0;

    /* ACK Flag */
    header->ack = 1;
    header->eod = recvHeader->eod;
    header->fin = 0;

    /* Make a new buffer which will contain header in the start of array */
    unsigned char newBuffer[HEADER_SIZE];

    /* Copy the header as byte array */
    for (unsigned int i = 0; i < HEADER_SIZE; i++)
    {
        newBuffer[i] = *((char *)header + i);
    }

    CrudpBuffer_t toSend;

    toSend.n = HEADER_SIZE;
    toSend.bytes = newBuffer;

    int r = sendCrudp(local, remote, &toSend);

    free(header);

    return r;
};

int sendFin(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpHeader_t *recvHeader)
{
    /* Make a new header for sending SYN ACK*/
    CrudpHeader_t *header = (CrudpHeader_t *)calloc(1, HEADER_SIZE);

    /* Sequence number from the transmitter has to be increment */
    ackChecker(recvHeader);

    header->sn = recvHeader->an;

    ackNumber = recvHeader->sn + recvHeader->wn;
    header->an = ++ackNumber;

    /* Default window size is 1 */
    header->wn = 0;
    header->syn = 0;

    /* ACK Flag */
    header->ack = 1;
    header->eod = recvHeader->eod;
    header->fin = 1;

    /* Make a new buffer which will contain header in the start of array */
    unsigned char newBuffer[HEADER_SIZE];

    /* Copy the header as byte array */
    for (unsigned int i = 0; i < HEADER_SIZE; i++)
    {
        newBuffer[i] = *((char *)header + i);
    }

    CrudpBuffer_t toSend;

    toSend.n = HEADER_SIZE;
    toSend.bytes = newBuffer;

    int r = sendCrudp(local, remote, &toSend);
    free(header);

    return r;
};

int sendCrudp(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpBuffer_t *buffer)
{
    int r = sendto(local->sd, (void *)buffer->bytes, buffer->n, 0,
                   (struct sockaddr *)&remote->addr, sizeof(remote->addr));

    if (r < 0)
    {
        printf("%d ", buffer->n);
        perror("sendCrudp(): sendto()");
    }

    return r;
};

int recvCrudp(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpBuffer_t *buffer)
{
    int r;
    socklen_t l = sizeof(struct sockaddr);
    r = recvfrom(local->sd, (void *)buffer->bytes, buffer->n, 0,
                 (struct sockaddr *)&remote->addr, &l);
    return r;
};

void closeUdp(UdpSocket_t *udp)
{
    if (udp->sd > 0)
    {
        (void)close(udp->sd);
    }
    udp->sd = 0;
}
