/* TruckDash - plugin de telemetria para Euro Truck Simulator 2 / ATS.
 *
 * Copie TruckDash.dll para  <ETS2>\bin\win_x64\plugins\
 * O plugin le a telemetria pelo SCS SDK e envia por UDP para o tablet.
 * Nao precisa de servidor nem de nenhum outro programa no PC.
 */
#include "scs_min.h"
#include "../common/td_proto.h"

/* ------------------------------------------------------------------ */
/* Camada de plataforma: Windows (sem CRT, so kernel32/ws2_32) ou Linux */
#ifdef _WIN32
typedef uintptr_t SOCKET;
#define INVALID_SOCKET ((SOCKET)~(uintptr_t)0)
#define AF_INET 2
#define SOCK_DGRAM 2
#define IPPROTO_UDP 17
#define SOL_SOCKET 0xffff
#define SO_REUSEADDR 0x0004
#define SO_BROADCAST 0x0020
#define FIONBIO 0x8004667Eu
struct sockaddr { unsigned short sa_family; char sa_data[14]; };
struct sockaddr_in { short sin_family; unsigned short sin_port; uint32_t sin_addr; char sin_zero[8]; };
struct addrinfo {
    int ai_flags, ai_family, ai_socktype, ai_protocol;
    size_t ai_addrlen;
    char *ai_canonname;
    struct sockaddr *ai_addr;
    struct addrinfo *ai_next;
};
#define WINAPI __stdcall
__declspec(dllimport) int WINAPI WSAStartup(unsigned short, void *);
__declspec(dllimport) int WINAPI WSACleanup(void);
__declspec(dllimport) SOCKET WINAPI socket(int, int, int);
__declspec(dllimport) int WINAPI bind(SOCKET, const struct sockaddr *, int);
__declspec(dllimport) int WINAPI setsockopt(SOCKET, int, int, const char *, int);
__declspec(dllimport) int WINAPI ioctlsocket(SOCKET, long, unsigned long *);
__declspec(dllimport) int WINAPI sendto(SOCKET, const char *, int, int, const struct sockaddr *, int);
__declspec(dllimport) int WINAPI recvfrom(SOCKET, char *, int, int, struct sockaddr *, int *);
__declspec(dllimport) int WINAPI closesocket(SOCKET);
__declspec(dllimport) int WINAPI connect(SOCKET, const struct sockaddr *, int);
__declspec(dllimport) int WINAPI send(SOCKET, const char *, int, int);
__declspec(dllimport) int WINAPI recv(SOCKET, char *, int, int);
__declspec(dllimport) int WINAPI getsockopt(SOCKET, int, int, char *, int *);
__declspec(dllimport) int WINAPI WSAGetLastError(void);
typedef struct { unsigned int fd_count; SOCKET fd_array[64]; } fd_set;
struct timeval { long tv_sec, tv_usec; };
__declspec(dllimport) int WINAPI select(int, fd_set *, fd_set *, fd_set *, const struct timeval *);
#define SOCK_STREAM 1
#define IPPROTO_TCP 6
#define TCP_NODELAY 1
#define SO_ERROR 0x1007
static int would_block(void) { int e = WSAGetLastError(); return e == 10035 /*WSAEWOULDBLOCK*/ || e == 10036 /*INPROGRESS*/; }
typedef int socklen_t;
__declspec(dllimport) int WINAPI gethostname(char *, int);
__declspec(dllimport) int WINAPI getaddrinfo(const char *, const char *, const struct addrinfo *, struct addrinfo **);
__declspec(dllimport) void WINAPI freeaddrinfo(struct addrinfo *);
__declspec(dllimport) unsigned long long WINAPI GetTickCount64(void);
__declspec(dllimport) unsigned long WINAPI GetModuleFileNameA(void *, char *, unsigned long);
__declspec(dllimport) void *WINAPI CreateFileA(const char *, unsigned long, unsigned long, void *, unsigned long, unsigned long, void *);
__declspec(dllimport) int WINAPI ReadFile(void *, void *, unsigned long, unsigned long *, void *);
__declspec(dllimport) int WINAPI CloseHandle(void *);
static void *g_module;
/* le TruckDash.txt na mesma pasta da DLL */
static int read_cfg(char *buf, int max) {
    char path[600];
    unsigned long n = GetModuleFileNameA(g_module, path, 520), got = 0;
    if (n == 0 || n >= 520) return 0;
    while (n > 0 && path[n - 1] != '\\' && path[n - 1] != '/') n--;
    const char *f = "TruckDash.txt";
    for (int i = 0; f[i]; i++) path[n++] = f[i];
    path[n] = 0;
    void *h = CreateFileA(path, 0x80000000u /*GENERIC_READ*/, 1 /*SHARE_READ*/, 0, 3 /*OPEN_EXISTING*/, 0x80, 0);
    if (h == (void *)(intptr_t)-1) return 0;
    if (!ReadFile(h, buf, (unsigned long)max - 1, &got, 0)) got = 0;
    CloseHandle(h);
    buf[got] = 0;
    return (int)got;
}
#define EXPORT __declspec(dllexport)
static uint64_t now_ms(void) { return GetTickCount64(); }
static void sock_nonblock(SOCKET s) { unsigned long one = 1; ioctlsocket(s, (long)FIONBIO, &one); }
#define closesock closesocket

