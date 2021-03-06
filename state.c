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

static Quantified *
quantified_new (Rx *rx, int min, int max) {
    Quantified *q = calloc(1, sizeof (Quantified));
    q->min = min;
    q->max = max;
    rx->quantifications = list_push(rx->quantifications, q);
    return q;
}

void
quantify (State **a, State **b, int min, int max) {
    State *g, *h;
    Quantified *q;
    if (min == 1 && max == 1)
        return;
    g = *a;
    h = *b;
    *a = state_new(g->group);
    q = quantified_new(g->group, min, max);
    *b = transition_to_group(*a, g, h, QUANTIFIED, q);
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

