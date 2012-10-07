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
    /* integer: [+-]? \d+  */
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
    int capture = -1;
    if (*pos++ != '~')
        return 0;
    if (*pos++ != '~')
        return 0;
    if (integer(p, pos, fin)) {
        capture = atoi(pos);
        if (capture < 0) {
            p->error = strdupf("only non negative ints allowed at '%s'", pos);
            return -1;
        }
        pos = *fin;
    }
    p->rx->end = transition_to_group(
        p->rx->end, NULL, NULL, CAPTUREREF, INT_TO_POINTER(capture));
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
named_char_class (Parser *p, const char *pos, const char **fin, List **cc) {
    /* named_char_class: alnum | alpha | blank | cntrl | digit | graph |
                         lower | print | punct | space | upper | word |
                         xdigit  */
    int (*func) () = NULL;
    int retval =
        !strncmp(pos, "alnum", 5)  && (pos += 5) && (func = isalnum) ||
        !strncmp(pos, "alpha", 5)  && (pos += 5) && (func = isalpha) ||
        !strncmp(pos, "blank", 5)  && (pos += 5) && (func = isblank) ||
        !strncmp(pos, "cntrl", 5)  && (pos += 5) && (func = iscntrl) ||
        !strncmp(pos, "digit", 5)  && (pos += 5) && (func = isdigit) ||
        !strncmp(pos, "graph", 5)  && (pos += 5) && (func = isgraph) ||
        !strncmp(pos, "lower", 5)  && (pos += 5) && (func = islower) ||
        !strncmp(pos, "print", 5)  && (pos += 5) && (func = isprint) ||
        !strncmp(pos, "punct", 5)  && (pos += 5) && (func = ispunct) ||
        !strncmp(pos, "space", 5)  && (pos += 5) && (func = isspace) ||
        !strncmp(pos, "upper", 5)  && (pos += 5) && (func = isupper) ||
        !strncmp(pos, "word", 4)   && (pos += 4) && (func = isword)  ||
        !strncmp(pos, "xdigit", 6) && (pos += 6) && (func = isxdigit);
    if (!retval)
        return 0;
    *fin = pos;
    *cc = list_push(*cc, INT_TO_POINTER(CC_FUNC));
    *cc = list_push(*cc, func);
    return 1;
}

static int
escaped_char_class (Parser *p, const char *pos, const char **fin,
                   int *type, void **value) {
    /* escaped_char_class: '\' <-[a..zA..Z0..9_-] +[nNrRtTsSwWdD]>  */
    char c, l;
    if (*pos++ != '\\')
        return 0;
    c = *pos++;
    l = tolower(c);
    if (!isalnum(c) && c != '_' && c != '-') {
        *type = CC_CHAR;
        *value = INT_TO_POINTER(c);
    }
    else if (l == 'n' || l == 'r' || l == 't') {
        *type = c == l ? CC_CHAR : CC_NCHAR;
        *value = INT_TO_POINTER(l == 'n' ? '\n' : l == 'r' ? '\r' : '\t');
    }
    else if (l == 's' || l == 'w' || l == 'd') {
        *type = c == l ? CC_FUNC : CC_NFUNC;
        *value = l == 's' ? isspace : l == 'w' ? isword : isdigit;
    }
    else {
        return 0;
    }
    *fin = pos;
    return 1;
}

static int
bracketed_char_class (Parser *p, const char *pos, const char **fin, List **cc) {
    /* bracketed_char_class: '[' (<escaped_char_class> | <-[\]]>)* ']'  */
    List *action = NULL;
    int seen_char = 0;
    int type;
    void *value;
    if (*pos++ != '[')
        return 0;
    while (1) {
        ws(pos, &pos);
        if (!pos[0] || pos[0] == ']')
            break;
        if (seen_char && !strncmp(pos, "..", 2)) {
            pos += 2;
            action->data = INT_TO_POINTER(CC_RANGE);
            continue;
        }
        if (!seen_char)
            action = *cc = list_push(*cc, INT_TO_POINTER(CC_CHAR));
        if (escaped_char_class(p, pos, &pos, &type, &value)) {
            action->data = INT_TO_POINTER(type);
            *cc = list_push(*cc, value);
        }
        else {
            *cc = list_push(*cc, INT_TO_POINTER(pos[0]));
            pos++;
        }
        seen_char = 1;
        if (action->data != INT_TO_POINTER(CC_CHAR)) {
            action = NULL;
            seen_char = 0;
        }
    }
    if (*pos != ']') {
        p->error = strdupf("expected ']' at '%s'", pos);
        return -1;
    }
    *fin = ++pos;
    return 1;
}

