#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "rxpriv.h"

typedef struct {
    char *error;
    Rx *top;
    Rx *rx;
    CharClass *cc;
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
captureref (Parser *p, const char *pos, const char **fin) {
    /* captureref: '~~' <integer>?  */
    Transition *t;
    if (*pos++ != '~')
        return 0;
    if (*pos++ != '~')
        return 0;
    t = transition_new(p->rx->end, NULL);
    p->rx->end = t->back = state_new(p->rx);
    if (integer(p, pos, fin)) {
        int capture = atoi(pos);
        if (capture < 0) {
            p->error = strdupf("only non negative ints allowed at '%s'", pos);
            return -1;
        }
        t->type = CLUSTER;
        t->c = capture;
        pos = *fin;
    }
    else {
        t->to = p->top->start;
    }
    *fin = pos;
    return 1;
}

/* TODO all white space should use this rule and make it handle comments  */
int
ws (const char *pos, const char **fin) {
    /* ws: \s*  */
    while (isspace(*pos))
        pos++;
    *fin = pos;
    return 1;
}

static int
charclass (Parser *p, const char *pos, const char **fin) {
    /* charclass: '[' ('\]' | <-[\]]>)* ']' | upper | lower | alpha | digit |
                  xdigit | print | graph | cntrl | punct | alnum | space |
                  blank | word  */
    if (*pos == '[') {
        const char *start = ++pos;
        while (1) {
            if (!strncmp(pos, "\\]", 2))
                pos += 2;
            else if (*pos && *pos != ']')
                pos++;
            else
                break;
        }
        if (*pos != ']') {
            p->error = strdupf("expected ']' at '%s'", pos);
            return -1;
        }
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->set = strdupf("%.*s", pos - start, start);
        pos++;
    }
    /* TODO too much copypasta here, this can be much shorter  */
    else if (!strncmp(pos, "upper", 5)) {
        pos += 5;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->isfunc = isupper;
    }
    else if (!strncmp(pos, "lower", 5)) {
        pos += 5;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->isfunc = islower;
    }
    else if (!strncmp(pos, "alpha", 5)) {
        pos += 5;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->isfunc = isalpha;
    }
    else if (!strncmp(pos, "digit", 5)) {
        pos += 5;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->isfunc = isdigit;
    }
    else if (!strncmp(pos, "xdigit", 6)) {
        pos += 6;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->isfunc = isxdigit;
    }
    else if (!strncmp(pos, "print", 5)) {
        pos += 5;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->isfunc = isprint;
    }
    else if (!strncmp(pos, "graph", 5)) {
        pos += 5;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->isfunc = isgraph;
    }
    else if (!strncmp(pos, "cntrl", 5)) {
        pos += 5;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->isfunc = iscntrl;
    }
    else if (!strncmp(pos, "punct", 5)) {
        pos += 5;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->isfunc = ispunct;
    }
    else if (!strncmp(pos, "alnum", 5)) {
        pos += 5;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->isfunc = isalnum;
    }
    else if (!strncmp(pos, "space", 5)) {
        pos += 5;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->isfunc = isspace;
    }
    else if (!strncmp(pos, "blank", 5)) {
        pos += 5;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->isfunc = isblank;
    }
    else if (!strncmp(pos, "word", 4)) {
        pos += 4;
        p->cc = calloc(1, sizeof (CharClass));
        p->cc->set = strdup("a..zA..Z0..9_-");
    }
    else
        return 0;
    *fin = pos;
    return 1;
}

static int
charclasscombo (Parser *p, const char *pos, const char **fin) {
    /* charclasscombo: <[+-]>? <charclass> (<[+-]> <charclass>)*  */
    Transition *t = NULL;
    int not = 0;
    if (*pos == '+' || *pos == '-') {
        not = *pos == '-';
        pos++;
        ws(pos, &pos);
    }
    if (charclass(p, pos, fin)) {
        if (p->error)
            return -1;
        pos = *fin;
        t = transition_new(p->rx->end, state_new(p->rx));
        t->type = CHARCLASS;
        p->cc->not = not;
        t->ccc = list_push(t->ccc, p->cc);
        p->rx->end = t->to;
    }
    else {
        return 0;
    }
    while (1) {
        ws(pos, &pos);
        if (*pos == '+')
            not = 0;
        else if (*pos == '-')
            not = 1;
        else
            break;
        pos++;
        ws(pos, &pos);
        if (charclass(p, pos, fin)) {
            if (p->error)
                return -1;
            pos = *fin;
            p->cc->not = not;
            t->ccc = list_push(t->ccc, p->cc);
        }
        else {
            p->error = strdupf("expected charclass at '%s'", pos);
            return -1;
        }
    }
    ws(pos, &pos);
    *fin = pos;
    return 1;
}

static int
metasyntax (Parser *p, const char *pos, const char **fin) {
    /* metasyntax: '<' (<captureref> | <charclasscombo>) '>'  */
    if (*pos++ != '<')
        return 0;
    if (captureref(p, pos, fin)) {
        if (p->error)
            return -1;
        pos = *fin;
    }
    else if (charclasscombo(p, pos, fin)) {
        if (p->error)
            return -1;
        pos = *fin;
    }
    else {
        p->error = strdupf("unrecognized metasyntax at '%s'", pos);
        return -1;
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
    /* escape: '\' <-[a..zA..Z0..9_-] +[nNrRtT]>  */
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

