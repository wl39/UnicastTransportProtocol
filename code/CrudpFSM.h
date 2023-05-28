extern int receiver,
    transmitter,
    w,
    tcp_state,
    tcp_new_state, // after the state change

    // The TCP FSM only ever has 2 inputs, events, or actions
    // possible in each state, so the following is convenient.
    inputs[3],  // possible input from TCP user
    events[3],  // possible network or timer events
    actions[3], // actions to be taken during state change

    // Input for what to do next in the emulation.
    what; // what to do next

int checkInputsAndEvents(int tcp_state, int *inputs, int *events);

void stateHandler();

void runActions(char *remote, CrudpHeader_t *header);

void setupTransfer();

void green();

void reset();