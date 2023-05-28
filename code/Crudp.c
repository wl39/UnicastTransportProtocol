#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>

#include "CrudpSocket.h"

#define G_MY_PORT ((uint16_t)23204) // use 'id -u'
#define G_SIZE ((uint32_t)65495)
#define G_ITIMER_S ((uint32_t)0)  // seconds
#define G_ITIMER_US ((uint32_t)200000) // microseconds
#define HEADER_SIZE ((uint32_t)12)

#define ERROR(_s) fprintf(stderr, "%s\n", _s)

// CRUDP_INVALID is not part of RFC793(S), just for this FSM emulation.
#define CRUDP_INVALID ((int)0)

// CRUDP inputs from CRUDP user, e.g. via API.
#define CRUDP_INPUT_ACTIVE_OPEN ((int)1)
#define CRUDP_INPUT_PASSIVE_OPEN ((int)2)
#define CRUDP_INPUT_CLOSE ((int)3)
#define CRUDP_INPUT_SEND ((int)4)

// CRUDP actions
#define CRUDP_ACTION_SND_SYN ((int)5)
#define CRUDP_ACTION_SND_ACK ((int)6)
#define CRUDP_ACTION_SND_SYN_ACK ((int)7)
#define CRUDP_ACTION_SND_FIN ((int)8)
#define CRUDP_ACTION_OPEN_SOCKET ((int)9)
#define CRUDP_ACTION_CLOSE_SOCKET ((int)10)

// CRUDP events
#define CRUDP_EVENT_RCV_SYN ((int)11)
#define CRUDP_EVENT_RCV_ACK_OF_SYN ((int)12)
#define CRUDP_EVENT_RCV_SYN_ACK ((int)13)
#define CRUDP_EVENT_RCV_FIN ((int)14)
#define CRUDP_EVENT_RCV_ACK_OF_FIN ((int)15)
#define CRUDP_EVENT_CLOSE_SOCKET ((int)16)

// CRUDP states
#define CRUDP_STATE_CLOSED ((int)17)
#define CRUDP_STATE_LISTEN ((int)18)
#define CRUDP_STATE_SYN_SENT ((int)19)
#define CRUDP_STATE_SYN_RCVD ((int)20)

#define CRUDP_STATE_ESTABLISHED ((int)21)

#define CRUDP_STATE_FINWAIT_1 ((int)22)
#define CRUDP_STATE_FINWAIT_2 ((int)23)
#define CRUDP_STATE_CLOSING ((int)24)
#define CRUDP_STATE_TIME_WAIT ((int)25)
#define CRUDP_STATE_CLOSE_WAIT ((int)26)
#define CRUDP_STATE_LAST_ACK ((int)27)

#define CRUDP_SEND_DATA ((int)28)
#define CRUDP_RECV_DATA ((int)29)

// This array of strings is indexed by integer values in #define
// values above, so the order of the list is important!
const char *CRUDP_fsm_strings_G[] = {
    "--",
    "active OPEN",
    "passive OPEN",
    "CLOSE",
    "SEND",
    "snd SYN",
    "snd ACK",
    "snd SYN,ACK",
    "snd FIN",
    "open socket",
    "close socket",
    "rcv SYN",
    "rcv ACK of SYN",
    "rcv SYN,ACK",
    "rcv FIN",
    "rcv ACK of FIN",
    "Timeout=2MSL",
    "CLOSED",
    "LISTEN",
    "SYN_SENT",
    "SYN_RCVD",
    "ESTBALISHED",
    "FINWAIT_1",
    "FINWAIT_2",
    "CLOSING",
    "TIME_WAIT",
    "CLOSE_WAIT",
    "LAST_ACK",
    "SEND_DATA",
    "RECV_DATA",
};

int w,
    startW,
    tcp_state,
    tcp_new_state, // after the state change

    // The CRUDP FSM only ever has 2 inputs, events, or actions
    // possible in each state, so the following is convenient.
    inputs[3],  // possible input from CRUDP user
    events[3],  // possible network or timer events
    actions[3], // actions to be taken during state change

    // Input for what to do next in the emulation.
    what; // what to do next
