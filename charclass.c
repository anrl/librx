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

void
char_class_free (List *cc) {
    list_free(cc, NULL);
}

void
char_class_print (List *cc) {
    List *elem;
    for (elem = cc; elem; elem = elem->next) {
        if (elem->data == (void *) CC_EXCLUDES) {
            printf("excludes\n");
        }
        else if (elem->data == (void *) CC_INCLUDES) {
            printf("includes\n");
        }
        else if (elem->data == (void *) CC_CHAR) {
            elem = elem->next;
            printf("char %c\n", (char) elem->data);
        }
        else if (elem->data == (void *) CC_RANGE) {
            elem = elem->next;
            printf("range %c", (char) elem->data);
            elem = elem->next;
            printf(" -> %c\n", (char) elem->data);
        }
        else if (elem->data == (void *) CC_FUNC) {
            elem = elem->next;
            printf("func %p\n", elem->data);
        }
        else {
            printf("unknown %p\n", elem->data);
        }
    }
    printf("---\n");
}

