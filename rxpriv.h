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
List *list_pop       (List *list, void *dump);
List *list_copy      (List *list);
List *list_cat       (List *a, List *b);
List *list_free      (List *list, void (*freefunc) ());
List *list_find      (List *list, void *data, int (*cmpfunc) ());
List *list_remove    (List *list, void *data, int (*cmpfunc) (),
                      void (*freefunc) ());

/* state  */
typedef struct {
    Rx   *group;
    List *transitions;
    int (*assertfunc) (const char *str, const char *pos);
} State;

typedef enum {
    NOCHAR,   /* gets you from one state to another but eats nothing  */
    ANYCHAR,  /* eats any char (.)  */
    CHAR,     /* eats one char  */
    NEGCHAR,  /* eats anything but a char  */
    CAPTURE,  /* goes to a capture  */
    CHARCLASS /* eats a char in a char class  */
} TransitionType;

typedef struct {
    int not;
    int (*isfunc) ();
    char *set;
} CharClass;

typedef struct {
    TransitionType type;
    State *to;
    State *back;
    char c;
    List *ccc; /* char class combo  */
} Transition;

State      *state_new      (Rx *rx);
void        state_free     (State *state);
Transition *transition_new (State *from, State *to);

/* cursor  */
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

/* matcher  */
int isword (int c);
int bos    (const char *str, const char *pos);
int bol    (const char *str, const char *pos);
int eos    (const char *str, const char *pos);
int eol    (const char *str, const char *pos);
int lwb    (const char *str, const char *pos);
int rwb    (const char *str, const char *pos);
int wb     (const char *str, const char *pos);
int nwb    (const char *str, const char *pos);

/* parser  */
struct Rx {
    List  *extends;
    char  *name;
    State *start;
    State *end;
    List  *states;
    List  *captures;
    List  *clusters;
    List  *subrules;
};

int ws (const char *pos, const char **fin);

/* rx  */
void rx_print (Rx *rx);

#endif

