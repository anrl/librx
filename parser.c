#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
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
named_char_class (const char *pos, const char **fin, int (**func)()) {
    !strncmp(pos, "alnum", 5)  && (pos += 5) && (*func = isalnum) ||
    !strncmp(pos, "alpha", 5)  && (pos += 5) && (*func = isalpha) ||
    !strncmp(pos, "blank", 5)  && (pos += 5) && (*func = isblank) ||
    !strncmp(pos, "cntrl", 5)  && (pos += 5) && (*func = iscntrl) ||
    !strncmp(pos, "digit", 5)  && (pos += 5) && (*func = isdigit) ||
    !strncmp(pos, "graph", 5)  && (pos += 5) && (*func = isgraph) ||
    !strncmp(pos, "lower", 5)  && (pos += 5) && (*func = islower) ||
    !strncmp(pos, "print", 5)  && (pos += 5) && (*func = isprint) ||
    !strncmp(pos, "punct", 5)  && (pos += 5) && (*func = ispunct) ||
    !strncmp(pos, "space", 5)  && (pos += 5) && (*func = isspace) ||
    !strncmp(pos, "upper", 5)  && (pos += 5) && (*func = isupper) ||
    !strncmp(pos, "word", 4)   && (pos += 4) && (*func = isword) ||
    !strncmp(pos, "xdigit", 6) && (pos += 6) && (*func = isxdigit);
    if (!*func)
        return 0;
    *fin = pos;
    return 1;
}

static int
char_class (Parser *p, const char *pos, const char **fin, List **cc) {
    /* charclass: '[' ('\]' | <-[\]]>)* ']' | upper | lower | alpha | digit |
                  xdigit | print | graph | cntrl | punct | alnum | space |
                  blank | word  */
    List *action = NULL;
    int (*func) () = NULL;
    *cc = NULL;
    if (*pos == '[') {
        while (1) {
            pos++;
            ws(pos, &pos);
            if (!pos[0] || pos[0] == ']') {
                break;
            }
            else if (action && action->data == (void *) CC_CHAR &&
                     !strncmp(pos, "..", 2)) {
                pos++;
                action->data = (void *) CC_RANGE;
                continue;
            }
            if (!action || action->data == (void *) CC_CHAR)
                action = *cc = list_push(*cc, (void *) CC_CHAR);
            if (pos[0] == '\\') {
                if (pos[1] == '[' || pos[1] == ']' || pos[1] == ' ' || pos[1] == '\\')
                    *cc = list_push(*cc, (void *) pos[1]);
                else if (pos[1] == 'n')
                    *cc = list_push(*cc, (void *) '\n');
                else if (pos[1] == 'r')
                    *cc = list_push(*cc, (void *) '\r');
                else if (pos[1] == 't')
                    *cc = list_push(*cc, (void *) '\t');
                else
                    break;
                pos++;
            }
            else {
                *cc = list_push(*cc, (void *) pos[0]);
            }
            if (action->data == (void *) CC_RANGE)
                action = NULL;
        }
        if (*pos != ']') {
            p->error = strdupf("expected ']' at '%s'", pos);
            return -1;
        }
        pos++;
    }
    else if (named_char_class(pos, &pos, &func)) {
        *cc = list_push(*cc, (void *) CC_FUNC);
        *cc = list_push(*cc, func);
    }
    else {
        return 0;
    }
    *fin = pos;
    return 1;
}

static int
char_class_combo (Parser *p, const char *pos, const char **fin) {
    /* char_class_combo: <[+-]>? <char_class> (<[+-]> <char_class>)*  */
    Transition *t;
    List *cc;
    int excludes = 0;
    if (*pos == '+' || *pos == '-') {
        excludes = *pos == '-';
        pos++;
        ws(pos, &pos);
    }
    if (!char_class(p, pos, fin, &cc))
        return 0;
    if (p->error)
        return -1;
    pos = *fin;
    t = transition_new(p->rx->end, state_new(p->rx));
    t->type = EAT | CHARCLASS;
    t->cc = list_push(t->cc, (void *) (excludes ? CC_EXCLUDES : CC_INCLUDES));
    t->cc = list_cat(t->cc, cc);
    p->rx->end = t->to;
    while (1) {
        ws(pos, &pos);
        if (*pos == '+' || *pos == '-')
            excludes = *pos == '-';
        else
            break;
        pos++;
        ws(pos, &pos);
        if (!char_class(p, pos, fin, &cc))
            p->error = strdupf("expected charclass at '%s'", pos);
        if (p->error)
            return -1;
        pos = *fin;
        t->cc = list_cat(cc, t->cc);
        t->cc = list_unshift(t->cc, (void *) (excludes ? CC_EXCLUDES : CC_INCLUDES));
    }
    ws(pos, &pos);
    *fin = pos;
    return 1;
}

