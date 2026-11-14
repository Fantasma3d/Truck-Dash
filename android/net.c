/* Rede do tablet: escuta os pacotes do plugin e manda HELLO para ser descoberto */
#include "net.h"

void ip_str(uint32_t a, char *b, int n) {
    const unsigned char *p = (const unsigned char *)&a;
    snprintf(b, n, "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
}

static uint32_t parse_ip(const char *s) {
    unsigned v[4] = {0}; int k = 0;
    for (; *s && k < 4; s++) {
        if (*s >= '0' && *s <= '9') v[k] = v[k] * 10 + (*s - '0');
        else if (*s == '.') k++;
        else if (*s == '\n' || *s == '\r' || *s == ' ') { if (v[0]) break; }
    }
    if (k != 3 || v[0] == 0 || v[0] > 255 || v[1] > 255 || v[2] > 255 || v[3] > 255) return 0;
    uint32_t a; unsigned char *p = (unsigned char *)&a;
    p[0] = v[0]; p[1] = v[1]; p[2] = v[2]; p[3] = v[3];
    return a;
}

static uint32_t read_cfg_ip(const char *path) {
    char buf[64];
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    ssize_t n = read(fd, buf, sizeof buf - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = 0;
    return parse_ip(buf);
}

/* lista os IPs do tablet (Wi-Fi, USB...) direto das interfaces de rede */
static void find_local_ip(net_t *n, uint32_t target) {
    (void)target;
    struct ifreq req[16];
    struct ifconf ic;
    char out[48] = "", b[20];
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) return;
    ic.ifc_len = sizeof req;
    ic.ifc_buf = (char *)req;
    if (ioctl(s, SIOCGIFCONF, &ic) == 0) {
        int cnt = ic.ifc_len / (int)sizeof(struct ifreq);
        for (int i = 0; i < cnt && i < 16; i++) {
            struct sockaddr_in *sa = (struct sockaddr_in *)&req[i].ifr_addr;
            const unsigned char *p = (const unsigned char *)&sa->sin_addr.s_addr;
            if (sa->sin_family != AF_INET || p[0] == 127 || p[0] == 0) continue;
            ip_str(sa->sin_addr.s_addr, b, sizeof b);
            size_t l = strlen(out);
            if (l + strlen(b) + 4 < sizeof out) snprintf(out + l, sizeof out - l, "%s%s", l ? " / " : "", b);
        }
    }
    close(s);
    pthread_mutex_lock(&n->m);
    memcpy(n->local_ip, out, sizeof out);
    pthread_mutex_unlock(&n->m);
}

static void send_hello(int s, uint32_t addr, uint32_t seq) {
    td_hello h;
    struct sockaddr_in sa;
    memcpy(h.magic, "TDHI", 4);
    h.version = TD_VERSION; h.reserved = 0; h.seq = seq;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = td_htons(TD_PORT_PC);
    sa.sin_addr.s_addr = addr;
    sendto(s, &h, sizeof h, 0, (struct sockaddr *)&sa, sizeof sa);
}

static void *net_thread(void *arg) {
    net_t *n = (net_t *)arg;
    int s = -1;
    uint32_t seq = 0;
    double last_hello = 0, last_ipcheck = -100;
    unsigned char buf[1024];
    while (!n->quit) {
        if (s < 0) {
            struct sockaddr_in sa;
            int one = 1;
            struct timeval tv = { 0, 250000 };
            s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (s < 0) { usleep(1000000); continue; }
            setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
            setsockopt(s, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
            memset(&sa, 0, sizeof sa);
            sa.sin_family = AF_INET;
            sa.sin_port = td_htons(TD_PORT_TABLET);
            if (bind(s, (struct sockaddr *)&sa, sizeof sa) != 0) { close(s); s = -1; usleep(1000000); continue; }
        }
        double t = now_s();
        if (t - last_hello >= 1.0) {
            uint32_t cfg = n->cfg_path[0] ? read_cfg_ip(n->cfg_path) : 0;
            last_hello = t;
            seq++;
            send_hello(s, 0xffffffffu, seq);
            if (n->server) send_hello(s, n->server, seq);
            if (cfg && cfg != n->server) send_hello(s, cfg, seq);
        }
        if (t - last_ipcheck > 3.0) {
            last_ipcheck = t;
            find_local_ip(n, n->server ? n->server : 0x08080808u);
        }
        struct sockaddr_in from;
        socklen_t fl = sizeof from;
        ssize_t r = recvfrom(s, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (r < 16) continue;
        if (memcmp(buf, "TDSH", 4) != 0) continue;
        uint16_t ver, size;
        memcpy(&ver, buf + 4, 2); memcpy(&size, buf + 6, 2);
        if (ver != TD_VERSION || size != r) continue;
        pthread_mutex_lock(&n->m);
        uint32_t seqn; memcpy(&seqn, buf + 8, 4);
        /* com broadcast + unicast o mesmo pacote chega 2x: ignora repetidos */
        if (seqn != n->pkt.seq || n->server != from.sin_addr.s_addr || t - n->rx_time > 1.0) {
            memset(&n->pkt, 0, sizeof n->pkt);
            memcpy(&n->pkt, buf, (size_t)r < sizeof n->pkt ? (size_t)r : sizeof n->pkt);
            n->rx_time = t;
            n->rx_count++;
            if (n->server != from.sin_addr.s_addr) last_ipcheck = -100;
            n->server = from.sin_addr.s_addr;
        }
        pthread_mutex_unlock(&n->m);
    }
    if (s >= 0) close(s);
    return 0;
}

/* ---- USB: o "adb forward" do PC entrega a conexao TCP em 127.0.0.1:45456 ---- */
static int recv_all(int c, unsigned char *b, int len) {
    int got = 0;
    while (got < len) {
        ssize_t r = recv(c, b + got, (size_t)(len - got), 0);
        if (r <= 0) return 0;
        got += (int)r;
    }
    return 1;
}

static void *usb_thread(void *arg) {
    net_t *n = (net_t *)arg;
    unsigned char buf[1024];
    for (;;) {
        struct sockaddr_in sa;
        int one = 1;
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) { usleep(2000000); continue; }
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        memset(&sa, 0, sizeof sa);
        sa.sin_family = AF_INET;
        sa.sin_port = td_htons(TD_PORT_USB);
        { unsigned char *b = (unsigned char *)&sa.sin_addr.s_addr; b[0] = 127; b[3] = 1; }
        if (bind(s, (struct sockaddr *)&sa, sizeof sa) != 0 || listen(s, 2) != 0) { close(s); usleep(2000000); continue; }
        for (;;) {
            int c = accept(s, 0, 0);
            if (c < 0) break;
            n->usb = 1;
            for (;;) {
                uint16_t ver, size;
                if (!recv_all(c, buf, 8)) break;
                memcpy(&ver, buf + 4, 2); memcpy(&size, buf + 6, 2);
                if (memcmp(buf, "TDSH", 4) != 0 || size < 16 || size > sizeof buf) break;   /* fora de sincronia */
                if (!recv_all(c, buf + 8, size - 8)) break;
                if (ver != TD_VERSION) continue;
                pthread_mutex_lock(&n->m);
                memset(&n->pkt, 0, sizeof n->pkt);
                memcpy(&n->pkt, buf, size < sizeof n->pkt ? size : sizeof n->pkt);
                n->rx_time = now_s();
                n->rx_count++;
                pthread_mutex_unlock(&n->m);
            }
            n->usb = 0;
            close(c);
        }
        close(s);
    }
    return 0;
}

void net_start(net_t *n) {
    pthread_t th, th2;
    pthread_mutex_init(&n->m, 0);
    pthread_create(&th, 0, net_thread, n);
    pthread_detach(th);
    pthread_create(&th2, 0, usb_thread, n);
    pthread_detach(th2);
}