// Get and check input from terminal

int G_net, G_flag;

int established, transmitter, receiver, received, rto_incr = 0;

sigset_t G_sigmask;

struct sigaction G_sigio, G_sigalrm;

struct timespec sendTime, endTime;
double gSnedTime, gEndTime;

UdpSocket_t *G_local;
UdpSocket_t *G_remote;

char *filename,
    *remote,
    *fileBuffer;

// For RTO
long filelen, vn, sn, tn, rn;
unsigned long urto, rto = 0;
uint32_t srto = 1;

// File read index
long currentIndex = 0;

struct itimerval G_timer;

unsigned char bytes[G_SIZE];
int r;

void stateHandler(CrudpHeader_t *header);
CrudpHeader_t *headerHandler();

int checkInputsAndEvents(int tcp_state, int *inputs, int *events);

void makeSocket(char *remote);

FILE *fileToSave;
/*
  i/o functions
*/

void setupSIGIO();
void handleSIGIO(int sig);
void checkNetwork();
int setAsyncFd(int fd);

/*
  alarm functions
*/
void handleSIGALRM(int sig);
void setupSIGALRM();
void setITIMER(uint32_t sec, uint32_t usec);

void readFile();

void runActions(char *remote, CrudpHeader_t *header);

void setupTransfer();
void setRTO();
struct timespec getTime();

void green();
void yellow();
void reset();

#define CHECK_INPUTS_AND_EVENTS \
    what = checkInputsAndEvents(tcp_state, inputs, events);

int main(int argc, char *argv[])
{
    if (argc > 2 && argc < 5)
    {
        if ((!strcmp(argv[2], "-r") && argc != 3) || (!strcmp(argv[2], "-t") && argc != 4))
        {
            ERROR("usage: test <hostname> -t|-r [File name \"for -t\"]");

            exit(0);
        }
    }

        gSnedTime = (double) clock() / CLOCKS_PER_SEC;

    remote = argv[1];
    w = startW = strcmp("-r", argv[2]) == 0 ? CRUDP_INPUT_ACTIVE_OPEN : CRUDP_INPUT_PASSIVE_OPEN;
    filename = argv[3];

    tcp_state = CRUDP_STATE_CLOSED;

    while (!(transmitter || receiver))
    {
        stateHandler(NULL);
    }

    G_net = G_local->sd;

    sigemptyset(&G_sigmask);

    setupSIGIO();

    G_flag = 0;
    while (!G_flag)
    {
        checkNetwork();
        // (void)pause(); // wait for signal, otherwise do nothing
    }

    closeUdp(G_local);
    closeUdp(G_remote);

    return 0;
}

/**
 * @brief Put actions based on the state
 *
 * @param header Received Header
 */
