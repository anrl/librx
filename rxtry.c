#include <stdio.h>
#include "rx.h"

int
main (int argc, char **argv) {
    rx_debug = 1;
    if (argc != 3) {
        fprintf(stderr, "usage: ./rx string regex\n");
        return 1;
    }
    Rx *rx = rx_new(argv[2]);
    if (!rx)
        return 0;
    rx_match(rx, argv[1]);
    rx_free(rx);
    return 0;
}

