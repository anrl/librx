#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "rxpriv.h"

typedef struct Path Path;
struct Path {
    State *state;
    Path *from;
    const char *pos;
    int links;
    List *backs;
};

/* A matcher keeps track of the current state of the match  */
typedef struct {
    Rx *rx;
    List *paths;
    List *next_paths;
    List *matches;
    const char *str;
    char *error;
} Matcher;

int
isword (int c) {
    return isalnum(c) || c == '_' || c == '-';
}

int
bos (const char *str, const char *pos) {
    return pos == str;
}

int
bol (const char *str, const char *pos) {
    return bos(str, pos) || pos[-1] == '\n';
}

int
eos (const char *str, const char *pos) {
    return !*pos;
}

int
eol (const char *str, const char *pos) {
    return eos(str, pos) || *pos == '\n';
}

int
lwb (const char *str, const char *pos) {
    return (bos(str, pos) || !isword(pos[-1])) && isword(*pos);
}

int
rwb (const char *str, const char *pos) {
    return !bos(str, pos) && isword(pos[-1]) && !isword(*pos);
}

int
wb (const char *str, const char *pos) {
    return lwb(str, pos) || rwb(str, pos);
}

static void
path_unref (Path *path) {
    if (!path)
        return;
    if (--path->links <= 0) {
        path_unref(path->from);
        list_free(path->backs, NULL);
        free(path);
    }
}

static void
matcher_free (Matcher *m) {
    if (!m)
        return;
    list_free(m->next_paths, path_unref);
    list_free(m->paths, path_unref);
    list_free(m->matches, path_unref);
    free(m->error);
    free(m);
}

static int
ccatom (const char *pos, const char **fin, char *atom) {
    /* ccatom: '\' <[\[\]\ \\nrt]> | <-space>  */
    if (pos[0] == '\\' && (
        pos[1] == '[' || pos[1] == ']' || pos[1] == ' ' || pos[1] == '\\'))
    {
        *atom = pos[1];
        pos += 2;
    }
    else if (pos[0] == '\\' && pos[1] == 'n') {
        *atom = '\n';
        pos += 2;
    }
    else if (pos[0] == '\\' && pos[1] == 'r') {
        *atom = '\r';
        pos += 2;
    }
    else if (pos[0] == '\\' && pos[1] == 't') {
        *atom = '\t';
        pos += 2;
    }
    else if (*pos && !isspace(*pos)) {
        *atom = pos[0];
        pos++;
    }
    else {
        return 0;
    }
    *fin = pos;
    return 1;
}

/* returns true if c is in the character class given in set  */
static int
isincc (Matcher *m, char c, const char *set) {
    /* set: <ccatom> '..' <ccatom> | <ccatom>  */
    char atom1, atom2;
    const char *fin;
    while (1) {
        ws(set, &set);
        if (ccatom(set, &fin, &atom1) &&
            ws(fin, &fin) &&
            !strncmp(fin, "..", 2) &&
            ws(fin + 2, &fin) &&
            ccatom(fin, &fin, &atom2))
        {
            if (atom2 <= atom1) {
                m->error = strdupf("invalid char class range at '%s'", set);
                return -1;
            }
            if (c >= atom1 && c <= atom2)
                return 1;
            set = fin;
        }
        else if (ccatom(set, &fin, &atom1)) {
            if (c == atom1)
                return 1;
            set = fin;
        }
        else if (*set) {
            m->error = strdupf("invalid char class syntax at '%s'", set);
            return -1;
        }
        else
            break;
    }
    return 0;
}

/* Match a character class combo such as <punct + alpha - [a..f] - [\,]> */
static int
ccc_match (Matcher *m, List *ccc, char c) {
    int match;
    CharClass *cc;
    List *elem;
    if (!ccc)
        return 0;
    cc = ccc->data;
    match = cc->not;
    for (elem = ccc; elem; elem = elem->next) {
        cc = elem->data;
        if (!cc->set && cc->isfunc(c) ||
             cc->set && isincc(m, c, cc->set))
            match = cc->not ? 0 : 1;
        if (m->error)
            return -1;
    }
    return match;
}