void stateHandler(CrudpHeader_t *header)
{
    tcp_new_state = what = CRUDP_INVALID;
    inputs[0] = inputs[1] = inputs[2] = CRUDP_INVALID;
    events[0] = events[1] = events[2] = CRUDP_INVALID;
    actions[0] = actions[1] = actions[2] = CRUDP_INVALID;

    switch (tcp_state)
    {
        // Initial State
    case CRUDP_STATE_CLOSED:
        inputs[0] = CRUDP_INPUT_ACTIVE_OPEN;  // from CRUDP user
        inputs[1] = CRUDP_INPUT_PASSIVE_OPEN; // from CRUDP user

        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
            // Active open for receiver
        case CRUDP_INPUT_ACTIVE_OPEN:
            receiver = 1;

            actions[0] = CRUDP_ACTION_OPEN_SOCKET; // local CRUDP
            actions[1] = CRUDP_ACTION_SND_SYN;     // local CRUDP
            tcp_new_state = CRUDP_STATE_SYN_SENT;
            break;
            // Passive open for transmitter
        case CRUDP_INPUT_PASSIVE_OPEN:
            transmitter = 1;
            actions[0] = CRUDP_ACTION_OPEN_SOCKET; // local CRUDP
            tcp_new_state = CRUDP_STATE_LISTEN;
            break;
        }
        break;
        // LISTEN - Waiting for SYN
    case CRUDP_STATE_LISTEN:
        inputs[0] = CRUDP_INPUT_SEND;    // from CRUDP user
        inputs[1] = CRUDP_INPUT_CLOSE;   // from CRUDP user
        events[0] = CRUDP_EVENT_RCV_SYN; // from network

        w = CRUDP_EVENT_RCV_SYN;

        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
        case CRUDP_INPUT_SEND:
            actions[0] = CRUDP_ACTION_SND_SYN; // local CRUDP
            tcp_new_state = CRUDP_STATE_SYN_SENT;
            break;

        case CRUDP_INPUT_CLOSE:
            actions[0] = CRUDP_ACTION_CLOSE_SOCKET; // local CRUDP
            actions[1] = CRUDP_INPUT_CLOSE;         // local CRUDP
            tcp_new_state = CRUDP_STATE_CLOSED;
            break;

        case CRUDP_EVENT_RCV_SYN:
            actions[0] = CRUDP_ACTION_SND_SYN_ACK; // local CRUDP
            tcp_new_state = CRUDP_STATE_SYN_RCVD;
            setupSIGALRM();
            break;
        }
        break;

    // SYN SENT
    case CRUDP_STATE_SYN_SENT:
        inputs[0] = CRUDP_INPUT_CLOSE;       // from CRUDP user
        events[0] = CRUDP_EVENT_RCV_SYN;     // from network
        events[1] = CRUDP_EVENT_RCV_SYN_ACK; // from network

        w = CRUDP_EVENT_RCV_SYN_ACK;
        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
        case CRUDP_INPUT_CLOSE:
            actions[0] = CRUDP_ACTION_CLOSE_SOCKET; // local CRUDP
            actions[1] = CRUDP_INPUT_CLOSE;         // local CRUDP
            tcp_new_state = CRUDP_STATE_CLOSED;
            break;

        case CRUDP_EVENT_RCV_SYN:
            actions[0] = CRUDP_ACTION_SND_ACK; // local CRUDP
            tcp_new_state = CRUDP_STATE_SYN_RCVD;
            break;

        case CRUDP_EVENT_RCV_SYN_ACK:
            actions[0] = CRUDP_ACTION_SND_ACK; // local CRUDP
            tcp_new_state = CRUDP_STATE_ESTABLISHED;
            break;
        }
        break;

    // SYN RCVD
    case CRUDP_STATE_SYN_RCVD:
    {
        inputs[0] = CRUDP_INPUT_CLOSE;          // from CRUDP user
        events[0] = CRUDP_EVENT_RCV_ACK_OF_SYN; // from network

        w = CRUDP_EVENT_RCV_ACK_OF_SYN;
        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
        case CRUDP_INPUT_CLOSE:
            actions[0] = CRUDP_ACTION_SND_FIN; // local CRUDP
            actions[1] = CRUDP_INPUT_CLOSE;    // local CRUDP
            tcp_new_state = CRUDP_STATE_FINWAIT_1;
            break;

        case CRUDP_EVENT_RCV_ACK_OF_SYN:
         gSnedTime = (double) clock() / CLOCKS_PER_SEC;
            setupTransfer();
            actions[0] = CRUDP_SEND_DATA;
            tcp_new_state = CRUDP_STATE_ESTABLISHED;
            break;
        }
        break;
    }
    // ESTABLISHED
    case CRUDP_STATE_ESTABLISHED:
    {
        inputs[0] = CRUDP_SEND_DATA;
        inputs[1] = CRUDP_INPUT_CLOSE;
        events[0] = CRUDP_RECV_DATA;
        events[1] = CRUDP_EVENT_RCV_FIN;

        // Transmitter: Send data until it receives FIN flag
        w = transmitter ? (header->fin ? CRUDP_EVENT_RCV_FIN : CRUDP_SEND_DATA)
                        // Receiver: Receive data until it receives EOD flag
                        : (header->eod ? CRUDP_INPUT_CLOSE : CRUDP_RECV_DATA);
        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
        case CRUDP_SEND_DATA:
            actions[0] = CRUDP_SEND_DATA;
            tcp_new_state = CRUDP_STATE_ESTABLISHED;
            break;

        case CRUDP_RECV_DATA:
            actions[0] = CRUDP_RECV_DATA;
            tcp_new_state = CRUDP_STATE_ESTABLISHED;
            break;
        case CRUDP_INPUT_CLOSE:
            actions[0] = CRUDP_ACTION_SND_FIN; // local CRUDP
            tcp_new_state = CRUDP_STATE_FINWAIT_1;
            break;

        case CRUDP_EVENT_RCV_FIN:
            actions[0] = CRUDP_ACTION_SND_ACK; // local CRUDP
            tcp_new_state = CRUDP_STATE_CLOSE_WAIT;
            break;
        }
    }
    break;
    // CLOSE WAIT
    case CRUDP_STATE_CLOSE_WAIT:
    {

        inputs[0] = CRUDP_INPUT_CLOSE; // local CRUDP
        w = CRUDP_INPUT_CLOSE;
        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
        case CRUDP_INPUT_CLOSE:
            actions[0] = CRUDP_ACTION_SND_FIN; // local CRUDP
            tcp_new_state = CRUDP_STATE_LAST_ACK;
            break;
        }
    }
    break;
    // LAST ACK
    case CRUDP_STATE_LAST_ACK:
        events[0] = CRUDP_EVENT_RCV_ACK_OF_FIN; // from network
        w = CRUDP_EVENT_RCV_ACK_OF_FIN;

        CHECK_INPUTS_AND_EVENTS;
        switch (what)
        {
        case CRUDP_EVENT_RCV_ACK_OF_FIN:
            actions[0] = CRUDP_ACTION_CLOSE_SOCKET;
            tcp_new_state = CRUDP_STATE_CLOSED;
            break;
        }
        break;
        // FINWAIT 1
    case CRUDP_STATE_FINWAIT_1:
        events[0] = CRUDP_EVENT_RCV_FIN;        // from network
        events[1] = CRUDP_EVENT_RCV_ACK_OF_FIN; // from network
        w = CRUDP_EVENT_RCV_ACK_OF_FIN;
        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
        case CRUDP_EVENT_RCV_FIN:
            actions[0] = CRUDP_ACTION_SND_ACK; // local CRUDP
            tcp_new_state = CRUDP_STATE_CLOSING;
            break;

        case CRUDP_EVENT_RCV_ACK_OF_FIN:
            tcp_new_state = CRUDP_STATE_FINWAIT_2;
            break;
        }
        break;
        // FINWAIT 2
    case CRUDP_STATE_FINWAIT_2:
        events[0] = CRUDP_EVENT_RCV_FIN; // from network
        w = CRUDP_EVENT_RCV_FIN;
        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
        case CRUDP_EVENT_RCV_FIN:
            actions[0] = CRUDP_ACTION_SND_ACK; // local CRUDP
            tcp_new_state = CRUDP_STATE_TIME_WAIT;
            break;
        }
        break;
        // TIME WAIT
    case CRUDP_STATE_TIME_WAIT:
        events[0] = CRUDP_EVENT_CLOSE_SOCKET; // local timer
        w = CRUDP_EVENT_CLOSE_SOCKET;
        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
        case CRUDP_EVENT_CLOSE_SOCKET:
            // CLOSE SOCKET
            actions[0] = CRUDP_ACTION_CLOSE_SOCKET; // local CRUDP
            tcp_new_state = CRUDP_STATE_CLOSED;
            break;
        }
        break;
    default:
        break;
    }

    runActions(remote, header);
}

