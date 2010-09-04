#include "rxpriv.h"
#include <stdio.h>
#include <stdlib.h>

Node *
node_new (Node *parent, void *data) {
    Node *node = calloc(1, sizeof (Node));
    node->data = data;
    node->parent = parent;
    if (parent) {
        parent->children = list_push(parent->children, node);
        parent->refs++;
    }
    return node;
}

void
node_free (Node *leaf, void (*freefunc) ()) {
    Node *parent;
    if (!leaf)
        return;
    parent = leaf->parent;
    if (parent) {
        parent->children = list_remove(parent->children, leaf, NULL, NULL);
        parent->refs--;
    }
    if (freefunc)
        freefunc(leaf->data);
    free(leaf);
}

void
node_free_branch (Node *leaf, void (*freefunc) ()) {
    Node *parent;
    if (!leaf)
        return;
    parent = leaf->parent;
    node_free(leaf, freefunc);
    if (parent && parent->refs <= 0)
        node_free_branch(parent, freefunc);
}

