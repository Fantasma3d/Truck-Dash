/* Logica do app independente de plataforma: junta rede + paineis */
#include "app.h"
#include "panel.h"
#include "net.h"

static net_t g_net;
static dash_t g_dash;
static surf_t g_bg;
static uint32_t g_bg_key;
static double g_last;
static char g_theme_file[256];

static void save_theme(void) {
    char b[16];
    if (!g_theme_file[0]) return;
    int fd = open(g_theme_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    int n = snprintf(b, sizeof b, "%d\n", g_dash.manual ? g_dash.theme + 1 : 0);
    write(fd, b, n);
    close(fd);
}

static void load_theme(void) {
    char b[16];
    int fd = open(g_theme_file, O_RDONLY);
    if (fd < 0) return;
    ssize_t n = read(fd, b, sizeof b - 1);
    close(fd);
    if (n <= 0) return;
    b[n] = 0;
    int v = 0;
    for (int i = 0; b[i] >= '0' && b[i] <= '9'; i++) v = v * 10 + (b[i] - '0');
    if (v > 0 && v <= THEME_COUNT) { g_dash.manual = 1; g_dash.theme = v - 1; }
}

void app_init(const char *data_dir, const char *ext_dir) {
    memset(&g_dash, 0, sizeof g_dash);
    g_dash.theme = 9;
    if (data_dir) { snprintf(g_theme_file, sizeof g_theme_file, "%s/tema.txt", data_dir); load_theme(); }
    if (ext_dir) snprintf(g_net.cfg_path, sizeof g_net.cfg_path, "%s/truckdash.txt", ext_dir);
    net_start(&g_net);
    g_last = now_s();
}

void app_tap(void) {
    /* Auto -> tema 0 -> 1 -> ... -> 9 -> Auto */
    if (!g_dash.manual) { g_dash.manual = 1; g_dash.theme = 0; }
    else if (g_dash.theme + 1 >= THEME_COUNT) g_dash.manual = 0;
    else g_dash.theme++;
    if (g_dash.manual) snprintf(g_dash.toast, sizeof g_dash.toast, "Painel: %s", panel_theme_name(g_dash.theme));
    else snprintf(g_dash.toast, sizeof g_dash.toast, "Painel automatico (pela marca)");
    g_dash.toast_until = g_dash.t + 2.0f;
    save_theme();
    g_bg_key = 0;
}

void app_frame(surf_t *fb) {
    double t = now_s();
    float dt = (float)(t - g_last);
    if (dt > 0.2f) dt = 0.2f;
    g_last = t;

    pthread_mutex_lock(&g_net.m);
    if (g_net.rx_count) {
        g_dash.p = g_net.pkt;
        g_dash.have = 1;
    }
    g_dash.connected = g_net.rx_count && (t - g_net.rx_time) < 3.0 && (g_net.pkt.flags & TDF_SDK_ACTIVE);
    memcpy(g_dash.local_ip, g_net.local_ip, sizeof g_dash.local_ip);
    g_dash.usb = g_net.usb;
    if (g_net.server) ip_str(g_net.server, g_dash.server, sizeof g_dash.server);
    pthread_mutex_unlock(&g_net.m);

    panel_update(&g_dash, dt);

    if (g_bg.w != fb->w || g_bg.h != fb->h) {
        free(g_bg.px);
        g_bg.px = (uint32_t *)malloc((size_t)fb->w * fb->h * 4);
        g_bg.w = fb->w; g_bg.h = fb->h; g_bg.stride = fb->w;
        g_bg_key = 0;
    }
    if (!g_bg.px) return;
    uint32_t key = panel_static_key(&g_dash, fb->w, fb->h) | 1;
    if (key != g_bg_key) {
        panel_draw_static(&g_bg, &g_dash);
        g_bg_key = key;
    }
    r_blit(fb, &g_bg);
    panel_draw_dynamic(fb, &g_dash);
}
