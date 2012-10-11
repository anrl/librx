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

/* charclass  */
typedef enum {
    CC_EXCLUDES, CC_INCLUDES, CC_CHAR, CC_NCHAR, CC_RANGE, CC_FUNC, CC_NFUNC
} CharClassAction;

typedef struct {
    const char *str;
    int length;
    List *actions;
} CharClass;

CharClass *char_class_new   (Rx *rx, const char *str, int length);
void       char_class_free  (CharClass *cc);
void       char_class_print (CharClass *cc);

/* state  */
typedef struct {
    Rx   *group;
    List *transitions;
    List *backtransitions;
    int (*assertfunc) (const char *str, const char *pos);
} State;

typedef enum {
    EAT        = 1 << 0,
    CHAR       = 1 << 1,
    ANYCHAR    = 1 << 2,
    NEGCHAR    = 1 << 3,
    CHARCLASS  = 1 << 4,
    CAPTUREREF = 1 << 5,
    QUANTIFIED = 1 << 6
} TransitionType;

typedef struct {
    State *to;
    State *ret;
    int type;
    void *param;
} Transition;

typedef struct {
    int min;
    int max;
} Quantified;

State      *state_new           (Rx *rx);
State      *state_split         (State *state);
void        state_free          (State *state);
Transition *transition_new      (State *from, State *to, State *ret,
                                 int type, void *param);
void        transition_free     (Transition *t);
State      *transition_state    (State *a, State *b, int type, void *param);
State      *transition_to_group (State *a, State *g, State *h,
                                 int type, void *param);
void        quantify            (State **a, State **b, int min, int max);

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
    List       *charclasses;
    List       *quantifications;
};

Rx *rx_extend (Rx *parent);

#endif