/**
 * @brief Parse Header from byte streams
 *
 * @return CrudpHeader_t* parsed header struct
 */
CrudpHeader_t *headerHandler()
{
    unsigned char *headerByte = (unsigned char *)calloc(1, HEADER_SIZE * sizeof(char));

    for (int i = 0; i < HEADER_SIZE; i++)
    {
        headerByte[i] = *((char *)bytes + i);
    }

    return (CrudpHeader_t *)headerByte;
}

/**
 * @brief Check inputs and events and choose one of them
 *        based on 'w'
 *
 * @param tcp_state current tcp state
 * @param inputs inputs array
 * @param events events array
 * @return int selected inputs/events
 */
int checkInputsAndEvents(int tcp_state, int *inputs, int *events)
{
    int what;

    printf("\n** Current state: %s\n", CRUDP_fsm_strings_G[tcp_state]);

    what = CRUDP_INVALID;

    // Current possibilities for inputs or actions
    printf("   Possible inputs and events:\n");
    for (int *ip = inputs; *ip != CRUDP_INVALID; ++ip)
        printf("     %2d : %s\n", *ip, CRUDP_fsm_strings_G[*ip]);
    for (int *ep = events; *ep != CRUDP_INVALID; ++ep)
        printf("     %2d : %s\n", *ep, CRUDP_fsm_strings_G[*ep]);

    // Check validity of input
    for (int *ip = inputs; *ip != CRUDP_INVALID; ++ip)
        if (w == *ip)
        {
            what = *ip;
            break;
        }
    if (what == CRUDP_INVALID)
        for (int *ep = events; *ep != CRUDP_INVALID; ++ep)
            if (w == *ep)
            {
                what = *ep;
                break;
            }
    if (what == CRUDP_INVALID)
        printf("\n** Please select a number from the list.\n");

    green();
    printf("      S : %s\n\n", CRUDP_fsm_strings_G[w]);
    reset();

    return what;
}

