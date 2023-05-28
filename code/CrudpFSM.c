#include "CrudpFSM.h"

// //// ////
// TCP_INVALID is not part of RFC793(S), just for this FSM emulation.
#define TCP_INVALID ((int)0)

// //// ////
// TCP inputs from TCP user, e.g. via API.
#define TCP_INPUT_ACTIVE_OPEN ((int)1)
#define TCP_INPUT_PASSIVE_OPEN ((int)2)
#define TCP_INPUT_CLOSE ((int)3)
#define TCP_INPUT_SEND ((int)4)

// //// ////
// TCP actions
#define TCP_ACTION_SND_SYN ((int)5)
#define TCP_ACTION_SND_ACK ((int)6)
#define TCP_ACTION_SND_SYN_ACK ((int)7)
#define TCP_ACTION_SND_FIN ((int)8)
#define TCP_ACTION_CREATE_TCB ((int)9)
#define TCP_ACTION_DELETE_TCB ((int)10)

// //// ////
// TCP events
#define TCP_EVENT_RCV_SYN ((int)11)
#define TCP_EVENT_RCV_ACK_OF_SYN ((int)12)
#define TCP_EVENT_RCV_SYN_ACK ((int)13)
#define TCP_EVENT_RCV_FIN ((int)14)
#define TCP_EVENT_RCV_ACK_OF_FIN ((int)15)
#define TCP_EVENT_TIMEOUT_2MSL ((int)16)

// //// ////
// TCP states
#define TCP_STATE_CLOSED ((int)17)
#define TCP_STATE_LISTEN ((int)18)
#define TCP_STATE_SYN_SENT ((int)19)
#define TCP_STATE_SYN_RCVD ((int)20)

#define TCP_STATE_ESTABLISHED ((int)21)

#define TCP_STATE_FINWAIT_1 ((int)22)
#define TCP_STATE_FINWAIT_2 ((int)23)
#define TCP_STATE_CLOSING ((int)24)
#define TCP_STATE_TIME_WAIT ((int)25)
#define TCP_STATE_CLOSE_WAIT ((int)26)
#define TCP_STATE_LAST_ACK ((int)27)

// This array of strings is indexed by integer values in #define
// values above, so the order of the list is important!
const char *TCP_fsm_strings_G[] = {
    "--",
    "active OPEN",
    "passive OPEN",
    "CLOSE",
    "SEND",
    "snd SYN",
    "snd ACK",
    "snd SYN,ACK",
    "snd FIN",
    "create TCB",
    "delete TCB",
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
    "LAST_ACK"};

int checkInputsAndEvents(int tcp_state, int *inputs, int *events)
{
    int what;

    printf("\n** Current state: %s\n", TCP_fsm_strings_G[tcp_state]);

    what = TCP_INVALID;

    // Current possibilities for inputs or actions
    printf("   Possible inputs and events:\n");
    for (int *ip = inputs; *ip != TCP_INVALID; ++ip)
        printf("     %2d : %s\n", *ip, TCP_fsm_strings_G[*ip]);
    for (int *ep = events; *ep != TCP_INVALID; ++ep)
        printf("     %2d : %s\n", *ep, TCP_fsm_strings_G[*ep]);

    // Check validity of input
    for (int *ip = inputs; *ip != TCP_INVALID; ++ip)
        if (w == *ip)
        {
            what = *ip;
            break;
        }
    if (what == TCP_INVALID)
        for (int *ep = events; *ep != TCP_INVALID; ++ep)
            if (w == *ep)
            {
                what = *ep;
                break;
            }
    if (what == TCP_INVALID)
        printf("\n** Please select a number from the list.\n");

    green();
    printf("      S : %s\n\n", TCP_fsm_strings_G[w]);
    reset();

    return what;
}

#define CHECK_INPUTS_AND_EVENTS \
    what = checkInputsAndEvents(tcp_state, inputs, events);

