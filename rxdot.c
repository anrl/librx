#include <stdio.h>
#include <unistd.h>
#include "rx.h"

int
main (int argc, char **argv) {
    char *regex;
    Rx *rx;
    int backwards = 0;
    char opt;
    while ((opt = getopt(argc, argv, "b")) != -1) {
        if (opt == 'b') {
            backwards = 1;
        }
    }
    if (argc - optind != 1) {
        fprintf(stderr, "usage: ./rxdot <regex>\n");
        return 1;
    }
    regex = argv[optind];
    rx = rx_new(regex);
    if (!rx)
        return 0;
    rx_print(rx, backwards);
    rx_free(rx);
    return 0;
}