/* Increments one particular path by a character and return a new list of
paths for it in m->next_paths.  */
static void
get_next_paths (Matcher *m, const char *pos, Path *path) {
    List *telem;

    /* reference path for the duration of the function  */
    path->links++;

    if (path->state->assertfunc) {
        if (!path->state->assertfunc(m->str, pos)) {
            path_unref(path);
            return;
        }
    }
    if (!path->state->transitions) {
        if (path->backs) {
            /* leave group  */
            Path *next = calloc(1, sizeof (Path));
            State *back;
            next->backs = list_copy(path->backs);
            back = list_last_data(next->backs);
            next->backs = list_pop(next->backs);
            next->state = back;
            next->pos = pos;
            next->from = path;
            path->links++;
            get_next_paths(m, pos, next);
            if (m->error)
                return;
        }
        else {
            /* end state  */
            path->links++;
            m->matches = list_push(m->matches, path);
        }
    }
    for (telem = path->state->transitions; telem; telem = telem->next) {
        Transition *t = telem->data;
        Path *next;
        if (t->type == CHAR && t->c != *pos)
            continue;
        if (t->type == NEGCHAR && t->c == *pos)
            continue;
        if (t->type == CLUSTER && !t->to) {
            Rx *capture = list_nth_data(m->rx->captures, t->c);
            if (!capture) {
                m->error = strdupf("capture %d not found", t->c);
                return;
            }
            t->to = capture->start;
        }
        if (t->type == CHARCLASS && !ccc_match(m, t->ccc, *pos))
            continue;
        if (m->error)
            return;
        next = calloc(1, sizeof (Path));
        next->state = t->to;
        next->from = path;
        next->backs = list_copy(path->backs);
        path->links++;
        if (t->back)
            next->backs = list_push(next->backs, t->back);
        if (t->type == NOCHAR || t->type == CLUSTER) {
            next->pos = pos;
            get_next_paths(m, pos, next);
            if (m->error)
                return;
        }
        else {
            next->pos = pos + 1;
            m->next_paths = list_push(m->next_paths, next);
        }
    }
    path_unref(path);
}

static void
paths_print (Matcher *m, int i, const char *str) {
    List *elem;
    printf("iter %d\n", i);
    for (elem = m->paths; elem; elem = elem->next) {
        Path *path = elem->data;
        printf("path '%.*s'\n", path->pos - str, str);
    }
    for (elem = m->matches; elem; elem = elem->next) {
        Path *path = elem->data;
        printf("match '%.*s'\n", path->pos - str, str);
    }
}

/* A match traverses the nfa by storing a list of paths. A path is a data
structure that points to a particular state and position in the string being
matched. It also keeps track of where it came from and how many paths branch
from it. So for any given path, it can be figured out how it matched.

Paths are pruned if it has no where to go and is not in the end state.
A Path is moved into the matches list if its in the end state.

A match is over when the string reaches the end or there are no more paths.  */
int
rx_match (Rx *rx, const char *str) {
    int retval;
    int i = 0;
    Matcher *m;
    const char *startpos = str;
    if (rx_debug)
        printf("matching against '%s'\n", str);
    while (1) {
        const char *pos = startpos;
        Path *p = calloc(1, sizeof (Path));
        p->state = rx->start;
        p->pos = pos;
        m = calloc(1, sizeof (Matcher));
        m->paths = list_push(m->paths, p);
        m->str = str;
        m->rx = rx;
        while (1) {
            List *elem;
            for (elem = m->paths; elem; elem = elem->next) {
                Path *path = elem->data;
                get_next_paths(m, pos, path);
                if (m->error) {
                    fprintf(stderr, "%s\n", m->error);
                    matcher_free(m);
                    return 0;
                }
            }
            list_free(m->paths, NULL);
            m->paths = m->next_paths;
            m->next_paths = NULL;
            if (rx_debug)
                paths_print(m, ++i, str);
            if (!m->paths)
                break;
            if (!*pos++)
                break;
        }
        retval = m->matches ? 1 : 0;
        matcher_free(m);
        if (pos == str && rx->start->assertfunc == bos)
            break;
        if (retval)
            break;
        if (!*startpos++)
            break;
    }
    if (rx_debug)
        printf(retval ? "It matched\n" : "No match\n");
    return retval;
}

