NAME
====

librx - Regex library inspired by Thompson NFAs, Perl 6, and PCRE

SYNOPSIS
========

    #include <rx.h>
    
    int main {
        Rx *rx = rx_new("((title|chapter|page|line|column)-)? (0|1|2|3|4|5)+");
        if (rx_match(rx, "chapter-55"))
            printf("'chapter-55' matches!\n");
        rx_free(rx);
    }

prints "'chapter-55' matches!"

DESCRIPTION
===========

This regular expression library is based on a Thompson NFA rather than a
backtracking NFA. I originally read about Thompson's NFA in Russ Cox's article,
"Regular Expression Matching Can Be Simple And Fast" at
http://swtch.com/~rsc/regexp/regexp1.html. It also uses some syntactic features
found in Perl 6 which is described at http://perlcabal.org/syn/S05.html.

During a match all possible paths are explored until there are no paths left or
no characters left in the string.

FEATURES
========

-   Insignificant whitespace

    The following are equivalent: ``/a b/`` and ``/      ab        /``

-   Extensible metasyntax

    In classic Perl regex ``(?...)`` can add special features. Anything that would
    have been implemented this way will be implemented with angled
    brackets: ``<...>``. Currently it only supports referencing subpatterns.

-   Recursive subpatterns

    You can refer to the pattern in previous groups by referencing them as a
    number in the extensible meta syntax. ``/(cool)<0>/``.
    
-   Syntax section has more

FUNCTIONS
=========

-   ``Rx *rx_new(const char *rx_str)``

    Allocate a new Rx object from a string containing the regular expression.

-   ``int rx_match(Rx *rx, const char *str)``

    Match the regex against a string. Returns whether it matched. Eventually
    this should fill in a match object which will allow one to find out what
    matched and the groups that matched in it.

-   ``void rx_free(Rx *rx)``

    Frees the memory of a regex previously created by rx_new().

-   ``int rx_debug``

    You may set this global variable to cause rx_new() to print out a
    representation of the regex to stdout and rx_match() will print out its
    list of paths and matches after each character of the string is read.

SYNTAX
======

Many normal regex features are supported.

An alphanumeric character, '_', or '-' will match itself. All other characters
need to be escaped with a backslash (unimplemented).

You may quote a string of characters with single (``'``) or double (``"``)
quotes and its contents will match unaltered. There is no difference between
single and double quotes except you have the escape the delimeter you use.
for example ``'\'*\''`` is the same as ``"'*'"`` (unimplemented).

All whitespace is insignificant except in quoted forms.

A ``|`` separates alternate matches.

Each atom may have a repetition specifier after it.

-   ``*`` matches 0 or more times
-   ``+`` matches 1 or more times
-   ``?`` matches 0 or 1 times

TODO more of this...

