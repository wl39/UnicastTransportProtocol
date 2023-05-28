#ifndef __CrudpSocket_h__
#define __CrudpSocket_h__

#include <inttypes.h>
#include <netinet/in.h>

typedef struct UdpSocket_s
{
    int sd;
    struct sockaddr_in addr;
} UdpSocket_t;

/**
 * @brief CRUDPHeader structure
 * 
 */
typedef struct CrudpHeader_s
{
    uint32_t sn : 32; //sequence number
    uint32_t an : 32; //acknowledgement number
    uint16_t wn : 16; //window size

    //1 bit flag
    unsigned int syn : 1; //SYN
    unsigned int ack : 1; //ACK
    unsigned int eod : 1; //EOD (Flag up when there is no data to send)
    unsigned int fin : 1; //FIN
} CrudpHeader_t;

typedef struct CrudpBuffer_s
{
    uint32_t n;
    uint8_t *bytes;
} CrudpBuffer_t;

typedef struct UdpBuffer_s
{
    uint32_t n;
    uint8_t *bytes;
} UdpBuffer_t;

// Setup Udp Socket
UdpSocket_t *
setupUdpSocket_t(const char *hostname, const uint16_t port);

/**
 * @brief Open Udp socket
 * 
 * @param udp Socket to open
 * @return positive int if socket is opended otherwise return -1
 */
int openUdp(UdpSocket_t *udp);

/**
 * @brief Send SYN Packet
 * 
 * @param local Transmitter socket
 * @param remote Receiver socket
 * @return int total size of data sent
 */
int synSend(const UdpSocket_t *local, const UdpSocket_t *remote);

/**
 * @brief Receive SYN ACK Packet
 * 
 * @param local Transmitter socket
 * @param remote Receiver socket
 * @param recvHeader Received haeder
 * @return int total size of data sent
 */
int synRecv(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpHeader_t *recvHeader);

/**
 * @brief Wait for Established (Send ACK)
 * 
 * @param local Transmitter socket
 * @param remote Receiver socket
 * @param recvHeader Received header
 * @return int total size of data sent
 */
int estWait(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpHeader_t *recvHeader);

/**
 * @brief Check acknowledgement
 * 
 * @param recvHeader Received header
 */
void ackChecker(const CrudpHeader_t *recvHeader);

/**
 * @brief Send data from loacl to remote
 *        use when the connection established.
 * @param local Transmitter socket
 * @param remote Receiver socket
 * @param recvHeader Received header
 * @param data Data to send
 * @param eod End of data flag
 * @return int total size of data sent
 */
int sendData(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpHeader_t *recvHeader, const unsigned char *data, int eod);

/**
 * @brief Receive data from remote socket
 * 
 * @param local Transmitter socket
 * @param remote Receiver socket
 * @param recvHeader Received header
 * @param rto_incr flag for rto increased
 *                  if the value is greater than 0
 *                  RTO vlaue is increased
 * @return int total size of data sent
 */
int recvData(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpHeader_t *recvHeader, int rto_incr);

/**
 * @brief Send FIN packet
 * 
 * @param local Transmitter socket
 * @param remote Receiver socket
 * @param recvHeader Received header
 * @return int total size of data sent
 */
int sendFin(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpHeader_t *recvHeader);

/**
 * @brief Send UDP Packet
 * 
 * @param local Transmitter socket
 * @param remote Receiver socket
 * @param buffer to send
 * @return int total size of data sent 
 */
int sendCrudp(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpBuffer_t *buffer);

/**
 * @brief Receive UDP Packet
 * 
 * @param local Transmitter socket
 * @param remote Receiver socket
 * @param buffer data will be saved in the buffer
 * @return int total size of data sent
 */
int recvCrudp(const UdpSocket_t *local, const UdpSocket_t *remote, const CrudpBuffer_t *buffer);

// Close Udp Socket
void closeUdp(UdpSocket_t *udp);

#endif