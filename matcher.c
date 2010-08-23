#include <stdio.h>
#include <stdlib.h>
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
    List *paths;
    List *matches;
} Matcher;

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
    list_free(m->paths, path_unref);
    list_free(m->matches, path_unref);
    free(m);
}

/* Increments one particular path by a character and return a new list of
paths for it.  */
static List *
get_next_paths (Matcher *m, const char *pos, Path *path) {
    List *paths = NULL;
    List *telem;

    /* reference path for the duration of the function  */
    path->links++;

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
            paths = list_cat(paths, get_next_paths(m, pos, next));
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
        next = calloc(1, sizeof (Path));
        next->state = t->to;
        next->from = path;
        next->backs = list_copy(path->backs);
        path->links++;
        if (t->back)
            next->backs = list_push(next->backs, t->back);
        if (t->type == NOCHAR) {
            next->pos = pos;
            paths = list_cat(paths, get_next_paths(m, pos, next));
        }
        else {
            next->pos = pos + 1;
            paths = list_push(paths, next);
        }
    }
    path_unref(path);
    return paths;
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
    const char *pos = str;
    Matcher *m = calloc(1, sizeof (Matcher));
    Path *p = calloc(1, sizeof (Path));
    p->state = rx->start;
    p->pos = str;
    m->paths = list_push(m->paths, p);
    if (rx_debug)
        printf("matching against '%s'\n", str);
    while (1) {
        List *nextpaths = NULL;
        List *elem;
        for (elem = m->paths; elem; elem = elem->next) {
            Path *path = elem->data;
            nextpaths = list_cat(
                nextpaths,
                get_next_paths(m, pos, path));
        }
        list_free(m->paths, NULL);
        m->paths = nextpaths;
        if (rx_debug)
            paths_print(m, ++i, str);
        if (!m->paths)
            break;
        if (!*pos++)
            break;
    }
    retval = m->matches ? 1 : 0;
    if (rx_debug)
        printf(retval ? "It matched\n" : "No match\n");
    matcher_free(m);
    return retval;
}

