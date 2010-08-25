#ifndef __RXPRIV_H__
#define __RXPRIV_H__

#include "rx.h"

/* handy  */
char *strdupf (const char *fmt, ...);

/* list  */
typedef struct List List;

struct List {
    void *data;
    List *next;
};

List *list_push      (List *list, void *data);
void *list_last_data (List *list);
void *list_nth_data  (List *list, int n);
int   list_elems     (List *list);
List *list_pop       (List *list);
List *list_copy      (List *list);
List *list_cat       (List *a, List *b);
void  list_free      (List *list, void (*freefunc) ());
List *list_find      (List *list, void *data, int (*cmpfunc) ());
List *list_remove    (List *list, void *data, int (*cmpfunc) (),
                      void (*freefunc) ());

/* state  */
typedef struct {
    List *transitions;
    Rx *group;
} State;

typedef enum {
    NOCHAR,   /* gets you from one state to another but eats nothing  */
    ANYCHAR,  /* eats any char (.)  */
    CHAR,     /* eats one char  */
    NEGCHAR   /* eats anything but a char  */
} TransitionType;

typedef struct {
    TransitionType type;
    State *to;
    State *back;
    char c;
} Transition;

State *state_new (Rx *rx);
void state_free (State *state);
Transition *transition_new (State *from, State *to);

/* parser  */
struct Rx {
    List *extends;
    char *name;
    State *start;
    State *end;
    List *states;
    List *captures;
    List *clusters;
    List *subrules;
};

/* rx  */
void rx_print (Rx *rx);

#endif

