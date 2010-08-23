#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "rxpriv.h"

char *
strdupf (const char *fmt, ...) {
    char *str;
    int size;
    va_list args;
    va_start(args, fmt);
    size = vsnprintf(NULL, 0, fmt, args) + 1;
    str = malloc(size);
    vsprintf(str, fmt, args);
    va_end(args);
    return str;
}

