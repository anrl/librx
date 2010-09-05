#include "rxpriv.h"
#include <stdio.h>
#include <stdlib.h>

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


