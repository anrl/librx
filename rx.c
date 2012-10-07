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
rx_print_state (Rx *rx, State *state, void **visited, int n) {
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
    for (elem = state->transitions; elem; elem = elem->next) {
        Transition *t = elem->data;
        printf("\"%p\" -> \"%p\"", state, t->to);
        if (t->type & (CHAR | ANYCHAR) && isgraph(t->c))
            printf(" [label=\"%c\"]", t->c);
        if (t->type & CHARCLASS)
            printf(" [label=\"%s%.*s\"]",
                t->cc->length == 2 ? "\\" : "", t->cc->length, t->cc->str);
        if (t->ret)
            printf(" [color=blue,style=dotted,label=\"return to %p\"]", t->ret);
        printf("\n");
        rx_print_state(rx, t->to, visited, n);
        rx_print_state(rx, t->ret, visited, n);
    }
}

void
rx_print (Rx *rx) {
    void *visited[4096] = {0};
    printf("digraph G {\n");
    rx_print_state(rx, rx->start, visited, sizeof visited / sizeof visited[0]);
    printf("}\n");
}

/*
hmmm... case where longest isnt greedy
'abbbbb' =~ /(ab)*(ab*)* /

All possible matches will be found regarding greedy and non greedy.

Grouping will cause the creation of a new regex and the nfa will only reference
it. That way thay can be reused either by number or name later in the regex.
This allows for matching things like balanced parentheses.

Im not sure what to do about empty atoms '', "", <?> etc. If quantified (''*)
they will cause an infinite loop which the matcher currently cant get out of.
Perhaps if I keep track of the min number of characters a group or atom can
match and ignore quantifiers if 0. Same issue with (a*)*. I believe Perl keeps
track of what min length each thing can match to figure its way out of this
case.
*/