void stateHandler()
{
    switch (tcp_state)
    {
    case TCP_STATE_CLOSED:
        inputs[0] = TCP_INPUT_ACTIVE_OPEN;  // from TCP user
        inputs[1] = TCP_INPUT_PASSIVE_OPEN; // from TCP user

        w = strcmp("-r", argv[2]) == 0 ? TCP_INPUT_ACTIVE_OPEN : TCP_INPUT_PASSIVE_OPEN;

        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
        case TCP_INPUT_ACTIVE_OPEN:
            receiver = 1;

            actions[0] = TCP_ACTION_CREATE_TCB; // local TCP
            actions[1] = TCP_ACTION_SND_SYN;    // local TCP
            tcp_new_state = TCP_STATE_SYN_SENT;
            break;

        case TCP_INPUT_PASSIVE_OPEN:
            transmitter = 1;
            actions[0] = TCP_ACTION_CREATE_TCB; // local TCP
            tcp_new_state = TCP_STATE_LISTEN;
            break;
        }
        break;

    case TCP_STATE_LISTEN:
        inputs[0] = TCP_INPUT_SEND;    // from TCP user
        inputs[1] = TCP_INPUT_CLOSE;   // from TCP user
        events[0] = TCP_EVENT_RCV_SYN; // from network

        w = TCP_EVENT_RCV_SYN;

        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
        case TCP_INPUT_SEND:
            actions[0] = TCP_ACTION_SND_SYN; // local TCP
            tcp_new_state = TCP_STATE_SYN_SENT;
            break;

        case TCP_INPUT_CLOSE:
            actions[0] = TCP_ACTION_DELETE_TCB; // local TCP
            actions[1] = TCP_INPUT_CLOSE;       // local TCP
            tcp_new_state = TCP_STATE_CLOSED;
            break;

        case TCP_EVENT_RCV_SYN:
            actions[0] = TCP_ACTION_SND_SYN_ACK; // local TCP
            tcp_new_state = TCP_STATE_SYN_RCVD;
            setupSIGALRM();
            break;
        }
        break;

    // SYN SENT
    case TCP_STATE_SYN_SENT:
        inputs[0] = TCP_INPUT_CLOSE;       // from TCP user
        events[0] = TCP_EVENT_RCV_SYN;     // from network
        events[1] = TCP_EVENT_RCV_SYN_ACK; // from network

        w = TCP_EVENT_RCV_SYN_ACK;
        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
        case TCP_INPUT_CLOSE:
            actions[0] = TCP_ACTION_DELETE_TCB; // local TCP
            actions[1] = TCP_INPUT_CLOSE;       // local TCP
            tcp_new_state = TCP_STATE_CLOSED;
            break;

        case TCP_EVENT_RCV_SYN:
            actions[0] = TCP_ACTION_SND_ACK; // local TCP
            tcp_new_state = TCP_STATE_SYN_RCVD;
            break;

        case TCP_EVENT_RCV_SYN_ACK:
            actions[0] = TCP_ACTION_SND_ACK; // local TCP
            tcp_new_state = TCP_STATE_ESTABLISHED;
            break;
        }
    // SYN RCVD
    case TCP_STATE_SYN_RCVD:
        inputs[0] = TCP_INPUT_CLOSE;          // from TCP user
        events[0] = TCP_EVENT_RCV_ACK_OF_SYN; // from network

        w = TCP_EVENT_RCV_ACK_OF_SYN;
        CHECK_INPUTS_AND_EVENTS;

        switch (what)
        {
        case TCP_INPUT_CLOSE:
            actions[0] = TCP_ACTION_SND_FIN; // local TCP
            actions[1] = TCP_INPUT_CLOSE;    // local TCP
            tcp_new_state = TCP_STATE_FINWAIT_1;
            break;

        case TCP_EVENT_RCV_ACK_OF_SYN:
            tcp_new_state = TCP_STATE_ESTABLISHED;
            setupTransfer();
            break;
        }
    case TCP_STATE_ESTABLISHED:
    {
        extern int windowSize;
        windowSize ? windowSize : windowSize++;

        unsigned char sendData[windowSize];

        /* Send the file */
        if (transmitter)
        {
            if (currentIndex >= filelen - 1)
            {
                green();
                printf("File Sent Completed! - total %ld bytes sent\n", currentIndex + 1);
                reset();
                exit(0);
            }
            for (unsigned int i = 0; i < windowSize; i++, currentIndex++)
            {
                sendData[i] = *((char *)fileBuffer + currentIndex);
            }
            int r = sendingData(G_local, G_remote, header, sendData);

            printf("** %d bytes sent. - HEADER: %d bytes | DATA: %d\n", r, HEADER_SIZE, r - HEADER_SIZE);

            // sleep(1);
        }

        /* Receive the file */
        if (receiver)
        {
            for (unsigned int i = HEADER_SIZE, j = 0; i < r; i++, j++)
            {
                recvData[j] = *((char *)buffer.bytes + i);
            }

            recvData[dataSize] = '\0';

            printf("%d, %d\t%s\n", dataSize, r, recvData);

            receivingData(G_local, G_remote, header);

            free(recvData);
        }
    }
    break;
    default:
        printf("We have a problem! %d\n", tcp_state);
        tcp_new_state = TCP_INVALID;
        break;
    }
}