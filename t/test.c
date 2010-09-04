#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <tap.h>
#include "../rxpriv.h"

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

int *
int_new (int x) {
    int *i = malloc(sizeof (int));
    *i = x;
    return i;
}

int
main () {
    rx_like   ("chapter-55, page-44, line-33",
               "([chapter|page|line] - <digit>+) [',' \\s* <~~0>] ** 1..2",
               "synopsis");
    rx_like   ("abcd", "", "empty regex always matches");
    rx_like   ("a", "a", "one character match");
    rx_unlike ("a", "b", "fail one character");
    rx_like   ("frob", "frob", "multichar match");
    rx_unlike ("frob", "nicate", "fail multichar");
    rx_like   ("abbbbc", "ab*c", "* quantifier");
    rx_unlike ("abbbbxc", "ab*c", "fail * quantifier");
    rx_like   ("abbbbc", "ab+c", "+ quantifier");
    rx_unlike ("ac", "ab+c", "fail + quantifier");
    rx_like   ("abc", "ab?c", "? quantifier");
    rx_unlike ("abbc", "ab?c", "fail ? quantifier");
    rx_like   ("abcd", ".", "any char");
    rx_unlike ("", ".", "fail any char");
    rx_like   ("abcd", "...", "multiple any chars");
    rx_unlike ("abcd", ".....", "fail multiple any chars");
    rx_like   ("they just stand back", "they.*just.*stand.*back", "dot star separated");
    rx_like   ("ab", "      a       b     ", "whitespace has no effect");
    rx_like   ("abbbbb", "(ab)*(ab*)*", "grouping");
    rx_unlike ("abbbbb", "(ac)+", "fail grouping");
    rx_like   ("elephant", "cat|dog|elephant|kangaroo", "ahhhh elephante!");
    rx_unlike ("elephant", "cat|dog|wolf|kangaroo", "elephante?");
    rx_like   ("hoothoothoot", "(hoot)<~~0>+", "subpattern");
    rx_unlike ("hootasdf", "(hoot)<~~0>+", "fail subpattern");
    rx_like   ("ExxExxx3E33", "(E(x)*<~~0>*3)", "recursive subpattern");
    rx_like   ("file.txt", "file\\.txt", "escape");
    rx_unlike ("file~txt", "file\\.txt", "fail escape");
    rx_like   ("kupo! kupo!", "kupo\\!\\ kupo\\!", "escape bangs and spaces");
    rx_like   ("Do you\nremember me?", "Do \\  you \\n remember \\  me \\?", "escape newline");
    rx_like   ("Wark!", "\\N\\T\\N+", "negated character");
    rx_unlike ("Kw\neh!", "^\\N\\T\\N+", "fail negated character");
    rx_like   ("The world is veiled in darkness.", "'The world is'", "single quotes");
    rx_like   ("***Weezy blog***", "'***Weezy blog***'", "yet another single quotes");
    rx_unlike ("nothing like it", "'like this'", "fail single quotes");
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
    rx_like   ("dark peak", "<~~1><~~0><~~1><~~0> ' ' <~~0><~~0>(p|e|a|k)(d|a|r|k)", "match time capture resolution");
    rx_like   ("foofoofoo", "foo<~~>*", "overall capture match");
    skip(1, 1, "errors should not be sent to stdout by default");
    rx_unlike ("aabb", "a<~~0>", "match error: capture 0 doesn't exist");
    endskip;
    rx_like   ("R", "<[A..Z]>", "char class");
    rx_unlike ("r", "<[A..Z]>", "fail char class");
    rx_like   ("overall the same", "<alpha>+<space>+<alpha>+", "named char classes");
    rx_unlike ("overall the same", "<upper>+<space>+<upper>+", "fail named char classes");
    rx_like   ("couldn't see the future", "<[c..u]>*<[\\']>", "backslash in char class");
    rx_like   ("Thats-a-right I'm Don", "<alpha + [-]>+ ' ' I\\'m", "char class combo");
    rx_unlike ("Thats-a-right I'm Don", "^ <alpha + [_]>+ ' ' I\\'m", "fail char class combo");
    rx_like   (":;:", "<[:;]><[:;]><[:;]>", "cc doesnt needs many escapes");
    rx_like   ("[nightmares]", "'[' <-[\\]]>* ']'", "match brackets with char class");
    rx_like   ("abcabcabcabcd", "'abc'**4", "fixed exact repetition");
    rx_like   ("abcabcabcabcd", "'abc'   **    4", "fixed exact repetition with space");
    rx_unlike ("abcabcabcabcd", "'abc' ** 5", "fail fixed exact repetition");
    rx_like   ("abcabcabcabcd", "'abc' ** 2..4", "fixed range repetition");
    rx_unlike ("abc", "'abc'**2..4", "fail fixed range repetition");
    rx_like   ("abcabcabcabcd", "'abc' ** 2..*", "open range repetition");
    rx_unlike ("abcd", "'abc' ** 2..*", "fail open range repetition");
    rx_like   ("ab  cdef", "ab\\s+cdef", "whitespace");
    rx_unlike ("abcdef", "ab\\s+cdef", "fail whitespace");
    rx_like   ("abcdef", "a\\S+f", "non whitespace");
    rx_unlike ("ab  cdef", "ab\\S+cdef", "fail non whitespace");
    rx_like   ("abcdef", "a\\w+f", "word character");
    rx_unlike ("a=[ *f", "a\\w+f", "fail word character");
    rx_like   ("a&%;' f", "a\\W+f", "non word character");
    rx_unlike ("abcdef", "a\\W+f", "fail non word character");
    rx_like   ("ab42cdef", "ab\\d+cdef", "digit");
    rx_unlike ("abcdef", "a\\d+f", "fail digit");
    rx_like   ("abcdef", "a\\D+f", "non digit");
    rx_unlike ("ab0cdef", "a\\D+f", "fail non digit");
    rx_like   ("abcdef", "^ abc", "bos");
    rx_unlike ("abcdef", "^ def", "fail bos");
    rx_unlike ("def\nabc", "^ abc", "bos not bol");
    rx_unlike ("def\nabc", "def \\n ^ abc", "yet another bos not bol");
    rx_like   ("abcdef", "def $", "eos");
    rx_unlike ("def\nabc", "def $", "fail eos");
    rx_like   ("abc\ndef", "^^ abc \\n ^^ def", "bol");
    rx_like   ("\n", "^^ \\n", "bol just newline");
    rx_like   ("abc\ndef", "abc $$ \\n def $$", "eol");
    rx_like   ("\n", "$$ \\n", "eol just newline");
    rx_like   ("abc def", "<<def", "left word boundary");
    rx_like   ("abc def", "<<abc", "left word boundary bos");
    rx_unlike ("abc def", "<<bc", "fail left word boundary");
    rx_unlike ("abc def", "c<<", "fail left word boundary");
    rx_unlike (":::::::", "<<", "fail left word boundary no word chars");
    rx_like   ("abc def", "c>>", "right word boundary");
    rx_like   ("abc def", "def>>", "right word boundary");
    rx_unlike ("abc def", ">>abc", "fail right word boundary");
    rx_unlike ("abc def", ">>bc", "fail right word boundary mid-word");
    rx_unlike (":::::::", ">>", "fail right word boundary no word chars");
    rx_like   ("abc\ndef\n-==\nghi", "\\b def", "word boundary \\W\\w");
    rx_like   ("abc\ndef\n-==\nghi", "abc \\b", "word boundary \\w\\W");
    rx_like   ("abc\ndef\n-==\nghi", "\\b abc", "bos word boundary");
    rx_like   ("abc\ndef\n-==\nghi", "ghi \\b", "eos word boundary");
    rx_unlike ("abc\ndef\n-==\nghi", "a \\b", "fail \\w\\w word boundary");
    rx_unlike ("abc\ndef\n-==\nghi", "\\= \\b", "fail \\W\\W word boundary");
    rx_like   ("abcdef", "ab\\Bc", "non word boundary");
    return exit_status();
}

