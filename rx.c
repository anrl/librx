/* A regex implementation based on http://swtch.com/~rsc/regexp/regexp1.html,
Perl 5, and Perl 6. */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "rxpriv.h"

Rx *
rx_new (const char *regex) {
    Rx *rx = calloc(1, sizeof (Rx));
    rx->regex = regex;
    if (!rx_parse(rx)) {
        rx_free(rx);
        return NULL;
    }
    return rx;
}

void
rx_free (Rx *rx) {
    list_free(rx->states, state_free);
    list_free(rx->captures, rx_free);
    list_free(rx->clusters, rx_free);
    list_free(rx->subrules, rx_free);
    list_free(rx->extends, NULL);
    list_free(rx->charclasses, char_class_free);
    list_free(rx->quantifications, NULL);
    free(rx);
}

Rx *
rx_extend (Rx *parent) {
    Rx *rx = calloc(1, sizeof (Rx));
    rx->regex = parent->regex;
    rx->extends = list_push(rx->extends, parent);
    rx->end = rx->start = state_new(rx);
    return rx;
}

static int
set_index (void **set, int n, void *key) {
    int index = (int) key % n;
    while (set[index] && set[index] != key)
        index = (index + 1) % n;
    return index;
}

static void
set_insert (void **set, int n, void *key) {
    int index = set_index(set, n, key);
    set[index] = key;
}

static void *
set_lookup (void **set, int n, void *key) {
    int index = set_index(set, n, key);
    return set[index] == key ? key : NULL;
}

static void
rx_print_state (Rx *rx, State *state, int backwards, void **visited, int n) {
    List *elem;
    if (!state)
        return;
    if (set_lookup(visited, n, state))
        return;
    set_insert(visited, n, state);
    printf("\"%p\"", state);
    if (rx->start == state)
        printf(" [fillcolor=yellow,style=filled]");
    if (rx->end == state)
        printf(" [fillcolor=yellow,style=filled]");
    printf("\n");
    elem = backwards ? state->backtransitions : state->transitions;
    for (; elem; elem = elem->next) {
        Transition *t = elem->data;
        printf("\"%p\" -> \"%p\"", state, t->to);
        if (t->type & (CHAR | ANYCHAR) && isgraph(POINTER_TO_INT(t->param))) {
            printf(" [label=\"%c\"]", POINTER_TO_INT(t->param));
        }
        else if (t->type & CHARCLASS) {
            CharClass *cc = t->param;
            printf(" [label=\"%s%.*s\"]",
                cc->length == 2 ? "\\" : "", cc->length, cc->str);
        }
        else if (t->type & QUANTIFIED) {
            Quantified *q = t->param;
            printf(" [label=\"qfy %d..%d to %p\"]", q->min, q->max, t->ret);
        }
        else if (t->ret) {
            printf(" [label=\"return to %p\"]", t->ret);
        }
        printf("\n");
        rx_print_state(rx, t->to, backwards, visited, n);
        rx_print_state(rx, t->ret, backwards, visited, n);
    }
}

void
rx_print (Rx *rx, int backwards) {
    void *visited[4096] = {0};
    int n = sizeof visited / sizeof visited[0];
    State *start = backwards ? rx->end : rx->start;
    printf("digraph G {\n");
    rx_print_state(rx, start, backwards, visited, n);
    printf("}\n");
}

