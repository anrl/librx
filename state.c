#include <stdlib.h>
#include "rxpriv.h"

void
state_free (State *state) {
    list_free(state->transitions, free);
    free(state);
}