/**
 * @brief Open socket for receiver based on FQDN
 *
 * @param remote FQDN of receiver
 */
void makeSocket(char *remote)
{
    if ((G_local = setupUdpSocket_t((char *)0, G_MY_PORT)) == (UdpSocket_t *)0)
    {
        ERROR("local problem");
        exit(0);
    }

    if ((G_remote = setupUdpSocket_t(remote, G_MY_PORT)) == (UdpSocket_t *)0)
    {
        ERROR("remote hostname/port problem");
        exit(0);
    }

    if (openUdp(G_local) < 0)
    {
        ERROR("openUdp() problem");
        exit(0);
    };
}

void handleSIGIO(int sig)
{
    if (sig == SIGIO)
    {
        /* protect the network and keyboard reads from signals */
        sigprocmask(SIG_BLOCK, &G_sigmask, (sigset_t *)0);

        /* allow the signals to be delivered */
        sigprocmask(SIG_UNBLOCK, &G_sigmask, (sigset_t *)0);
    }
    else
    {
        ERROR("handleSIGIO(): got a bad signal number");
    }
}

void setupSIGIO()
{
    sigaddset(&G_sigmask, SIGIO);

    G_sigio.sa_handler = handleSIGIO;
    G_sigio.sa_flags = 0;

    if (sigaction(SIGIO, &G_sigio, (struct sigaction *)0) < 0)
    {
        perror("setupSIGIO(): sigaction() problem");
        exit(0);
    }

    if (setAsyncFd(G_net) < 0)
    {
        ERROR("setupSIGIO(): setAsyncFd(G_net) problem");
        exit(0);
    }
}

/**
 * @brief Read data from current socket
 *
 */
void checkNetwork()
{
    UdpSocket_t receive;
    CrudpBuffer_t buffer;

    /* print any network input */
    buffer.bytes = bytes;
    buffer.n = G_SIZE;

    if ((r = recvCrudp(G_local, &receive, &buffer)) < 0)
    {
        if (errno != EWOULDBLOCK)
        {
            ERROR("checkNetwork(): recvUdp() problem");
            exit(1);
        }
    }
    else
    {
        endTime = getTime();

        // Calculate rto
        rto = urto + srto * 1000000;

        if (tcp_state != CRUDP_STATE_LISTEN)
        {
            setRTO();
            urto /= 1000;

            if (urto >= 1000000)
            {
                srto = urto / 1000000;
                urto -= srto * 1000000;
            }
        }

        // is current rto greater than previous rto
        rto_incr = (urto + srto * 1000000) > rto;

        CrudpHeader_t *header = headerHandler();
        stateHandler(header);

        setITIMER(srto, urto);
    }
}

