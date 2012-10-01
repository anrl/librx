#include <stdio.h>
#include "rx.h"

int
main (int argc, char **argv) {
    char *regex;
    Rx *rx;
    if (argc != 2) {
        fprintf(stderr, "usage: ./rxdot <regex>\n");
        return 1;
    }
    regex = argv[1];
    rx = rx_new(regex);
    if (!rx)
        return 0;
	rx_print(rx);
    rx_free(rx);
    return 0;
}