static int
metasyntax (Parser *p, const char *pos, const char **fin) {
    /* metasyntax: '<' (<captureref> | <char_class_combo>) '>'  */
    if (*pos++ != '<')
        return 0;
    if (captureref(p, pos, fin)) {
        if (p->error)
            return -1;
        pos = *fin;
    }
    else if (char_class_combo(p, pos, fin)) {
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
    p->rx = rx_extend(orig);
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
        c = tolower(*pos);
        t->type = EAT | CHARCLASS;
        t->cc = list_push(t->cc, (void *) (isupper(*pos) ? CC_EXCLUDES : CC_INCLUDES));
        t->cc = list_push(t->cc, (void *) CC_FUNC);
        t->cc = list_push(t->cc, c == 's' ? isspace
                               : c == 'w' ? isword
                               : c == 'd' ? isdigit : NULL);
    }
    else {
        t->c = c;
        t->type = isupper(*pos) ? EAT | NEGCHAR : EAT | CHAR;
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
            t->type = EAT | CHAR;
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
    t->type = *pos == '.' ? EAT | ANYCHAR : EAT | CHAR;
    t->c = *pos;
    p->rx->end = t->to;
    *fin = ++pos;
    return 1;
}

static int
quantifier (Parser *p, const char *pos, const char **fin, State *start) {
    /* quantifier: '**' \d+ ('..' (\d+ | '*'))? | '*' | '+' | '?'  */
    int i, min = 1, max = 1;
    if (!strncmp(pos, "**", 2)) {
        pos += 2;
        ws(pos, &pos);
        if (!integer(p, pos, fin)) {
            p->error = strdupf("expected integer at '%s'", pos);
            return -1;
        }
        min = max = atoi(pos);
        if (min < 0) {
            p->error = strdupf("only non negative ints allowed at '%s'", pos);
            return -1;
        }
        pos = *fin;
        ws(pos, &pos);
        *fin = pos;
        if (!strncmp(pos, "..", 2)) {
            pos += 2;
            ws(pos, &pos);
            if (integer(p, pos, fin)) {
                max = atoi(pos);
                if (max <= min) {
                    p->error = strdupf("can't do ** n..m with m <= n at '%s'", pos);
                    return -1;
                }
                pos = *fin;
            }
            else if (*pos == '*') {
                max = 0;
                pos++;
            }
            else {
                p->error = strdupf("expected integer or '*' at '%s'", pos);
                return -1;
            }
        }
    }
    else if (*pos == '*') {
        min = 0;
        max = 0;
        pos++;
    }
    else if (*pos == '+') {
        min = 1;
        max = 0;
        pos++;
    }
    else if (*pos == '?') {
        min = 0;
        max = 1;
        pos++;
    }
    else {
        return 0;
    }
    *fin = pos;
    if (min == 1 && max == 1) {
    }
    else if (min == 0 && max == 0) {
        transition_new(p->rx->end, start);
        p->rx->end = start;
    }
    else if (min == 1 && max == 0) {
        transition_new(p->rx->end, start);
    }
    else if (min == 0 && max == 1) {
        transition_new(start, p->rx->end);
    }
    else {
        State *atom = state_split(start);
        p->rx->end = start;
        for (i = 0; i < min; i++) {
            Transition *t = transition_new(p->rx->end, atom);
            t->back = p->rx->end = state_new(p->rx);
        }
        if (max > min) {
            State *end = state_new(p->rx);
            transition_new(p->rx->end, end);
            for (i = 0; i < max - min; i++) {
                Transition *t = transition_new(p->rx->end, atom);
                t->back = p->rx->end = state_new(p->rx);
                transition_new(p->rx->end, end);
            }
            p->rx->end = end;
        }
        if (!max) {
            Transition *t = transition_new(p->rx->end, atom);
            t->back = p->rx->end;
        }
    }
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
    int amatch;
    ws(pos, &pos);
    if (assertion(p, pos, &pos)) {
        *fin = pos;
        return 1;
    }
    amatch = character(p, pos, &pos) ||
             escape(p, pos, &pos) ||
             group(p, pos, &pos) ||
             metasyntax(p, pos, &pos) ||
             quote(p, pos, &pos);
    if (!amatch)
        return 0;
    if (p->error)
        return -1;
    ws(pos, &pos);
    quantifier(p, pos, &pos, start);
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

int
rx_parse (Rx *rx) {
    const char *pos = rx->regex;
    Parser *p = calloc(1, sizeof (Parser));
    p->top = p->rx = rx;
    rx->end = rx->start = state_new(rx);
    disjunction(p, pos, &pos);
    if (!p->error) {
        ws(pos, &pos);
        if (*pos)
            p->error = strdupf("invalid regex syntax '%s'", pos);
    }
    if (p->error) {
        fprintf(stderr, "%s\n", p->error);
        free(p->error);
        free(p);
        return 0;
    }
    free(p);
    return 1;
}