/* sem CRT: o compilador pode gerar chamadas a memset/memcpy */
int _fltused = 0;
void *memset(void *d, int c, size_t n) { volatile unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { volatile unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; return d; }
int WINAPI DllMain(void *h, unsigned long reason, void *r) { (void)r; if (reason == 1) g_module = h; return 1; }
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/select.h>
#include <netinet/tcp.h>
typedef int SOCKET;
static int would_block(void) { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS; }
#define INVALID_SOCKET (-1)
#define EXPORT __attribute__((visibility("default")))
static uint64_t now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static void sock_nonblock(SOCKET s) { fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK); }
#define closesock close
static int read_cfg(char *buf, int max) {
    int fd = open("TruckDash.txt", O_RDONLY);
    if (fd < 0) return 0;
    int n = (int)read(fd, buf, max - 1);
    close(fd);
    if (n < 0) n = 0;
    buf[n] = 0;
    return n;
}
#endif

static uint16_t be16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }
static void zero(void *p, size_t n) { volatile unsigned char *q = p; while (n--) *q++ = 0; }
static int str_eq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static void str_cpy(char *d, const char *s, size_t n) {
    size_t i = 0;
    if (s) for (; i + 1 < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
}

/* ------------------------------------------------------------------ */
static td_packet g_pkt;
static scs_log_t g_log;
static SOCKET g_sock = INVALID_SOCKET;
static uint64_t g_last_send, g_last_bcast_refresh;

#define MAX_BCAST 8
static uint32_t g_bcast[MAX_BCAST];   /* enderecos em ordem de rede */
static int g_nbcast;

static void logmsg(const char *m) { if (g_log) g_log(SCS_LOG_TYPE_message, m); }

/* IPs fixos de tablets (arquivo TruckDash.txt ao lado da DLL) */
#define MAX_FIXED 4
static uint32_t g_fixed[MAX_FIXED];
static int g_nfixed;

static void load_fixed(void) {
    char buf[512];
    int n = read_cfg(buf, sizeof buf), k = 0;
    unsigned v[4] = {0};
    int digits = 0, prev = g_nfixed;
    g_nfixed = 0;
    for (int i = 0; i <= n; i++) {
        char c = i < n ? buf[i] : '\n';
        if (c >= '0' && c <= '9') { v[k] = v[k] * 10 + (unsigned)(c - '0'); digits = 1; }
        else if (c == '.' && digits && k < 3) { k++; digits = 0; }
        else {
            if (k == 3 && digits && v[0] && v[0] < 256 && v[1] < 256 && v[2] < 256 && v[3] < 256 && g_nfixed < MAX_FIXED) {
                uint32_t a; unsigned char *b = (unsigned char *)&a;
                b[0] = (unsigned char)v[0]; b[1] = (unsigned char)v[1]; b[2] = (unsigned char)v[2]; b[3] = (unsigned char)v[3];
                g_fixed[g_nfixed++] = a;
            }
            k = 0; digits = 0; v[0] = v[1] = v[2] = v[3] = 0;
        }
    }
    if (g_nfixed && g_nfixed != prev) logmsg("TruckDash: usando IP do tablet de TruckDash.txt");
}

#define MAX_SUBS 8
typedef struct { uint32_t addr; uint16_t port; uint64_t seen; } sub_t;
static sub_t g_subs[MAX_SUBS];


static void add_bcast(uint32_t a) {
    for (int i = 0; i < g_nbcast; i++) if (g_bcast[i] == a) return;
    if (g_nbcast < MAX_BCAST) g_bcast[g_nbcast++] = a;
}

/* lista os IPv4 do PC e usa o broadcast /24 de cada um + 255.255.255.255 */
static void refresh_bcast(void) {
    char host[256];
    struct addrinfo hints, *res = 0, *it;
    g_nbcast = 0;
    add_bcast(0xffffffffu);
    if (gethostname(host, sizeof host) != 0) return;
    zero(&hints, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, 0, &hints, &res) != 0) return;
    for (it = res; it; it = it->ai_next) {
        if (!it->ai_addr || it->ai_family != AF_INET) continue;
        const struct sockaddr_in *sa = (const struct sockaddr_in *)it->ai_addr;
        uint32_t ip; memcpy(&ip, &sa->sin_addr, 4);
        unsigned char *b = (unsigned char *)&ip;
        if (b[0] == 127 || b[0] == 0 || (b[0] == 169 && b[1] == 254)) continue;
        b[3] = 255;
        add_bcast(ip);
    }
    freeaddrinfo(res);
}

