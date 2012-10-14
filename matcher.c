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
match_char (Match *m, State *state, char c, int eat, const char *pos, const char **fin) {
    if (c != *pos)
        return 0;
    if (eat)
        pos++;
    return match_state(m, state, pos, fin);
}

static int
match_transition (Match *m, Transition *t, const char *pos, const char **fin) {
    if (t->type & QUANTIFIED)
        return match_quantified(m, t->to, t->ret, t->param, 0, pos, fin);
    else if (t->ret)
        return match_state(m, t->to, pos, fin) &&
               match_state(m, t->ret, *fin, fin);
    else if (t->type & CHAR)
        return match_char(
            m, t->to, POINTER_TO_INT(t->param), t->type & EAT, pos, fin);
    else if (!t->type)
        return match_state(m, t->to, pos, fin);
    printf("unrecognized transition type %d\n", t->type);
    return 0;
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

