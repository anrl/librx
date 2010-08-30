#include <stdlib.h>
#include <stdio.h>
#include "rxpriv.h"

void
charclass_free (CharClass *cc) {
    free(cc->set);
    free(cc);
}

Transition *
transition_new (State *from, State *to) {
    Transition *t = calloc(1, sizeof (Transition));
    t->to = to;
    from->transitions = list_push(from->transitions, t);
    return t;
}

void
transition_free (Transition *t) {
    list_free(t->ccc, charclass_free);
    free(t);
}

State *
state_new (Rx *rx) {
    State *state = calloc(1, sizeof (State));
    rx->states = list_push(rx->states, state);
    state->group = rx;
    return state;
}

void
state_free (State *state) {
    list_free(state->transitions, transition_free);
    free(state);
}

