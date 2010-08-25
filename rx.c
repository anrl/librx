/* A regex implementation based on http://swtch.com/~rsc/regexp/regexp1.html,
Perl 5, and Perl 6. */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "rxpriv.h"

void
rx_free (Rx *rx) {
    list_free(rx->states, state_free);
    list_free(rx->captures, rx_free);
    list_free(rx->clusters, rx_free);
    list_free(rx->subrules, rx_free);
    free(rx);
}

void
rx_extends (Rx *rx, Rx *parent) {
    rx->extends = list_push(rx->extends, parent);
}

void
rx_print (Rx *rx) {
    List *elem;
    int level = 0;
    for (elem = rx->states; elem; elem = elem->next) {
        State *state = elem->data;
        if (!state->transitions && !level)
            printf("end ");
        printf("state at %p\n", state);
        {
            List *elem;
            for (elem = state->transitions; elem; elem = elem->next) {
                Transition *t = elem->data;
                printf("    transition ");
                if ((t->type == CHAR || t->type == ANYCHAR) && isgraph(t->c))
                    printf("'%c' ", t->c);
                printf("to %p\n", t->to);
                if (t->back)
                    level++;
            }
        }
    }
}

/*
hmmm... case where longest isnt greedy
'abbbbb' =~ /(ab)*(ab*)* /

All possible matches will be found regarding greedy and non greedy.

Grouping will cause the creation of a new regex and the nfa will only reference
it. That way thay can be reused either by number or name later in the regex. 
This allows for matching things like balanced parentheses.
*/

