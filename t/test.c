#include <stdio.h>
#include <stdarg.h>
#include <tap.h>
#include "../rx.h"

#define rx_like(...)   rx_like_at_loc(1, __FILE__, __LINE__, __VA_ARGS__, NULL)
#define rx_unlike(...) rx_like_at_loc(0, __FILE__, __LINE__, __VA_ARGS__, NULL)

int
rx_like_at_loc (int for_match, const char *file, int line, const char *got,
                const char *expected, const char *fmt, ...)
{
    va_list args;
    int test;
    Rx *rx = rx_new(expected);
    if (!rx)
        exit(255);
    test = rx_match(rx, got) ^ !for_match;
    rx_free(rx);
    va_start(args, fmt);
    vok_at_loc(file, line, test, fmt, args);
    va_end(args);
    if (!test) {
        diag("    %13s  '%s'", "", got);
        diag("    %13s: '%s'",
            for_match ? "doesn't match" : "matches", expected);
    }
    return test;
}

int
main () {
    rx_like   ("abcd", "", "empty regex always matches");
    rx_like   ("a", "a", "one character match");
    rx_unlike ("a", "b", "one character no match");
    rx_like   ("frob", "frob", "multichar match");
    rx_unlike ("frob", "nicate", "multichar no match");
    rx_like   ("abbbbc", "ab*c", "* repetition");
    rx_unlike ("abbbbxc", "ab*c", "* repetition no match");
    rx_like   ("abbbbc", "ab+c", "+ repetition");
    rx_unlike ("ac", "ab+c", "+ repetition no match");
    rx_like   ("abc", "ab?c", "? repetition");
    rx_unlike ("abbc", "ab?c", "? repetition no match");
    rx_like   ("abcd", ".", "any char");
    rx_unlike ("", ".", "any char no match");
    rx_like   ("abcd", "...", "multiple any chars");
    rx_unlike ("abcd", ".....", "multiple any chars no match");
    rx_like   ("they just stand back", "they.*just.*stand.*back", "dot star separated");
    rx_like   ("ab", "      a       b     ", "whitespace has no effect");
    rx_like   ("abbbbb", "(ab)*(ab*)*", "grouping");
    rx_unlike ("abbbbb", "(ac)+", "grouping no match");
    rx_like   ("elephant", "cat|dog|elephant|kangaroo", "ahhhh elephante!");
    rx_unlike ("elephant", "cat|dog|wolf|kangaroo", "elephante?");
    rx_like   ("hoothoothoot", "(hoot)<0>+", "subpattern");
    rx_unlike ("hootasdf", "(hoot)<0>+", "subpattern no match");
    rx_like   ("ExxExxx3E33", "(E(x)*<0>*3)", "recursive subpattern");
    rx_like   ("file.txt", "file\\.txt", "escape");
    rx_unlike ("file~txt", "file\\.txt", "escape no match");
    rx_like   ("kupo! kupo!", "kupo\\!\\ kupo\\!", "escape bangs and spaces");
    rx_like   ("Do you\nremember me?", "Do \\  you \\n remember \\  me \\?", "escape newline");
    rx_like   ("Wark!", "\\N\\T\\N+", "negated character");
    rx_unlike ("Kw\neh!", "\\N\\T\\N+", "negated character no match");
    rx_like   ("The world is veiled in darkness.", "'The world is'", "single quotes");
    rx_like   ("***Weezy blog***", "'***Weezy blog***'", "yet another single quotes");
    rx_unlike ("nothing like it", "'like this'", "single quotes no match");
    rx_like   ("The <wind> stops", "....\"<wind>\"", "double quotes");
    rx_like   ("foobar", "  \
        foo                 \
        [                   \
        | foo               \
        | bar               \
        | baz               \
        | quux              \
        | thud              \
        ]                   \
    ", "cluster");
    return exit_status();
}