static void net_open(void) {
    struct sockaddr_in sa;
    int one = 1;
#ifdef _WIN32
    static unsigned char wsa[1024];
    WSAStartup(0x0202, wsa);
#endif
    g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_sock == INVALID_SOCKET) { logmsg("TruckDash: falha ao criar socket UDP"); return; }
    setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof one);
    setsockopt(g_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);
    zero(&sa, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = be16(TD_PORT_PC);
    if (bind(g_sock, (struct sockaddr *)&sa, sizeof sa) != 0)
        logmsg("TruckDash: porta 45454 ocupada (so broadcast vai funcionar)");
    sock_nonblock(g_sock);
    refresh_bcast();
    load_fixed();
    g_last_bcast_refresh = now_ms();
}

static void net_poll_hello(uint64_t t) {
    char buf[64];
    struct sockaddr_in from;
    for (int guard = 0; guard < 32; guard++) {
#ifdef _WIN32
        int fl = sizeof from;
#else
        socklen_t fl = sizeof from;
#endif
        int n = recvfrom(g_sock, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (n <= 0) break;
        if (n < (int)sizeof(td_hello) || buf[0] != 'T' || buf[1] != 'D' || buf[2] != 'H' || buf[3] != 'I') continue;
        uint32_t a; memcpy(&a, &from.sin_addr, 4);
        int slot = -1, oldest = 0;
        for (int i = 0; i < MAX_SUBS; i++) {
            if (g_subs[i].addr == a && g_subs[i].port == from.sin_port) { slot = i; break; }
            if (g_subs[i].seen < g_subs[oldest].seen) oldest = i;
        }
        if (slot < 0) {
            slot = oldest;
            logmsg("TruckDash: tablet conectado");
        }
        g_subs[slot].addr = a; g_subs[slot].port = from.sin_port; g_subs[slot].seen = t;
    }
}

static void send_to(uint32_t addr, uint16_t port_be) {
    struct sockaddr_in sa;
    zero(&sa, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = port_be;
    memcpy(&sa.sin_addr, &addr, 4);
    sendto(g_sock, (const char *)&g_pkt, sizeof g_pkt, 0, (struct sockaddr *)&sa, sizeof sa);
}

static void net_send(uint64_t t) {
    if (g_sock == INVALID_SOCKET) return;
    g_pkt.seq++;
    for (int i = 0; i < g_nbcast; i++) send_to(g_bcast[i], be16(TD_PORT_TABLET));
    for (int i = 0; i < g_nfixed; i++) send_to(g_fixed[i], be16(TD_PORT_TABLET));
    for (int i = 0; i < MAX_SUBS; i++)
        if (g_subs[i].addr && t - g_subs[i].seen < 6000) send_to(g_subs[i].addr, g_subs[i].port);
}

/* ------------------------------------------------------------------ */
/* USB: TCP para 127.0.0.1:45456, que o "adb forward" leva ate o tablet */
static SOCKET g_tcp = INVALID_SOCKET;
static int g_tcp_state;              /* 0 nada, 1 conectando, 2 conectado */
static uint64_t g_tcp_retry, g_tcp_since;
static unsigned char g_pend[sizeof(td_packet)];
static int g_pend_len, g_pend_off;
static int g_tcp_logged;

static void tcp_close(uint64_t t) {
    if (g_tcp != INVALID_SOCKET) closesock(g_tcp);
    g_tcp = INVALID_SOCKET; g_tcp_state = 0; g_pend_len = g_pend_off = 0;
    g_tcp_retry = t + 2000;
}

static void tcp_poll(uint64_t t) {
    if (g_tcp_state == 0) {
        struct sockaddr_in sa;
        int one = 1;
        if (t < g_tcp_retry) return;
        g_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (g_tcp == INVALID_SOCKET) { g_tcp_retry = t + 5000; return; }
        sock_nonblock(g_tcp);
        setsockopt(g_tcp, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
        zero(&sa, sizeof sa);
        sa.sin_family = AF_INET;
        sa.sin_port = be16(TD_PORT_USB);
        { unsigned char *b = (unsigned char *)&sa.sin_addr; b[0] = 127; b[3] = 1; }
        if (connect(g_tcp, (struct sockaddr *)&sa, sizeof sa) != 0 && !would_block()) { tcp_close(t); return; }
        g_tcp_state = 1; g_tcp_since = t;
        return;
    }
    if (g_tcp_state == 1) {
        fd_set w, e;
        struct timeval tv = { 0, 0 };
#ifdef _WIN32
        w.fd_count = 1; w.fd_array[0] = g_tcp; e.fd_count = 1; e.fd_array[0] = g_tcp;
        int r = select(0, 0, &w, &e, &tv);
        if (r > 0 && e.fd_count) { tcp_close(t); return; }
        if (r > 0 && w.fd_count) {
#else
        FD_ZERO(&w); FD_ZERO(&e); FD_SET(g_tcp, &w); FD_SET(g_tcp, &e);
        int r = select(g_tcp + 1, 0, &w, &e, &tv);
        if (r > 0 && FD_ISSET(g_tcp, &w)) {
#endif
            int err = 0; socklen_t l = sizeof err;
            getsockopt(g_tcp, SOL_SOCKET, SO_ERROR, (char *)&err, &l);
            if (err) { tcp_close(t); return; }
            g_tcp_state = 2; g_tcp_since = t;
        } else if (t - g_tcp_since > 3000) tcp_close(t);
        return;
    }
    /* conectado: o adb fecha a conexao se o app nao estiver ouvindo no tablet */
    char c[64];
    int n = recv(g_tcp, c, sizeof c, 0);
    if (n == 0 || (n < 0 && !would_block())) { tcp_close(t); return; }
    if (!g_tcp_logged && t - g_tcp_since > 3000) { g_tcp_logged = 1; logmsg("TruckDash: tablet conectado pelo USB (adb)"); }
}

static void tcp_send(uint64_t t) {
    if (g_tcp_state != 2) return;
    if (g_pend_len == 0) {
        memcpy(g_pend, &g_pkt, sizeof g_pkt);
        g_pend_len = sizeof g_pkt; g_pend_off = 0;
    }
    while (g_pend_off < g_pend_len) {
        int n = send(g_tcp, (const char *)g_pend + g_pend_off, g_pend_len - g_pend_off, 0);
        if (n < 0) { if (!would_block()) tcp_close(t); return; }   /* buffer cheio: tenta no proximo quadro */
        g_pend_off += n;
    }
    g_pend_len = g_pend_off = 0;
}

/* ------------------------------------------------------------------ */
/* callbacks de canais */
static void SCSAPIFUNC cb_float(const scs_string_t n, const scs_u32_t i, const scs_value_t *const v, const scs_context_t c) {
    (void)n; (void)i;
    *(float *)c = v ? v->v.value_float : 0.0f;
}
static void SCSAPIFUNC cb_s32(const scs_string_t n, const scs_u32_t i, const scs_value_t *const v, const scs_context_t c) {
    (void)n; (void)i;
    *(int32_t *)c = v ? v->v.value_s32 : 0;
}
static void SCSAPIFUNC cb_u32(const scs_string_t n, const scs_u32_t i, const scs_value_t *const v, const scs_context_t c) {
    (void)n; (void)i;
    *(uint32_t *)c = v ? v->v.value_u32 : 0;
}
static void SCSAPIFUNC cb_flag(const scs_string_t n, const scs_u32_t i, const scs_value_t *const v, const scs_context_t c) {
    (void)n; (void)i;
    uint32_t bit = (uint32_t)(uintptr_t)c;
    if (v && v->v.value_bool) g_pkt.flags |= bit; else g_pkt.flags &= ~bit;
}

static scs_telemetry_register_for_channel_t g_reg;
static void reg_f(const char *name, float *dst)      { g_reg(name, SCS_U32_NIL, SCS_VALUE_TYPE_float, SCS_TELEMETRY_CHANNEL_FLAG_none, cb_float, dst); }
static void reg_s(const char *name, int32_t *dst)    { g_reg(name, SCS_U32_NIL, SCS_VALUE_TYPE_s32, SCS_TELEMETRY_CHANNEL_FLAG_none, cb_s32, dst); }
static void reg_u(const char *name, uint32_t *dst)   { g_reg(name, SCS_U32_NIL, SCS_VALUE_TYPE_u32, SCS_TELEMETRY_CHANNEL_FLAG_none, cb_u32, dst); }
static void reg_b(const char *name, uint32_t bit)    { g_reg(name, SCS_U32_NIL, SCS_VALUE_TYPE_bool, SCS_TELEMETRY_CHANNEL_FLAG_none, cb_flag, (scs_context_t)(uintptr_t)bit); }

/* ------------------------------------------------------------------ */
/* eventos */
static float attr_f(const scs_value_t *v) {
    switch (v->type) {
    case SCS_VALUE_TYPE_float: return v->v.value_float;
    case SCS_VALUE_TYPE_double: return (float)v->v.value_double;
    case SCS_VALUE_TYPE_u32: return (float)v->v.value_u32;
    case SCS_VALUE_TYPE_s32: return (float)v->v.value_s32;
    case SCS_VALUE_TYPE_u64: return (float)v->v.value_u64;
    default: return 0.0f;
    }
}
static const char *attr_s(const scs_value_t *v) { return v->type == SCS_VALUE_TYPE_string ? v->v.value_string : ""; }

static void on_config(const scs_telemetry_configuration_t *cfg) {
    const scs_named_value_t *a = cfg->attributes;
    if (str_eq(cfg->id, "truck")) {
        str_cpy(g_pkt.brand_id, "", sizeof g_pkt.brand_id);
        str_cpy(g_pkt.brand, "", sizeof g_pkt.brand);
        str_cpy(g_pkt.truck_id, "", sizeof g_pkt.truck_id);
        str_cpy(g_pkt.truck_name, "", sizeof g_pkt.truck_name);
        for (; a && a->name; a++) {
            const scs_value_t *v = &a->value;
            if (str_eq(a->name, "brand_id")) str_cpy(g_pkt.brand_id, attr_s(v), sizeof g_pkt.brand_id);
            else if (str_eq(a->name, "brand")) str_cpy(g_pkt.brand, attr_s(v), sizeof g_pkt.brand);
            else if (str_eq(a->name, "id")) str_cpy(g_pkt.truck_id, attr_s(v), sizeof g_pkt.truck_id);
            else if (str_eq(a->name, "name")) str_cpy(g_pkt.truck_name, attr_s(v), sizeof g_pkt.truck_name);
            else if (str_eq(a->name, "fuel.capacity")) g_pkt.fuel_capacity = attr_f(v);
            else if (str_eq(a->name, "fuel.warning.factor")) g_pkt.fuel_warn_factor = attr_f(v);
            else if (str_eq(a->name, "adblue.capacity")) g_pkt.adblue_capacity = attr_f(v);
            else if (str_eq(a->name, "brake.air.pressure.warning")) g_pkt.air_warn = attr_f(v);
            else if (str_eq(a->name, "brake.air.pressure.emergency")) g_pkt.air_emerg = attr_f(v);
            else if (str_eq(a->name, "rpm.limit")) g_pkt.rpm_limit = attr_f(v);
            else if (str_eq(a->name, "gears.forward")) g_pkt.fwd_gears = (int32_t)attr_f(v);
            else if (str_eq(a->name, "gears.reverse")) g_pkt.rev_gears = (int32_t)attr_f(v);
            else if (str_eq(a->name, "retarder.steps")) g_pkt.retarder_steps = (uint32_t)attr_f(v);
        }
        g_pkt.config_seq++;
    } else if (str_eq(cfg->id, "job")) {
        int any = 0;
        str_cpy(g_pkt.cargo, "", sizeof g_pkt.cargo);
        str_cpy(g_pkt.dest_city, "", sizeof g_pkt.dest_city);
        g_pkt.cargo_mass = 0; g_pkt.delivery_time = 0;
        g_pkt.flags &= ~(TDF_ON_JOB | TDF_CARGO_LOADED);
        for (; a && a->name; a++) {
            const scs_value_t *v = &a->value;
            any = 1;
            if (str_eq(a->name, "cargo")) str_cpy(g_pkt.cargo, attr_s(v), sizeof g_pkt.cargo);
            else if (str_eq(a->name, "destination.city")) str_cpy(g_pkt.dest_city, attr_s(v), sizeof g_pkt.dest_city);
            else if (str_eq(a->name, "cargo.mass")) g_pkt.cargo_mass = attr_f(v);
            else if (str_eq(a->name, "delivery.time")) g_pkt.delivery_time = (uint32_t)attr_f(v);
            else if (str_eq(a->name, "cargo.loaded") && v->type == SCS_VALUE_TYPE_bool && v->v.value_bool) g_pkt.flags |= TDF_CARGO_LOADED;
        }
        if (any) g_pkt.flags |= TDF_ON_JOB;
        g_pkt.config_seq++;
    }
}

static void SCSAPIFUNC on_event(const scs_event_t ev, const void *const info, const scs_context_t ctx) {
    (void)ctx;
    switch (ev) {
    case SCS_TELEMETRY_EVENT_paused:  g_pkt.flags |= TDF_PAUSED; break;
    case SCS_TELEMETRY_EVENT_started: g_pkt.flags &= ~TDF_PAUSED; break;
    case SCS_TELEMETRY_EVENT_configuration:
        if (info) on_config((const scs_telemetry_configuration_t *)info);
        break;
    case SCS_TELEMETRY_EVENT_frame_end: {
        uint64_t t = now_ms();
        if (g_sock == INVALID_SOCKET) break;
        net_poll_hello(t);
        tcp_poll(t);
        if (t - g_last_bcast_refresh > 10000) { refresh_bcast(); load_fixed(); g_last_bcast_refresh = t; }
        if (t - g_last_send >= 40) {       /* ~25 pacotes/s */
            g_last_send = t;
            net_send(t);
            tcp_send(t);
        }
        break;
    }
    default: break;
    }
}

static int is_game(const char *id, const char *want) { return id && str_eq(id, want); }

EXPORT scs_result_t SCSAPIFUNC scs_telemetry_init(const scs_u32_t version, const void *const vparams) {
    const scs_telemetry_init_params_v100_t *p = (const scs_telemetry_init_params_v100_t *)vparams;
    if (version != SCS_TELEMETRY_VERSION_1_00 && version != SCS_TELEMETRY_VERSION_1_01)
        return SCS_RESULT_unsupported;
    g_log = p->common.log;
    if (!is_game(p->common.game_id, "eut2") && !is_game(p->common.game_id, "ats")) {
        logmsg("TruckDash: jogo nao suportado");
        return SCS_RESULT_unsupported;
    }

    zero(&g_pkt, sizeof g_pkt);
    g_pkt.magic[0] = 'T'; g_pkt.magic[1] = 'D'; g_pkt.magic[2] = 'S'; g_pkt.magic[3] = 'H';
    g_pkt.version = TD_VERSION;
    g_pkt.size = sizeof(td_packet);
    g_pkt.flags = TDF_PAUSED | TDF_SDK_ACTIVE;
    g_pkt.rpm_limit = 2500.0f;
    str_cpy(g_pkt.game_id, p->common.game_id, sizeof g_pkt.game_id);

    if (p->register_for_event(SCS_TELEMETRY_EVENT_frame_end, on_event, 0) != SCS_RESULT_ok) {
        logmsg("TruckDash: falha ao registrar eventos");
        return SCS_RESULT_generic_error;
    }
    p->register_for_event(SCS_TELEMETRY_EVENT_paused, on_event, 0);
    p->register_for_event(SCS_TELEMETRY_EVENT_started, on_event, 0);
    p->register_for_event(SCS_TELEMETRY_EVENT_configuration, on_event, 0);

    g_reg = p->register_for_channel;
    reg_f("truck.speed", &g_pkt.speed);
    reg_f("truck.engine.rpm", &g_pkt.rpm);
    reg_s("truck.engine.gear", &g_pkt.gear);
    reg_s("truck.displayed.gear", &g_pkt.displayed_gear);
    reg_f("truck.cruise_control", &g_pkt.cruise);
    reg_f("truck.navigation.speed.limit", &g_pkt.speed_limit);
    reg_f("truck.navigation.distance", &g_pkt.nav_distance);
    reg_f("truck.navigation.time", &g_pkt.nav_time);
    reg_f("truck.fuel.amount", &g_pkt.fuel);
    reg_f("truck.fuel.range", &g_pkt.fuel_range);
    reg_f("truck.fuel.consumption.average", &g_pkt.fuel_avg);
    reg_f("truck.adblue", &g_pkt.adblue);
    reg_f("truck.brake.air.pressure", &g_pkt.air_pressure);
    reg_f("truck.brake.temperature", &g_pkt.brake_temp);
    reg_f("truck.oil.pressure", &g_pkt.oil_pressure);
    reg_f("truck.oil.temperature", &g_pkt.oil_temp);
    reg_f("truck.water.temperature", &g_pkt.water_temp);
    reg_f("truck.battery.voltage", &g_pkt.battery_voltage);
    reg_f("truck.odometer", &g_pkt.odometer);
    reg_f("truck.effective.throttle", &g_pkt.throttle);
    reg_f("truck.effective.brake", &g_pkt.brake);
    reg_f("truck.effective.clutch", &g_pkt.clutch);
    reg_f("truck.dashboard.backlight", &g_pkt.backlight);
    reg_f("truck.wear.engine", &g_pkt.wear_engine);
    reg_f("truck.wear.transmission", &g_pkt.wear_transmission);
    reg_f("truck.wear.cabin", &g_pkt.wear_cabin);
    reg_f("truck.wear.chassis", &g_pkt.wear_chassis);
    reg_f("truck.wear.wheels", &g_pkt.wear_wheels);
    reg_u("truck.brake.retarder", &g_pkt.retarder);
    reg_u("truck.light.aux.front", &g_pkt.aux_front);
    reg_u("truck.light.aux.roof", &g_pkt.aux_roof);
    reg_u("game.time", &g_pkt.game_time);
    reg_s("rest.stop", &g_pkt.rest_stop);

    reg_b("truck.engine.enabled", TDF_ENGINE);
    reg_b("truck.electric.enabled", TDF_ELECTRIC);
    reg_b("truck.brake.parking", TDF_PARKING_BRAKE);
    reg_b("truck.brake.motor", TDF_MOTOR_BRAKE);
    reg_b("truck.brake.air.pressure.warning", TDF_AIR_WARN);
    reg_b("truck.brake.air.pressure.emergency", TDF_AIR_EMERG);
    reg_b("truck.fuel.warning", TDF_FUEL_WARN);
    reg_b("truck.adblue.warning", TDF_ADBLUE_WARN);
    reg_b("truck.oil.pressure.warning", TDF_OIL_WARN);
    reg_b("truck.water.temperature.warning", TDF_WATER_WARN);
    reg_b("truck.battery.voltage.warning", TDF_BATT_WARN);
    reg_b("truck.light.lblinker", TDF_BLINK_L);
    reg_b("truck.light.rblinker", TDF_BLINK_R);
    reg_b("truck.hazard.warning", TDF_HAZARD);
    reg_b("truck.light.parking", TDF_LIGHT_PARK);
    reg_b("truck.light.beam.low", TDF_LIGHT_LOW);
    reg_b("truck.light.beam.high", TDF_LIGHT_HIGH);
    reg_b("truck.light.beacon", TDF_BEACON);
    reg_b("truck.light.brake", TDF_BRAKE_LIGHT);
    reg_b("truck.light.reverse", TDF_REVERSE);
    reg_b("truck.wipers", TDF_WIPERS);
    reg_b("truck.differential_lock", TDF_DIFF_LOCK);
    reg_b("truck.lift_axle", TDF_LIFT_AXLE);
    reg_b("truck.trailer.lift_axle", TDF_TRAILER_LIFT);

    net_open();
    logmsg("TruckDash: plugin iniciado - enviando telemetria para o tablet (UDP 45455)");
    return SCS_RESULT_ok;
}

EXPORT void SCSAPIFUNC scs_telemetry_shutdown(void) {
    if (g_sock != INVALID_SOCKET) {
        g_pkt.flags &= ~TDF_SDK_ACTIVE;   /* avisa o tablet que o jogo fechou */
        net_send(now_ms());
        tcp_send(now_ms());
        tcp_close(0);
        closesock(g_sock);
        g_sock = INVALID_SOCKET;
#ifdef _WIN32
        WSACleanup();
#endif
    }
    g_log = 0;
}
