#include <stdio.h>
#include "rx.h"

int
main (int argc, char **argv) {
    char buffer[16384];
    char *string;
    char *regex;
    Rx *rx;
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "usage: ./rxtry [string] regex\n");
        return 1;
    }
    rx_debug = 1;
    if (argc == 2) {
        fread(buffer, 1, sizeof buffer, stdin);
        string = buffer;
    }
    else {
        string = argv[1];
    }
    regex = argv[argc - 1];
    rx = rx_new(regex);
    if (!rx)
        return 0;
    rx_match(rx, string);
    rx_free(rx);
    return 0;
}

