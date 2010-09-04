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
        t->type = CAPTURE;
        t->c = capture;
        pos = *fin;
    }
    else {
        t->to = p->top->start;
    }
    *fin = pos;
    return 1;
}

int
ws (const char *pos, const char **fin) {
    /* ws: \s*  */
    while (isspace(*pos))
        pos++;
    *fin = pos;
    return 1;
}

static int
namedcharclass (Parser *p, const char **pos, const char *name,
                int (*isfunc)())
{
    int len = strlen(name);
    if (strncmp(*pos, name, len))
        return 0;
    *pos += len;
    p->cc = calloc(1, sizeof (CharClass));
    p->cc->isfunc = isfunc;
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
    else if (namedcharclass(p, &pos, "xdigit", isxdigit)) ;
    else if (namedcharclass(p, &pos, "upper",  isupper))  ;
    else if (namedcharclass(p, &pos, "lower",  islower))  ;
    else if (namedcharclass(p, &pos, "alpha",  isalpha))  ;
    else if (namedcharclass(p, &pos, "digit",  isdigit))  ;
    else if (namedcharclass(p, &pos, "print",  isprint))  ;
    else if (namedcharclass(p, &pos, "graph",  isgraph))  ;
    else if (namedcharclass(p, &pos, "cntrl",  iscntrl))  ;
    else if (namedcharclass(p, &pos, "punct",  ispunct))  ;
    else if (namedcharclass(p, &pos, "alnum",  isalnum))  ;
    else if (namedcharclass(p, &pos, "space",  isspace))  ;
    else if (namedcharclass(p, &pos, "blank",  isblank))  ;
    else if (namedcharclass(p, &pos, "word",   isword))   ;
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
    /* escape: '\' <-[a..zA..Z0..9_-] +[nNrRtTsSwWdD]>  */
    char c;
    Transition *t;
    int charclass = 0;
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
    else if (tolower(*pos) == 's')
        charclass = 1;
    else if (tolower(*pos) == 'w')
        charclass = 1;
    else if (tolower(*pos) == 'd')
        charclass = 1;
    else
        return 0;
    t = transition_new(p->rx->end, state_new(p->rx));
    if (charclass) {
        CharClass *cc = calloc(1, sizeof (CharClass));
        cc->not = isupper(*pos);
        if (tolower(*pos) == 's')
            cc->isfunc = isspace;
        else if (tolower(*pos) == 'w')
            cc->isfunc = isword;
        else if (tolower(*pos) == 'd')
            cc->isfunc = isdigit;
        t->ccc = list_push(t->ccc, cc);
        t->type = CHARCLASS;
    }
    else {
        t->c = c;
        t->type = isupper(*pos) ? NEGCHAR : CHAR;
    }
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
character (Parser *p, const char *pos, const char **fin) {
    /* character: <[a..zA..Z0..9_-.]>  */
    Transition *t;
    if (!(isalnum(*pos) || *pos == '_' || *pos == '-' || *pos == '.'))
        return 0;
    t = transition_new(p->rx->end, state_new(p->rx));
    t->type = *pos == '.' ? ANYCHAR : CHAR;
    t->c = *pos;
    p->rx->end = t->to;
    *fin = ++pos;
    return 1;
}

static int
genquantifier (Parser *p, const char *pos, const char **fin, State *start,
               State *astart)
{
    /* genquantifier: '**' \d+ ('..' (\d+ | '*'))? */
    int i, min;
    if (strncmp(pos, "**", 2))
        return 0;
    pos += 2;
    ws(pos, &pos);
    if (!integer(p, pos, fin)) {
        p->error = strdupf("expected integer at '%s'", pos);
        return -1;
    }
    min = atoi(pos);
    if (min < 0) {
        p->error = strdupf("only non negative ints allowed at '%s'", pos);
        return -1;
    }
    pos = *fin;
    p->rx->end = start;
    for (i = 0; i < min; i++) {
        Transition *t = transition_new(p->rx->end, astart);
        t->back = p->rx->end = state_new(p->rx);
    }
    ws(pos, &pos);
    *fin = pos;
    if (strncmp(pos, "..", 2))
        return 1;
    pos += 2;
    ws(pos, &pos);
    if (integer(p, pos, fin)) {
        State *end;
        int max = atoi(pos);
        if (max <= min) {
            p->error = strdupf("can't do ** n..m with n >= m at '%s'", pos);
            return -1;
        }
        pos = *fin;
        transition_new(p->rx->end, end = state_new(p->rx));
        for (i = 0; i < max - min; i++) {
            Transition *t = transition_new(p->rx->end, astart);
            t->back = p->rx->end = state_new(p->rx);
            transition_new(p->rx->end, end);
        }
        p->rx->end = end;
    }
    else if (*pos == '*') {
        pos++;
        Transition *t = transition_new(p->rx->end, astart);
        t->back = p->rx->end;
    }
    else {
        p->error = strdupf("expected integer or '*' at '%s'", pos);
        return -1;
    }
    *fin = pos;
    return 1;
}

static int
quantifier (Parser *p, const char *pos, const char **fin, State *start,
            State *astart)
{
    /* quantifier: <genquantifier> | '?' | '*' | '+' | ''  */
    if (genquantifier(p, pos, &pos, start, astart)) {
        if (p->error)
            return -1;
    }
    else {
        transition_new(start, astart);
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
    }
    *fin = pos;
    return 1;
}

static int
assertion (Parser *p, const char *pos, const char **fin) {
    /* assertion: '^' | '^^' | '$' | '$$' | '<<' | '>>' | '\b' | '\B'  */
    if (pos[0] == '^' && pos[1] == '^') {
        p->rx->end->assertfunc = bol;
        pos += 2;
    }
    else if (pos[0] == '^') {
        p->rx->end->assertfunc = bos;
        pos++;
    }
    else if (pos[0] == '$' && pos[1] == '$') {
        p->rx->end->assertfunc = eol;
        pos += 2;
    }
    else if (pos[0] == '$') {
        p->rx->end->assertfunc = eos;
        pos++;
    }
    else if (pos[0] == '<' && pos[1] == '<') {
        p->rx->end->assertfunc = lwb;
        pos += 2;
    }
    else if (pos[0] == '>' && pos[1] == '>') {
        p->rx->end->assertfunc = rwb;
        pos += 2;
    }
    else if (pos[0] == '\\' && pos[1] == 'b') {
        p->rx->end->assertfunc = wb;
        pos += 2;
    }
    else if (pos[0] == '\\' && pos[1] == 'B') {
        p->rx->end->assertfunc = nwb;
        pos += 2;
    }
    else
        return 0;
    *fin = pos;
    return 1;
}

static int
atom (Parser *p, const char *pos, const char **fin) {
    /* atom: <assertion> | (<character> | <escape> | <group> | <metasyntax> |
             <quote>) <quantifier>  */
    State *start = p->rx->end;
    State *astart;
    ws(pos, &pos);
    if (assertion(p, pos, &pos)) {
        *fin = pos;
        return 1;
    }
    astart = p->rx->end = state_new(p->rx);
    if      (character  (p, pos, &pos)) ;
    else if (escape     (p, pos, &pos)) ;
    else if (group      (p, pos, &pos)) ;
    else if (metasyntax (p, pos, &pos)) ;
    else if (quote      (p, pos, &pos)) ;
    else {
        p->rx->end = start;
        return 0;
    }
    if (p->error)
        return -1;
    ws(pos, &pos);
    quantifier(p, pos, &pos, start, astart);
    if (p->error)
        return -1;
    *fin = pos;
    return 1;
}

static int
conjunction (Parser *p, const char *pos, const char **fin) {
    /* conjunction: <atom>*  */
    while (1) {
        if (!atom(p, pos, &pos))
            break;
        if (p->error)
            return -1;
    }
    *fin = pos;
    return 1;
}

static int
disjunction (Parser *p, const char *pos, const char **fin) {
    /* disjuntion: '|'? <conjunction> ('|' <conjunction>)*  */
    State *start = p->rx->end;
    State *end = state_new(p->rx);
    ws(pos, &pos);
    if (*pos == '|')
        pos++;
    while (1) {
        conjunction(p, pos, &pos);
        if (p->error)
            return -1;
        transition_new(p->rx->end, end);
        p->rx->end = start;
        ws(pos, &pos);
        if (*pos == '|')
            pos++;
        else
            break;
    }
    p->rx->end = end;
    *fin = pos;
    return 1;
}

Rx *
rx_new (const char *rx_str) {
    const char *pos = rx_str;
    Rx *rx;
    Parser *p = calloc(1, sizeof (Parser));
    p->top = p->rx = calloc(1, sizeof (Rx));
    p->rx->end = p->rx->start = state_new(p->rx);
    if (rx_debug)
        printf("/%s/\n", rx_str);
    disjunction(p, pos, &pos);
    if (!p->error) {
        ws(pos, &pos);
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

