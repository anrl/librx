#include <stdlib.h>
#include "rxpriv.h"

State *
state_new (Rx *rx) {
    State *state = calloc(1, sizeof (State));
    rx->states = list_push(rx->states, state);
    state->group = rx;
    return state;
}

void
state_free (State *state) {
    list_free(state->transitions, free);
    free(state);
}

Transition *
transition_new (State *from, State *to) {
    Transition *t = calloc(1, sizeof (Transition));
    t->to = to;
    from->transitions = list_push(from->transitions, t);
    return t;
}

