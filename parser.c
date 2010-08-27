#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "rxpriv.h"

typedef struct {
    char *error;
    Rx *top;
    Rx *rx;
} Parser;

static int disjunction ();

static int
integer (Parser *p, const char *pos, const char **fin) {
    /* integer: ('+' | '-')? \d+  */
    if (*pos == '-' || *pos == '+')
        pos++;
    if (!isdigit(*pos))
        return 0;
    while (isdigit(*pos++))
        *fin = pos;
    return 1;
}

static int
metasyntax (Parser *p, const char *pos, const char **fin) {
    /* metasyntax: '<' '~~' <integer>? '>'  */
    Transition *t;
    if (*pos++ != '<')
        return 0;
    /* Extensible meta-syntax only supports numbered captures at the moment  */
    if (*pos++ != '~')
        return 0;
    if (*pos++ != '~')
        return 0;
    t = transition_new(p->rx->end, NULL);
    p->rx->end = t->back = state_new(p->rx);
    if (integer(p, pos, fin)) {
        int capture = atoi(pos);
        if (capture < 0) {
            p->error = strdupf("only non negative integers allowed at '%s'", pos);
            return -1;
        }
        t->type = CLUSTER;
        t->c = capture;
        pos = *fin;
    }
    else {
        t->to = p->top->start;
    }
    if (*pos != '>') {
        p->error = strdupf("expected '>' at '%s'", pos);
        return -1;
    }
    *fin = ++pos;
    return 1;
}

static int
group (Parser *p, const char *pos, const char **fin) {
    /* group: '(' <disjunction> ')' | '[' <disjunction> ']'  */
    Rx *orig = p->rx;
    Transition *t;
    char ldelimeter = *pos++;
    char rdelimeter;
    switch (ldelimeter) {
        case '(': rdelimeter = ')'; break;
        case '[': rdelimeter = ']'; break;
        default: return 0;
    }
    p->rx = calloc(1, sizeof (Rx));
    rx_extends(p->rx, orig);
    p->rx->end = p->rx->start = state_new(p->rx);
    if (ldelimeter == '(')
        orig->captures = list_push(orig->captures, p->rx);
    else
        orig->clusters = list_push(orig->clusters, p->rx);
    t = transition_new(orig->end, p->rx->start);
    t->back = state_new(orig);
    disjunction(p, pos, fin);
    if (p->error)
        return -1;
    p->rx = orig;
    p->rx->end = t->back;
    pos = *fin;
    if (*pos != rdelimeter) {
        p->error = strdupf("expected '%c' at '%s'", rdelimeter, pos);
        return -1;
    }
    *fin = ++pos;
    return 1;
}

static int
escape (Parser *p, const char *pos, const char **fin) {
    /* escape: '\' (<-[a..zA..Z0..9_-] +[nNrRtT]>)  */
    Transition *t;
    char c;
    if (*pos++ != '\\')
        return 0;
    if (!(isalnum(*pos) || *pos == '_' || *pos == '-'))
        c = *pos;
    else if (tolower(*pos) == 'n')
        c = '\n';
    else if (tolower(*pos) == 'r')
        c = '\r';
    else if (tolower(*pos) == 't')
        c = '\t';
    else
        return 0;
    t = transition_new(p->rx->end, state_new(p->rx));
    t->type = isupper(*pos) ? NEGCHAR : CHAR;
    t->c = c;
    p->rx->end = t->to;
    *fin = ++pos;
    return 1;
}

static int
quote (Parser *p, const char *pos, const char **fin) {
    /* quote: '"' (<-["]> | <escape>)* '"' | "'" <-[']>* "'" */
    char delimeter = *pos++;
    if (!(delimeter == '"' || delimeter == '\''))
        return 0;
    while (1) {
        if (delimeter == '"' && escape(p, pos, fin)) {
            pos = *fin;
        }
        else if (*pos && *pos != delimeter) {
            Transition *t = transition_new(p->rx->end, state_new(p->rx));
            t->type = CHAR;
            t->c = *pos;
            p->rx->end = t->to;
            pos++;
        }
        else
            break;
    }
    if (*pos != delimeter) {
        p->error = strdupf("expected %c at '%s'", delimeter, pos);
        return -1;
    }
    *fin = ++pos;
    return 1;
}

static int
atom (Parser *p, const char *pos, const char **fin) {
    /* atom: (<[a..zA..Z0..9_-]> | <escape> | '.' | <group> | <metasyntax> |
              <quote>) ('?' | '*' | '+')?  */
    State *start = p->rx->end;
    while (isspace(*pos))
        pos++;
    if (isalnum(*pos) || *pos == '_' || *pos == '-' || *pos == '.') {
        Transition *t = transition_new(p->rx->end, state_new(p->rx));
        t->type = *pos == '.' ? ANYCHAR : CHAR;
        t->c = *pos;
        p->rx->end = t->to;
        pos++;
    }
    else if (escape(p, pos, fin)) {
        pos = *fin;
    }
    else if (group(p, pos, fin)) {
        if (p->error)
            return -1;
        pos = *fin;
    }
    else if (metasyntax(p, pos, fin)) {
        if (p->error)
            return -1;
        pos = *fin;
    }
    else if (quote(p, pos, fin)) {
        if (p->error)
            return -1;
        pos = *fin;
    }
    else {
        *fin = pos;
        return 0;
    }
    while (isspace(*pos))
        pos++;
    /* quantifier  */
    if (*pos == '*') {
        transition_new(p->rx->end, start);
        p->rx->end = start;
        pos++;
    }
    else if (*pos == '+') {
        transition_new(p->rx->end, start);
        pos++;
    }
    else if (*pos == '?') {
        transition_new(start, p->rx->end);
        pos++;
    }
    *fin = pos;
    return 1;
}

static int
conjunction (Parser *p, const char *pos, const char **fin) {
    /* conjunction: <atom>*  */
    while (1) {
        if (atom(p, pos, fin)) {
            if (p->error)
                return -1;
        }
        else
            break;
        pos = *fin;
    }
    return 1;
}

static int
disjunction (Parser *p, const char *pos, const char **fin) {
    /* disjuntion: '|'? <conjunction> ('|' <conjunction>)*  */
    State *start = p->rx->end;
    State *end = state_new(p->rx);
    while (isspace(*pos))
        pos++;
    if (*pos == '|')
        pos++;
    while (1) {
        conjunction(p, pos, fin);
        if (p->error)
            return -1;
        pos = *fin;
        transition_new(p->rx->end, end);
        p->rx->end = start;
        while (isspace(*pos))
            pos++;
        if (*pos == '|')
            pos++;
        else
            break;
    }
    p->rx->end = end;
    return 1;
}

Rx *
rx_new (const char *rx_str) {
    const char *pos = rx_str;
    const char *fin = NULL;
    Rx *rx;
    Parser *p = calloc(1, sizeof (Parser));
    p->top = p->rx = calloc(1, sizeof (Rx));
    p->rx->end = p->rx->start = state_new(p->rx);
    if (rx_debug)
        printf("/%s/\n", rx_str);
    disjunction(p, pos, &fin);
    if (!p->error) {
        pos = fin;
        while (isspace(*pos))
            pos++;
        if (*pos)
            p->error = strdupf("invalid regex syntax '%s'", pos);
    }
    if (p->error) {
        fprintf(stderr, "%s\n", p->error);
        rx_free(p->top);
        free(p->error);
        free(p);
        return NULL;
    }
    if (rx_debug)
        rx_print(p->top);
    rx = p->top;
    free(p);
    return rx;
}

