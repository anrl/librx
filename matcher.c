#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "rxpriv.h"

typedef struct {
    Rx *rx;
    const char *str;
    const char *beg;
    const char *fin;
} Match;

static int match_state ();

static int
match_quantified (Match *m, State *g, State *b, Quantified *q, int n,
                  const char *pos, const char **fin) {
    for (; n < q->min; n++) {
        if (!match_state(m, g, pos, &pos))
            return 0;
    }
    if (q->max && n >= q->max)
        return match_state(m, b, pos, fin);
    return match_state(m, g, pos, fin) &&
           match_quantified(m, g, b, q, n + 1, *fin, fin) ||
           match_state(m, b, pos, fin);
}

static int
transition_useable (Transition *t, const char *pos) {
    if (t->type & EAT && !*pos) {
        return 0;
    }
    else if (t->type & CHAR) {
        return POINTER_TO_INT(t->param) == *pos;
    }
    else if (t->type & NEGCHAR) {
        return POINTER_TO_INT(t->param) == *pos;
    }
    else if (t->type & QUANTIFIED) {
        return 1;
    }
    else if (t->type) {
        printf("unrecognized type %d\n", t->type);
        return 0;
    }
    else {
        return 1;
    }
}

static int
match_transition (Match *m, Transition *t, const char *pos, const char **fin) {
    if (!transition_useable(t, pos))
        return 0;
    if (t->type & QUANTIFIED)
        return match_quantified(m, t->to, t->ret, t->param, 0, pos, fin);
    if (t->ret)
        return match_state(m, t->to, pos, &pos) && match_state(m, t->ret, pos, fin);
    if (t->type & EAT)
        return match_state(m, t->to, pos + 1, fin);
    return match_state(m, t->to, pos, fin);
}

static int
match_state (Match *m, State *state, const char *pos, const char **fin) {
    List *elem;
    printf("matching %.*s\e[1;32m%.*s\e[0m%s\n",
        m->beg - m->str, m->str, pos - m->beg, m->beg, pos);
    if (!state->transitions) {
        *fin = pos;
        return 1;
    }
    for (elem = state->transitions; elem; elem = elem->next) {
        if (match_transition(m, elem->data, pos, fin))
            return 1;
    }
    return 0;
}

int
rx_match (Rx *rx, const char *str) {
    Match *m = calloc(1, sizeof (Match));
    int retval;
    m->rx = rx;
    m->str = str;
    for (m->beg = str; *m->beg; m->beg++) {
        retval = match_state(m, rx->start, m->beg, &m->fin);
        if (retval)
            break;
    }
    return retval;
}

