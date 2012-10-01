#ifndef __RX_H__
#define __RX_H__

int rx_debug;

typedef struct Rx Rx;

Rx   *rx_new   (const char *rx_str);
void  rx_free  (Rx *rx);
void  rx_print (Rx *rx);
int   rx_match (Rx *rx, const char *str);

#endif

