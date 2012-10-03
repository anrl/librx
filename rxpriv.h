#ifndef __RXPRIV_H__
#define __RXPRIV_H__

#include "rx.h"

/* handy  */
#define INT_TO_POINTER(i) ((void *)(long)(i))
#define POINTER_TO_INT(p) ((int)(long)(p))
char *strdupf (const char *fmt, ...);

/* list  */
typedef struct List List;
struct List {
    void *data;
    List *next;
};

List *list_push      (List *list, void *data);
List *list_pop       (List *list, void *dump);
List *list_unshift   (List *list, void *data);
List *list_cat       (List *a, List *b);
void *list_last_data (List *list);
void *list_nth_data  (List *list, int n);
int   list_elems     (List *list);
List *list_copy      (List *list);
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
    EAT       = 1 << 0,
    CHAR      = 1 << 1,
    ANYCHAR   = 1 << 2,
    NEGCHAR   = 1 << 3,
    CHARCLASS = 1 << 4,
    CAPTURE   = 1 << 5
} TransitionType;

typedef struct {
    TransitionType type;
    State *to;
    State *back;
    char c;
    List *cc;
} Transition;

State      *state_new      (Rx *rx);
State      *state_split    (State *state);
void        state_free     (State *state);
Transition *transition_new (State *from, State *to);

/* assertions  */
int isword (int c);
int bos    (const char *str, const char *pos);
int bol    (const char *str, const char *pos);
int eos    (const char *str, const char *pos);
int eol    (const char *str, const char *pos);
int lwb    (const char *str, const char *pos);
int rwb    (const char *str, const char *pos);
int wb     (const char *str, const char *pos);
int nwb    (const char *str, const char *pos);

/* charclass  */
typedef enum {
    CC_EXCLUDES, CC_INCLUDES, CC_CHAR, CC_RANGE, CC_FUNC
} CharClassAction;

void char_class_free  (List *cc);
void char_class_print (List *cc);

/* parser  */
int rx_parse (Rx *rx);
int ws       (const char *pos, const char **fin);

/* rx  */
struct Rx {
    const char *regex;
    List       *extends;
    State      *start;
    State      *end;
    List       *states;
    List       *captures;
    List       *clusters;
    List       *subrules;
};

Rx *rx_extend (Rx *parent);

#endif

