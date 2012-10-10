#include <stdlib.h>
#include <stdio.h>
#include "rxpriv.h"

Transition *
transition_new (State *from, State *to, State *ret, int type, void *param) {
    Transition *t = calloc(1, sizeof (Transition));
    t->to = to;
    t->ret = ret;
    t->type = type;
    t->param = param;
    from->transitions = list_push(from->transitions, t);
    return t;
}

Transition *back_transition_new (State *from, State *to, State *ret,
                                 int type, void *param) {
    Transition *t = calloc(1, sizeof (Transition));
    t->to = to;
    t->ret = ret;
    t->type = type;
    t->param = param;
    from->backtransitions = list_push(from->backtransitions, t);
    return t;
}

void
transition_free (Transition *t) {
    free(t);
}

State *
transition_state (State *a, State *b, int type, void *param) {
    if (!b)
        b = state_new(a->group);
    transition_new(a, b, NULL, type, param);
    back_transition_new(b, a, NULL, type, param);
    return b;
}

State *
transition_to_group (State *a, State *g, State *h, int type, void *param) {
    State *b = state_new(a->group);
    transition_new(a, g, b, type, param);
    back_transition_new(b, h, a, type, param);
    return b;
}

void
quantify (State **a, State **b, int min, int max) {
    State *g = *a, *h = *b;
    int i;
    if (min == 1 && max == 1) {
        return;
    }
    *a = *b = state_new(g->group);
    for (i = 0; i < min; i++) {
        *b = transition_to_group(*b, g, h, 0, NULL);
    }
    for (i = 0; i < max - min; i++) {
        State *c = transition_to_group(*b, g, h, 0, NULL);
        transition_new(*b, c, NULL, 0, NULL);
        back_transition_new(c, *b, NULL, 0, NULL);
        *b = c;
    }
    if (max == 0) {
        transition_new(*b, g, *b, 0, NULL);
        back_transition_new(*b, h, *b, 0, NULL);
    }
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
    next->backtransitions = state->backtransitions;
    state->backtransitions = NULL;
    next->assertfunc = state->assertfunc;
    state->assertfunc = NULL;
    return next;
}

void
state_free (State *state) {
    list_free(state->transitions, transition_free);
    list_free(state->backtransitions, transition_free);
    free(state);
}

