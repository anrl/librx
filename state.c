#include <stdlib.h>
#include <stdio.h>
#include "rxpriv.h"

Transition *
transition_new (State *from, State *to) {
    Transition *t = calloc(1, sizeof (Transition));
    t->to = to;
    from->transitions = list_push(from->transitions, t);
    return t;
}

void
transition_free (Transition *t) {
    free(t);
}

State *
state_new (Rx *rx) {
    State *state = calloc(1, sizeof (State));
    rx->states = list_push(rx->states, state);
    state->group = rx;
    return state;
}

State *
state_split (State *state) {
    State *next = state_new(state->group);
    next->transitions = state->transitions;
    state->transitions = NULL;
    next->assertfunc = state->assertfunc;
    state->assertfunc = NULL;
    return next;
}

void
state_free (State *state) {
    list_free(state->transitions, transition_free);
    free(state);
}

