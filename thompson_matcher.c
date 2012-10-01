#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "rxpriv.h"

/* This file is no longer used. I tried to get parallel matching to work,
but when it needed to match greedy vs nongreedy, I gave up. To do it
you have to wait until the match is over, and gothrough all possible
matches to see who matched the right atoms greedily or not, and thats
too hard to do. backtracking is much easier. */

typedef struct Cursor Cursor;
struct Cursor {
    Cursor     *parent;
    List       *children;
    int         refs;
    const char *pos;
    State      *state;
    List       *backs;
};

Cursor *cursor_new         (Cursor *parent, const char *pos, State *state,
                            List *backs);
void    cursor_free        (Cursor *c);
void    cursor_free_branch (Cursor *c);

Cursor *
cursor_new (Cursor *parent, const char *pos, State *state, List *backs) {
    Cursor *c = calloc(1, sizeof (Cursor));
    c->parent = parent;
    if (parent) {
        parent->children = list_push(parent->children, c);
        parent->refs++;
    }
    c->pos = pos;
    c->state = state;
    c->backs = list_copy(backs);
    return c;
}

void
cursor_free (Cursor *c) {
    Cursor *parent;
    if (!c)
        return;
    list_free(c->children, cursor_free);
    parent = c->parent;
    if (parent) {
        parent->children = list_remove(parent->children, c, NULL, NULL);
        parent->refs--;
    }
    list_free(c->backs, NULL);
    free(c);
}

void
cursor_free_branch (Cursor *c) {
    Cursor *parent;
    if (!c)
        return;
    parent = c->parent;
    c->refs--;
    if (c->refs > 0)
        return;
    cursor_free(c);
    if (parent && parent->refs <= 0)
        cursor_free_branch(parent);
}


/* A matcher keeps track of the current state of the match  */
typedef struct {
    Rx *rx;
    List *cursors;
    List *matches;
    const char *str;
    const char *startpos;
    const char *pos;
    char *error;
} Matcher;

static Matcher *
matcher_new (Rx *rx, const char *str) {
    Matcher *m = calloc(1, sizeof (Matcher));
    m->rx = rx;
    m->str = m->startpos = m->pos = str;
    return m;
}

static void
matcher_prep (Matcher *m) {
    Cursor *root;
    if (!m)
        return;
    m->cursors = list_free(m->cursors, cursor_free_branch);
    m->matches = list_free(m->matches, cursor_free_branch);
    m->pos = m->startpos;
    root = cursor_new(NULL, m->pos, m->rx->start, NULL);
    m->cursors = list_push(m->cursors, root);
}

static void
matcher_free (Matcher *m) {
    if (!m)
        return;
    list_free(m->cursors, cursor_free_branch);
    list_free(m->matches, cursor_free_branch);
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
isincc (Matcher *m, const char *set, char c) {
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

/* Match a character class combo such as <punct + alpha - [a..f] - [,]>  */
static int
isinccc (Matcher *m, List *ccc, char c) {
    int retval;
    CharClass *cc;
    List *elem;
    if (!ccc)
        return 0;
    cc = ccc->data;
    retval = cc->not;
    for (elem = ccc; elem; elem = elem->next) {
        cc = elem->data;
        if (!cc->set && cc->isfunc(c) ||
             cc->set && isincc(m, cc->set, c))
            retval = cc->not ? 0 : 1;
        if (m->error)
            return -1;
    }
    return retval;
}

static void
advance_cursor (Matcher *m, Cursor *cursor) {
    List *elem;
    cursor->refs++;
    if (cursor->state->assertfunc) {
        if (!cursor->state->assertfunc(m->str, m->pos)) {
            cursor_free_branch(cursor);
            return;
        }
    }
    if (!cursor->state->transitions) {
        if (cursor->backs) {
            /* leave group  */
            Cursor *next = cursor_new(cursor, m->pos, NULL, cursor->backs);
            next->backs = list_pop(next->backs, &next->state);
            advance_cursor(m, next);
            if (m->error)
                return;
        }
        else {
            /* end state  */
            cursor->refs++;
            m->matches = list_push(m->matches, cursor);
        }
    }
    for (elem = cursor->state->transitions; elem; elem = elem->next) {
        Transition *t = elem->data;
        Cursor *next;
        if (t->type & CHAR && t->c != *m->pos)
            continue;
        if (t->type & NEGCHAR && t->c == *m->pos)
            continue;
        if (t->type & CAPTURE && !t->to) {
            Rx *capture = list_nth_data(m->rx->captures, t->c);
            if (!capture) {
                m->error = strdupf("capture %d not found", t->c);
                return;
            }
            t->to = capture->start;
        }
        if (t->type & CHARCLASS && !isinccc(m, t->ccc, *m->pos))
            continue;
        if (m->error)
            return;
        next = cursor_new(cursor, m->pos, t->to, cursor->backs);
        if (t->back)
            next->backs = list_push(next->backs, t->back);
        if (t->type & EAT) {
            next->pos++;
            m->cursors = list_push(m->cursors, next);
        }
        else {
            advance_cursor(m, next);
            if (m->error)
                return;
        }
    }
    cursor_free_branch(cursor);
}

static void
matcher_print (Matcher *m) {
    List *elem;
    for (elem = m->cursors; elem; elem = elem->next) {
        Cursor *cursor = elem->data;
        printf("cursor '%.*s'\n", cursor->pos - m->startpos, m->startpos);
    }
    for (elem = m->matches; elem; elem = elem->next) {
        Cursor *match = elem->data;
        printf("match '%.*s'\n", match->pos - m->startpos, m->startpos);
    }
}

/* find all the ways in which the string can match the given regex */
static Matcher *
get_all_matches (Rx *rx, const char *str) {
    Matcher *m = matcher_new(rx, str);
    while (1) {
        int i = 0;
        matcher_prep(m);
        while (1) {
            List *elem;
            List *cursors = m->cursors;
            m->cursors = NULL;
            for (elem = cursors; elem; elem = elem->next) {
                Cursor *cursor = elem->data;
                advance_cursor(m, cursor);
                if (m->error)
                    return NULL;
            }
            list_free(cursors, NULL);
            if (rx_debug) {
                printf("iter %d\n", ++i);
                matcher_print(m);
            }
            if (!m->cursors)
                break;
            if (!*m->pos++)
                break;
        }
        if (m->pos == m->str && rx->start->assertfunc == bos)
            break;
        if (m->matches)
            break;
        if (!*m->startpos++)
            break;
    }
    return m;
}

/* TODO rewrite this comment
A match traverses the nfa by storing a list of paths. A path is a data
structure that points to a particular state and position in the string being
matched. It also keeps track of where it came from and how many paths branch
from it. So for any given path, it can be figured out how it matched.

Paths are pruned if it has no where to go and is not in the end state.
A Path is moved into the matches list if its in the end state.

A match is over when the string reaches the end or there are no more paths.  */
int
rx_match (Rx *rx, const char *str) {
    int retval;
    Matcher *m;
    if (rx_debug)
        printf("matching against '%s'\n", str);
    m = get_all_matches(rx, str);
    if (m->error)
        fprintf(stderr, "%s\n", m->error);
    retval = m && m->matches ? 1 : 0;
    matcher_free(m);
    if (rx_debug)
        printf(retval ? "It matched\n" : "No match\n");
    return retval;
}

