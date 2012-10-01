#include <ctype.h>
#include "rxpriv.h"

int
isword (int c) {
    return isalnum(c) || c == '_' || c == '-';
}

int
bos (const char *str, const char *pos) {
    return pos == str;
}

int
bol (const char *str, const char *pos) {
    return bos(str, pos) || pos[-1] == '\n';
}

int
eos (const char *str, const char *pos) {
    return !*pos;
}

int
eol (const char *str, const char *pos) {
    return eos(str, pos) || *pos == '\n';
}

int
lwb (const char *str, const char *pos) {
    return (bos(str, pos) || !isword(pos[-1])) && isword(*pos);
}

int
rwb (const char *str, const char *pos) {
    return !bos(str, pos) && isword(pos[-1]) && !isword(*pos);
}

int
wb (const char *str, const char *pos) {
    return lwb(str, pos) || rwb(str, pos);
}

int
nwb (const char *str, const char *pos) {
    return !wb(str, pos);
}

