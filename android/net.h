#ifndef NET_H
#define NET_H
#include "plat_sys.h"
#include "../common/td_proto.h"
typedef struct {
    pthread_mutex_t m;
    td_packet pkt;
    double rx_time;      /* now_s() do ultimo pacote */
    uint32_t rx_count;
    uint32_t server;     /* IP do PC (ordem de rede) */
    char local_ip[48];
    char cfg_path[256];  /* arquivo opcional com o IP do PC */
    volatile int usb;    /* 1 = PC conectado pelo cabo (adb) */
    volatile int quit;
} net_t;
void net_start(net_t *n);
void ip_str(uint32_t a, char *b, int n);
#endif