int setAsyncFd(int fd)
{
    int r, flags = O_NONBLOCK | O_ASYNC; // man 2 fcntl

    if (fcntl(fd, F_SETOWN, getpid()) < 0)
    {
        perror("setAsyncFd(): fcntl(fd, F_SETOWN, getpid()");
        exit(0);
    }

    if ((r = fcntl(fd, F_SETFL, flags)) < 0)
    {
        perror("setAsyncFd(): fcntl() problem");
        exit(0);
    }

    return r;
}

void setITIMER(uint32_t sec, uint32_t usec)
{
    G_timer.it_interval.tv_sec = sec;
    G_timer.it_interval.tv_usec = usec;
    G_timer.it_value.tv_sec = sec;
    G_timer.it_value.tv_usec = usec;

    if (setitimer(ITIMER_REAL, &G_timer, (struct itimerval *)0) != 0)
    {
        perror("setitimer");
        ERROR("setITIMER(): setitimer() problem");
    }
}

void handleSIGALRM(int sig)
{
    if (sig == SIGALRM)
    {
        /* protect handler actions from signals */
        sigprocmask(SIG_BLOCK, &G_sigmask, (sigset_t *)0);

        // Retransmit the data
        if (transmitter && established)
        {

            CrudpHeader_t *header = headerHandler();
            stateHandler(header);

            return;
        }

        // // restart connection from the bottom
        // switch (tcp_state)
        // {
        // case CRUDP_STATE_ESTABLISHED:
        //     if (actions[0] == CRUDP_ACTION_SND_ACK)
        //     {
        //         if (receiver)
        //         {
        //             srto = 2;
        //         }
        //         else
        //         {
        //             srto = 1;
        //         }
        //         receiver = transmitter = 0;
        //         w = startW;
        //         tcp_state = CRUDP_STATE_CLOSED;
        //         closeUdp(G_local);
        //         closeUdp(G_remote);
        //         stateHandler(NULL);
        //         break;
        //     }
        // case CRUDP_STATE_SYN_SENT:
        // case CRUDP_STATE_LISTEN:
        // case CRUDP_STATE_SYN_RCVD:
        //     if (receiver)
        //     {
        //         srto = 2;
        //     }
        //     else
        //     {
        //         srto = 1;
        //     }
        //     receiver = transmitter = 0;
        //     w = startW;
        //     tcp_state = CRUDP_STATE_CLOSED;
        //     closeUdp(G_local);
        //     closeUdp(G_remote);
        //     stateHandler(NULL);
        //     break;
        // default:
        //     // Did not implement 4 way handshake
        //     printf("ERROR\n");
        //     exit(0);
        //     break;
        // }

        /* protect handler actions from signals */
        sigprocmask(SIG_UNBLOCK, &G_sigmask, (sigset_t *)0);
    }
    else
    {
        ERROR("handleSIGALRM() got a bad signal");
    }
}

void setupSIGALRM()
{
    sigaddset(&G_sigmask, SIGALRM);

    G_sigalrm.sa_handler = handleSIGALRM;
    G_sigalrm.sa_flags = 0;

    if (sigaction(SIGALRM, &G_sigalrm, (struct sigaction *)0) < 0)
    {
        perror("setupSIGALRM(): sigaction() problem");
        exit(0);
    }
    else
    {
        setITIMER(srto, (uint32_t)urto);
    }
}

