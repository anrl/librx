#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "rxpriv.h"

typedef struct {
    Rx *rx;
    const char *str;
} Match;

int
match_transition (Match *m, Transition *t, const char *pos) {
    if (t->type & CHAR) {
        return POINTER_TO_INT(t->param) == *pos;
    }
    else if (t->type & NEGCHAR) {
        return POINTER_TO_INT(t->param) == *pos;
    }
    else if (t->type) {
        printf("unrecognized type %d\n", t->type);
        return 0;
    }
    else {
        return 1;
    }
}

int
match_state (Match *m, State *state, const char *pos) {
    List *elem;
    int retval;
    printf("matching %s\n", pos);
    if (state == m->rx->end)
        return 1;
    for (elem = state->transitions; elem; elem = elem->next) {
        Transition *t = elem->data;
        if (!match_transition(m, t, pos))
            continue;
        if (t->type & EAT && !*pos)
            continue;
        if (t->type & EAT)
            retval = match_state(m, t->to, pos + 1);
        else
            retval = match_state(m, t->to, pos);
        if (retval)
            return retval;
    }
    return 0;
}

int
rx_match (Rx *rx, const char *str) {
    Match *m = calloc(1, sizeof (Match));
    m->rx = rx;
    m->str = str;
    return match_state(m, rx->start, str);
}

