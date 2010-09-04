#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "rxpriv.h"

/* A path points to a place in the regex and the string keeping track of any
return addresses it needs when entering groups  */
typedef struct {
    const char *pos;
    State      *state;
    List       *backs;
} Path;

static Path *
path_new (const char *pos, State *state, List *backs) {
    Path *path = malloc(sizeof (Path));
    path->pos = pos;
    path->state = state;
    path->backs = list_copy(backs);
    return path;
}

static void
path_free (Path *path) {
    if (!path)
        return;
    list_free(path->backs, NULL);
    free(path);
}

static void
path_node_free_branch (Node *leaf) {
    node_free_branch(leaf, path_free);
}

/* A matcher keeps track of the current state of the match  */
typedef struct {
    Rx *rx;
    List *paths;
    List *next_paths;
    List *matches;
    const char *str;
    const char *startpos;
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

int
nwb (const char *str, const char *pos) {
    return !wb(str, pos);
}

static void
matcher_free (Matcher *m) {
    if (!m)
        return;
    list_free(m->next_paths, path_node_free_branch);
    list_free(m->paths, path_node_free_branch);
    list_free(m->matches, path_node_free_branch);
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
             cc->set && isincc(m, cc->set, c))
            match = cc->not ? 0 : 1;
        if (m->error)
            return -1;
    }
    return match;
}

/* Increments one particular path by a character and return a new list of
paths for it in m->next_paths.  */
static void
get_next_paths (Matcher *m, const char *pos, Node *pathnode) {
    List *elem;
    Path *path = pathnode->data;

    /* reference path node for the duration of the function  */
    pathnode->refs++;

    if (path->state->assertfunc) {
        if (!path->state->assertfunc(m->str, pos)) {
            path_node_free_branch(pathnode);
            return;
        }
    }
    if (!path->state->transitions) {
        if (path->backs) {
            /* leave group  */
            Node *nextnode;
            Path *next = path_new(pos, NULL, path->backs);
            next->backs = list_pop(next->backs, &next->state);
            nextnode = node_new(pathnode, next);
            get_next_paths(m, pos, nextnode);
            if (m->error)
                return;
        }
        else {
            /* end state  */
            pathnode->refs++;
            m->matches = list_push(m->matches, pathnode);
        }
    }
    for (elem = path->state->transitions; elem; elem = elem->next) {
        Transition *t = elem->data;
        Path *next;
        Node *nextnode;
        if (t->type == CHAR && t->c != *pos)
            continue;
        if (t->type == NEGCHAR && t->c == *pos)
            continue;
        if (t->type == CAPTURE && !t->to) {
            Rx *capture = list_nth_data(m->rx->captures, t->c);
            if (!capture) {
                m->error = strdupf("capture %d not found", t->c);
                return;
            }
            t->to = capture->start;
        }
        if (t->type == CHARCLASS && !isinccc(m, t->ccc, *pos))
            continue;
        if (m->error)
            return;
        next = path_new(pos, t->to, path->backs);
        nextnode = node_new(pathnode, next);
        if (t->back)
            next->backs = list_push(next->backs, t->back);
        if (t->type == NOCHAR || t->type == CAPTURE) {
            get_next_paths(m, pos, nextnode);
            if (m->error)
                return;
        }
        else {
            next->pos++;
            m->next_paths = list_push(m->next_paths, nextnode);
        }
    }
    if (--pathnode->refs <= 0)
        path_node_free_branch(pathnode);
}

static void
matcher_print (Matcher *m, int i) {
    List *elem;
    printf("iter %d\n", i);
    for (elem = m->paths; elem; elem = elem->next) {
        Node *node = elem->data;
        Path *path = node->data;
        printf("path '%.*s'\n", path->pos - m->startpos, m->startpos);
    }
    for (elem = m->matches; elem; elem = elem->next) {
        Node *node = elem->data;
        Path *path = node->data;
        printf("match '%.*s'\n", path->pos - m->startpos, m->startpos);
    }
}

/* find all the ways in which the string can match the given regex */
static Matcher *
get_all_matches (Rx *rx, const char *str) {
    Matcher *m;
    const char *startpos = str;
    while (1) {
        int i = 0;
        const char *pos = startpos;
        Node *root = node_new(NULL, path_new(pos, rx->start, NULL));
        m = calloc(1, sizeof (Matcher));
        m->paths = list_push(m->paths, root);
        m->str = str;
        m->startpos = startpos;
        m->rx = rx;
        while (1) {
            List *elem;
            for (elem = m->paths; elem; elem = elem->next) {
                Node *path = elem->data;
                get_next_paths(m, pos, path);
                if (m->error) {
                    fprintf(stderr, "%s\n", m->error);
                    matcher_free(m);
                    return NULL;
                }
            }
            list_free(m->paths, NULL);
            m->paths = m->next_paths;
            m->next_paths = NULL;
            if (rx_debug)
                matcher_print(m, ++i);
            if (!m->paths)
                break;
            if (!*pos++)
                break;
        }
        if (pos == str && rx->start->assertfunc == bos)
            break;
        if (m->matches)
            break;
        if (!*startpos++)
            break;
        matcher_free(m);
    }
    list_free(m->paths, path_node_free_branch);
    m->paths = NULL;
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
    retval = m && m->matches ? 1 : 0;
    matcher_free(m);
    if (rx_debug)
        printf(retval ? "It matched\n" : "No match\n");
    return retval;
}

