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
["Regular Expression Matching Can Be Simple And Fast"][rsc]. It also uses some
syntactic features found in Perl 6 [Synopse 05][s5].

[rsc]: http://swtch.com/~rsc/regexp/regexp1.html
[s5]: http://perlcabal.org/syn/S05.html

During a match all possible paths are explored until there are no paths left or
no characters left in the string.

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

Many regex features that one may expect are supported.

An alphanumeric character, '_', or '-' will match itself. All other characters
need to be escaped with a backslash.

Note that in C, double quoted strings interpolate escapes, so you have to
escape all backslashes before sending them to rx_new().

You may quote a string of characters with single (``'``) or double (``"``)
quotes and its contents will match unaltered. There is no difference between
single and double quotes except double quotes allow for escapes. For example,
``'*runs away*'`` will match the string ``"*runs away*"``.

All whitespace is insignificant except in quoted forms.

A ``|`` separates alternate matches.

Each atom may have a quantifier after it.

-   ``*`` matches 0 or more times
-   ``+`` matches 1 or more times
-   ``?`` matches 0 or 1 times
-   ``** n`` matches n times
-   ``** n..m`` matches at least n times and at most m times
-   ``** n..*`` matches n or more times

You may group a portion of the regex in parentheses ``(`` which may be used as
any other atom.

An extensible meta-syntax of the form ``<...>`` has been added to implement
special features much like the Perl construct of ``(?...)``.

You can refer to the pattern in previous groups by referencing them as a number
in the extensible meta syntax. ``/(cool)<~~0>/``. These can even refer to its
own group recursively. You can refer to the whole pattern by using ``<~~>``.

The '.' character really matches any character. If you want everything but a
newline, use \N. Also, there are escapes \T and \R for anything but \t and \r.

There are also a few escapes that match a character class. \w matches a word
char, \s matches a space char, and \d matches a digit. The may be negated with
\W, \S, and \D which will match anything but what their lower case version
would match.

A character class is specified with <[...]>. For example, ``<[a..z_]>``,
specifies any character from a to z or _. whitespace is ignored in this
construct. and you can combine character classes by adding and subtracting
them. <[a..z] + ['] - [m..q]>. Negated character classes start with a -,
<-[aeiou]>, matches anything but a vowel.

The following named character classes are allowed as well: upper, lower, alpha,
digit, xdigit, print, graph, cntrl, punct, alnum, space, blank, and word. They
may be combined with + and - just as the bracketed char classes can.
``<[_] + alpha + punct>``.

