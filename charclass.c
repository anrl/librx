#include <stdlib.h>
#include <stdio.h>
#include "rxpriv.h"

/*
A character class can now be a chain of assertions to make on a
character. For example, this character class:

<punct + alpha - [a..fxyz] - [,]>

Means that a character must be punctuation, or alphabetical, but not in
the range "a" through "f" or characters "x", "y", "z", and not a comma.

In the code, it is represented backwards, because that is the order
matching occurs in. In the above example, if its a comma, you can
immediately tell the character is not in the class. If its not a comma,
you check if its in the range "a" through "f", if it is, its not in
the character class. Then if its an alphabetic, it is contained. If
punctiation, it is, otherwise it is not a part of the character class.

The above, once parsed, will be a flat array like this:

    [CC_EXCLUDES, CC_CHAR, ',',
     CC_EXCLUDES, CC_RANGE, 'a', 'f', CC_CHAR, 'x', CC_CHAR, 'y', CC_CHAR, 'z',
     CC_INCLUDES, CC_FUNC, isalpha,
     CC_INCLUDES, CC_FUNC, ispunct]
*/

CharClass *
char_class_new (const char *str, int length) {
    CharClass *cc = calloc(1, sizeof (CharClass));
    cc->str = str;
    cc->length = length;
    return cc;
}

void
char_class_free (CharClass *cc) {
    if (cc)
        list_free(cc->actions, NULL);
    free(cc);
}

void
char_class_print (CharClass *cc) {
    printf("%.*s\n", cc->length, cc->str);
}

