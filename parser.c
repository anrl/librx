#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "rxpriv.h"

static int disjunction ();

static int
integer (Rx *rx, const char *pos, const char **end) {
    /* integer: ('+' | '-')? \d+  */
    if (*pos == '-' || *pos == '+')
        pos++;
    if (!isdigit(*pos))
        return 0;
    while (isdigit(*pos++))
        *end = pos;
    return 1;
}

static int
metasyntax (Rx *rx, const char *pos, const char **end) {
    /* metasyntax: '<' <integer> '>'  */
    int subpattern;
    State *endstate;
    Transition *t;
    if (*pos++ != '<')
        return 0;
    /* Extensible meta-syntax only supports subpatterns at the moment.  */
    if (!integer(rx, pos, end)) {
        rx->error = strdupf("expected integer at '%s'", pos);
        return -1;
    }
    subpattern = atoi(pos);
    if (subpattern < 0) {
        rx->error = strdupf("only positive integers allowed at '%s'", pos);
        return -1;
    }
    /* While right now its only for patterns previously entered in as a group,
    if I allocate the start for the pattern and fill it in later, I could
    potentially reference patterns not yet seen. There will be an error if the
    pattern is never filled in.  */
    if (subpattern >= list_elems(rx->groups)) {
        rx->error = strdupf("subpattern %d does not exist yet '%s'",
                            subpattern, pos);
        return -1;
    }
    endstate = calloc(1, sizeof (State));
    rx->states = list_push(rx->states, endstate);
    t = calloc(1, sizeof (Transition));
    t->type = NOCHAR;
    t->to = list_nth_data(rx->groups, subpattern);
    t->back = endstate;
    rx->head->transitions = list_push(rx->head->transitions, t);
    rx->head = endstate;
    pos = *end;
    if (*pos != '>') {
        rx->error = strdupf("expected '>' at '%s'", pos);
        return -1;
    }
    *end = ++pos;
    return 1;
}

static int
group (Rx *rx, const char *pos, const char **end) {
    /* group: '(' <disjunction> ')'  */
    State *start;
    State *endstate;
    Transition *t;
    if (*pos++ != '(')
        return 0;
    start = calloc(1, sizeof (State));
    rx->states = list_push(rx->states, start);
    endstate = calloc(1, sizeof (State));
    rx->states = list_push(rx->states, endstate);
    t = calloc(1, sizeof (Transition));
    t->type = NOCHAR;
    t->to = start;
    t->back = endstate;
    rx->head->transitions = list_push(rx->head->transitions, t);
    rx->groups = list_push(rx->groups, start);
    rx->head = start;
    disjunction(rx, pos, end);
    if (rx->error)
        return -1;
    rx->head = endstate;
    pos = *end;
    if (*pos != ')') {
        rx->error = strdupf("expected ')' at '%s'", pos);
        return -1;
    }
    *end = ++pos;
    return 1;
}

static int
escape (Rx *rx, const char *pos, const char **end) {
    /* escape: '\' (<-[a..zA..Z0..9_-] +[nNrRtT]>)  */
    State *state;
    Transition *t;
    char c;
    if (*pos++ != '\\')
        return 0;
    if (!(isalnum(*pos) || *pos == '_' || *pos == '-')) c = *pos;
    else if (tolower(*pos) == 'n') c = '\n';
    else if (tolower(*pos) == 'r') c = '\r';
    else if (tolower(*pos) == 't') c = '\t';
    else return 0;
    state = calloc(1, sizeof (State));
    t = calloc(1, sizeof (Transition));
    rx->states = list_push(rx->states, state);
    t->type = isupper(*pos) ? NEGCHAR : CHAR;
    t->c = c;
    t->to = state;
    rx->head->transitions = list_push(rx->head->transitions, t);
    rx->head = state;
    *end = ++pos;
    return 1;
}

static int
atom (Rx *rx, const char *pos, const char **end) {
    /* atom: (<[a..z0..9_-]> | <escape> | '.' | <group> | <metasyntax>)
             ('?' | '*' | '+')?  */
    State *head = rx->head;
    while (isspace(*pos))
        pos++;
    if (isalnum(*pos) || *pos == '_' || *pos == '-' || *pos == '.') {
        State *s = calloc(1, sizeof (State));
        Transition *t = calloc(1, sizeof (Transition));
        rx->states = list_push(rx->states, s);
        t->type = *pos == '.' ? ANYCHAR : CHAR;
        t->c = *pos;
        t->to = s;
        rx->head->transitions = list_push(rx->head->transitions, t);
        rx->head = s;
        pos++;
    }
    else if (escape(rx, pos, end)) {
        pos = *end;
    }
    else if (group(rx, pos, end)) {
        if (rx->error)
            return -1;
        pos = *end;
    }
    else if (metasyntax(rx, pos, end)) {
        if (rx->error)
            return -1;
        pos = *end;
    }
    else {
        *end = pos;
        return 0;
    }
    while (isspace(*pos))
        pos++;
    if (*pos == '*') {
        Transition *t = calloc(1, sizeof (Transition));
        t->to = head;
        rx->head->transitions = list_push(rx->head->transitions, t);
        rx->head = head;
        pos++;
    }
    else if (*pos == '+') {
        Transition *t = calloc(1, sizeof (Transition));
        t->to = head;
        rx->head->transitions = list_push(rx->head->transitions, t);
        pos++;
    }
    else if (*pos == '?') {
        Transition *t = calloc(1, sizeof (Transition));
        t->to = rx->head;
        head->transitions = list_push(head->transitions, t);
        pos++;
    }
    *end = pos;
    return 1;
}

static int
conjunction (Rx *rx, const char *pos, const char **end) {
    /* conjunction: <atom>*  */
    while (1) {
        if (atom(rx, pos, end)) {
            if (rx->error)
                return -1;
        }
        else
            break;
        pos = *end;
    }
    return 1;
}

static int
disjunction (Rx *rx, const char *pos, const char **end) {
    /* disjuntion: '|'? <conjunction> ('|' <conjunction>)*  */
    State *head = rx->head;
    State *endstate = calloc(1, sizeof (State));
    rx->states = list_push(rx->states, endstate);
    while (isspace(*pos))
        pos++;
    if (*pos == '|')
        pos++;
    while (1) {
        Transition *t;
        conjunction(rx, pos, end);
        if (rx->error)
            return -1;
        pos = *end;
        t = calloc(1, sizeof (Transition));
        t->type = NOCHAR;
        t->to = endstate;
        rx->head->transitions = list_push(rx->head->transitions, t);
        rx->head = head;
        while (isspace(*pos))
            pos++;
        if (*pos == '|')
            pos++;
        else
            break;
    }
    rx->head = endstate;
    return 1;
}

Rx *
rx_new (const char *rx_str) {
    const char *pos = rx_str;
    const char *end = NULL;
    Rx *rx = calloc(1, sizeof (Rx));
    rx->start = calloc(1, sizeof (State));
    rx->states = list_push(rx->states, rx->start);
    rx->head = rx->start;
    if (rx_debug)
        printf("/%s/\n", rx_str);
    disjunction(rx, pos, &end);
    if (!rx->error) {
        pos = end;
        while (isspace(*pos))
            pos++;
        if (*pos)
            rx->error = strdupf("invalid regex syntax '%s'", pos);
    }
    if (rx->error) {
        fprintf(stderr, "%s\n", rx->error);
        rx_free(rx);
        return NULL;
    }
    if (rx_debug)
        rx_print(rx);
    return rx;
}