static int
char_class (Parser *p, const char *pos, const char **fin, List **cc) {
    /* char_class: <bracketed_char_class> | <named_char_class>  */
    *cc = NULL;
    if (!bracketed_char_class(p, pos, &pos, cc) &&
        !named_char_class(p, pos, &pos, cc))
        return 0;
    if (p->error)
        return -1;
    *fin = pos;
    return 1;
}

static int
char_class_combo (Parser *p, const char *pos, const char **fin) {
    /* char_class_combo: <[+-]>? <char_class> (<[+-]> <char_class>)*  */
    List *actions;
    CharClass *cc;
    int container = CC_INCLUDES;
    const char *start = pos;
    if (*pos == '+' || *pos == '-') {
        container = *pos == '-' ? CC_EXCLUDES : CC_INCLUDES;
        pos++;
        ws(pos, &pos);
    }
    if (!char_class(p, pos, fin, &actions))
        return 0;
    if (p->error)
        return -1;
    pos = *fin;
    cc = char_class_new(p->rx, start - 1, 0);
    cc->actions = list_push(cc->actions, INT_TO_POINTER(container));
    cc->actions = list_cat(cc->actions, actions);
    p->rx->end = transition_state(p->rx->end, NULL, EAT|CHARCLASS, cc);
    while (1) {
        ws(pos, &pos);
        if (*pos != '+' && *pos != '-')
            break;
        pos++;
        container = *pos == '-' ? CC_EXCLUDES : CC_INCLUDES;
        ws(pos, &pos);
        if (!char_class(p, pos, fin, &actions))
            p->error = strdupf("expected charclass at '%s'", pos);
        if (p->error)
            return -1;
        pos = *fin;
        cc->actions = list_cat(actions, cc->actions);
        cc->actions = list_unshift(cc->actions, INT_TO_POINTER(container));
    }
    ws(pos, &pos);
    cc->length = pos - start + 2;
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
    Rx *orig = p->rx, *group;
    char ldelimeter = *pos++, rdelimeter;
    switch (ldelimeter) {
        case '(': rdelimeter = ')'; break;
        case '[': rdelimeter = ']'; break;
        default: return 0;
    }
    group = rx_extend(orig);
    if (ldelimeter == '(')
        orig->captures = list_push(orig->captures, group);
    else
        orig->clusters = list_push(orig->clusters, group);
    p->rx = group;
    disjunction(p, pos, fin);
    p->rx = orig;
    if (p->error)
        return -1;
    p->rx->end = transition_to_group(
        p->rx->end, group->start, group->end, 0, NULL);
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
    int type;
    void *value;
    if (!escaped_char_class(p, pos, fin, &type, &value))
        return 0;
    if (type == CC_FUNC || type == CC_NFUNC) {
        CharClass *cc = char_class_new(p->rx, pos - 2, 2);
        cc->actions = list_push(cc->actions, INT_TO_POINTER(CC_INCLUDES));
        cc->actions = list_push(cc->actions, INT_TO_POINTER(type));
        cc->actions = list_push(cc->actions, value);
        p->rx->end = transition_state(p->rx->end, NULL, EAT|CHARCLASS, cc);
    }
    else {
        p->rx->end = transition_state(
            p->rx->end, NULL, CC_CHAR ? EAT|CHAR : EAT|NEGCHAR, value);
    }
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
            p->rx->end = transition_state(
                p->rx->end, NULL, EAT|CHAR, INT_TO_POINTER(*pos));
            pos++;
        }
        else {
            break;
        }
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
    int type;
    if (!(isalnum(*pos) || *pos == '_' || *pos == '-' || *pos == '.'))
        return 0;
    type = *pos == '.' ? EAT|ANYCHAR : EAT|CHAR;
    p->rx->end = transition_state(p->rx->end, NULL, type, INT_TO_POINTER(*pos));
    *fin = ++pos;
    return 1;
}

static int
quantifier (Parser *p, const char *pos, const char **fin, State *start) {
    /* quantifier: '**' \d+ ('..' (\d+ | '*'))? | '*' | '+' | '?'  */
    int min = 1, max = 1;
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
    p->rx->end = quantify(start, p->rx->end, min, max);
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
    State *end = NULL;
    ws(pos, &pos);
    if (*pos == '|')
        pos++;
    while (1) {
        p->rx->end = start;
        conjunction(p, pos, &pos);
        if (p->error)
            return -1;
        end = transition_state(p->rx->end, end, 0, NULL);
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