void readFile()
{
    FILE *file;

    file = fopen(filename, "rb");
    if (!file)
    {
        printf("File path/name is wrong\n");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    filelen = ftell(file);
    rewind(file);

    fileBuffer = (char *)malloc(filelen * sizeof(char));
    fread(fileBuffer, filelen, 1, file);
    fclose(file);
}

void makeFile()
{
    fileToSave = fopen("../save/download.txt", "w");

    if (fileToSave == NULL)
    {
        printf("File Generate Fail...\n");
        exit(1);
    }
    else
    {
        printf("File Generate Done\n");
    }
}

void writeFile(char *text)
{
    fputs(text, fileToSave);
}

/**
 * @brief Do action based on selected value with current state
 *
 * @param remote Receiver's FQDN
 * @param header received header
 */
void runActions(char *remote, CrudpHeader_t *header)
{
    printf("** New state: %s\n", CRUDP_fsm_strings_G[tcp_new_state]);
    printf("   Actions executed by CRUDP:\n");
    for (int *ap = actions; *ap != CRUDP_INVALID; ++ap)
    {
        printf("     %d : %s\n", *ap, CRUDP_fsm_strings_G[*ap]);
        switch (*ap)
        {
        case CRUDP_ACTION_OPEN_SOCKET:
            makeSocket(remote);
            green();
            printf("     O : %s Completed\n", CRUDP_fsm_strings_G[*ap]);
            reset();
            /* code */
            break;
        case CRUDP_ACTION_SND_SYN:
            sendTime = getTime();
            synSend(G_local, G_remote);
            green();
            printf("     O : %s Completed\n", CRUDP_fsm_strings_G[*ap]);
            reset();

            makeFile();
            setupSIGALRM();
            break;
        case CRUDP_ACTION_SND_SYN_ACK:
        {
            sendTime = getTime();
            synRecv(G_local, G_remote, header);
            green();
            printf("     O : %s Completed\n", CRUDP_fsm_strings_G[*ap]);
            reset();

            free(header);
        }
        break;
        case CRUDP_ACTION_SND_ACK:
        {
            sendTime = getTime();
            estWait(G_local, G_remote, header);
            green();
            printf("     O : %s Completed\n", CRUDP_fsm_strings_G[*ap]);
            reset();

            established = header->fin ? 0 : 1;

            if (header->fin)
            {
                tcp_state = tcp_new_state;
                stateHandler(header);

                return;
            }
            free(header);
        }
        break;
        case CRUDP_SEND_DATA:
        {

            extern uint32_t startSeq;

            currentIndex = (header->an % (startSeq + 1));

            int windowSize = header->wn;
            windowSize ? windowSize : windowSize++;

            unsigned char *dataToSend = (unsigned char *)calloc(1, windowSize);

            /* Send the file */

            for (unsigned int i = 0; (currentIndex < filelen && i < windowSize); i++, currentIndex++)
            {
                dataToSend[i] = *((char *)fileBuffer + currentIndex);
            }

            sendTime = getTime();
            int r = sendData(G_local, G_remote, header, dataToSend, currentIndex >= filelen);

            green();
            printf("     O : %s Completed\n", CRUDP_fsm_strings_G[*ap]);
            printf("** Send Total: %d bytes\n   Send Data: %d\n   Data: %s\n", r, r - HEADER_SIZE, dataToSend);
            reset();

            free(header);
        }
        break;
        case CRUDP_RECV_DATA:
        {
            extern uint32_t ackNumber;

            unsigned int dataSize = r - HEADER_SIZE;
            unsigned char *recvedData = (unsigned char *)calloc(1, dataSize + 1); // this code is for testing
            /* Receive the file */

            for (unsigned int i = HEADER_SIZE, j = 0; i < r; i++, j++)
            {
                recvedData[j] = *((char *)bytes + i);
            }

            recvedData[dataSize] = '\0';

            if (header->sn == ackNumber)
            {
                green();
                printf("** Recv Total: %d bytes\n   Recv Data: %d bytes\n   Data: %s\n", r, dataSize, recvedData);
                writeFile((char *)recvedData);

                printf("     O : %s Completed\n", CRUDP_fsm_strings_G[*ap]);
                reset();
            }
            else
            {
                yellow();
                printf("** Recv Total: %d bytes\n   Recv Data: %d bytes\n   Data: %s - Abandoned due to duplication\n", r, dataSize, recvedData);
                reset();
            }

            free(recvedData);

            sendTime = getTime();
            recvData(G_local, G_remote, header, rto_incr);

            free(header);
        }
        break;
        case CRUDP_ACTION_SND_FIN:
        { // Read last data
            extern uint32_t ackNumber;
            unsigned int dataSize = r - HEADER_SIZE;
            unsigned char *recvedData = (unsigned char *)calloc(1, dataSize + 1); // this code is for testing
            /* Receive the file */

            for (unsigned int i = HEADER_SIZE, j = 0; i < r; i++, j++)
            {
                recvedData[j] = *((char *)bytes + i);
            }

            recvedData[dataSize] = '\0';

            if (receiver)
            {
                if (header->sn == ackNumber)
                {
                    green();
                    printf("** Recv Total: %d bytes\n   Recv Data: %d bytes\n   Data: %s\n", r, dataSize, recvedData);
                    writeFile((char *)recvedData);

                    printf("     O : %s Completed\n", CRUDP_fsm_strings_G[*ap]);
                    reset();
                }
                else
                {
                    yellow();
                    printf("** Recv Total: %d bytes\n   Recv Data: %d bytes\n   Data: %s - Abandoned due to duplication\n", r, dataSize, recvedData);
                    reset();
                }
            }

            if (transmitter)
            {
                green();
                printf("     O : %s Completed\n", CRUDP_fsm_strings_G[*ap]);
                reset();
                free(recvedData);
                free(fileBuffer);
            }

            // send fin
            sendTime = getTime();
            sendFin(G_local, G_remote, header);

            if (receiver)
                fclose(fileToSave);

            established = 0;
            free(header);
        }
        break;
        case CRUDP_ACTION_CLOSE_SOCKET:
            gEndTime = (double) clock()  / CLOCKS_PER_SEC;
            closeUdp(G_local);
            closeUdp(G_remote);
            green();
            printf("     O : %s Completed\n", CRUDP_fsm_strings_G[*ap]);
            reset();
            
            printf("\n\n%lf\n\n", gEndTime -gSnedTime);
            exit(0);
        default:
            printf("Did not set the state!\n");
            break;
        }
    }
    printf("\n");

    tcp_state = tcp_new_state;
}

/**
 * @brief Read file and set established flag
 *
 */
void setupTransfer()
{
    printf("** New state: %s\n", CRUDP_fsm_strings_G[tcp_new_state]);
    printf("   Actions executed by CRUDP:\n");
    printf("     R : Read file - %s\n", filename);
    if (transmitter)
        readFile();
    green();
    printf("     O : Read file Completed - %s | %ld bytes \n\n", filename, filelen);
    reset();
    established = 1;
    tcp_state = tcp_new_state;
}

/**
 * @brief Get the Time object
 *
 * @return struct timespec
 */
struct timespec getTime()
{
    struct timespec t;

    if (clock_gettime(CLOCK_REALTIME, &t) == 0)
    {
        return t;
    }

    printf("clock_gettime error");

    return t;
}

/**
 * @brief set RTO
 *
 */
void setRTO()
{
    if (!vn)
    {
        vn = labs(endTime.tv_sec - sendTime.tv_sec) * 1000000000;
        vn += labs(endTime.tv_nsec - sendTime.tv_nsec) / 2;
        rn = vn * 2;
        sn = vn;
        tn = sn + 4 * vn;
    }

    else
    {
        long newRtt = labs(endTime.tv_sec - sendTime.tv_sec) * 1000000000;
        newRtt += labs(endTime.tv_nsec - sendTime.tv_nsec);

        vn = 0.75f * vn + 0.25f * labs(sn - newRtt);
        sn = 0.875f * sn + 0.125f * newRtt;
        tn = sn + 4 * vn;
    }

    urto = tn;
}

void green()
{
    printf("\033[0;32m");
}

void yellow()
{
    printf("\033[0;33m");
}

void reset()
{
    printf("\033[0m");
}
