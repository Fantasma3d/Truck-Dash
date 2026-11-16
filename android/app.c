/* Logica do app independente de plataforma: rede + painel Scania Smart Dash (camadas PNG) */
#include "app.h"
#include "panel.h"
#include "net.h"
#include "skin.h"

#define SKIN_NAME "Scania Smart Dash"
#define SKIN_FOLDER "TruckDash/scania-smart-dash"

static net_t g_net;
static dash_t g_dash;
static double g_last;
static char g_skin_dir[256];
static skin_t *g_skin;
static int g_demo;
static int g_page;              /* tela do carrossel (0 = primeira) */
static char g_page_file[256];

static void save_page(void) {
    char b[16];
    if (!g_page_file[0]) return;
    int fd = open(g_page_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    int n = snprintf(b, sizeof b, "%d\n", g_page + 1);
    write(fd, b, (size_t)n);
    close(fd);
}

static void load_page(void) {
    char b[16];
    int fd = open(g_page_file, O_RDONLY);
    if (fd < 0) return;
    ssize_t n = read(fd, b, sizeof b - 1);
    close(fd);
    if (n <= 0) return;
    b[n] = 0;
    int v = 0;
    for (int i = 0; b[i] >= '0' && b[i] <= '9'; i++) v = v * 10 + (b[i] - '0');
    if (v >= 1 && v <= 16) g_page = v - 1;
}

static void toast(const char *m) {
    snprintf(g_dash.toast, sizeof g_dash.toast, "%s", m);
    g_dash.toast_until = g_dash.t + 2.0f;
}

static void load_skin(void) {
    if (g_skin) { skin_free(g_skin); g_skin = 0; }
    if (!g_skin_dir[0]) return;
    g_skin = skin_load(g_skin_dir);
    if (skin_ok(g_skin) && !skin_error(g_skin)[0])
        snprintf(g_dash.status, sizeof g_dash.status, "Painel: %s (%d itens)", SKIN_NAME, skin_items(g_skin));
    else snprintf(g_dash.status, sizeof g_dash.status, "%s", skin_error(g_skin));
}

void app_init(const char *data_dir, const char *ext_dir) {
    memset(&g_dash, 0, sizeof g_dash);
    if (data_dir) { snprintf(g_page_file, sizeof g_page_file, "%s/tela.txt", data_dir); load_page(); }
    if (ext_dir) {
        snprintf(g_net.cfg_path, sizeof g_net.cfg_path, "%s/truckdash.txt", ext_dir);
        snprintf(g_skin_dir, sizeof g_skin_dir, "%s/%s", ext_dir, SKIN_FOLDER);
    }
    load_skin();
    net_start(&g_net);
    g_last = now_s();
}

/* carrossel: toque a direita avanca, a esquerda volta.
 * toque longo: a esquerda recarrega os PNGs, a direita liga/desliga a demonstracao */
void app_tap(int long_press, float x) {
    int right = x >= 0.5f;
    if (long_press) {
        if (right) { g_demo = !g_demo; toast(g_demo ? "Demonstracao ligada" : "Demonstracao desligada"); }
        else { load_skin(); toast(skin_ok(g_skin) && !skin_error(g_skin)[0] ? "Painel recarregado" : "Painel com problema"); }
        return;
    }
    if (!g_dash.connected || !skin_ok(g_skin)) return;
    int n = skin_pages(g_skin);
    g_page = (g_page + (right ? 1 : n - 1)) % n;
    save_page();
}

/* dados inventados para ajustar o painel sem abrir o jogo */
static void demo_packet(td_packet *p, float t) {
    int ph = (int)(t / 3.0f) % 3;
    memset(p, 0, sizeof *p);
    memcpy(p->game_id, "eut2", 5);
    memcpy(p->brand, "Scania", 7); memcpy(p->truck_name, "S", 2); memcpy(p->dest_city, "Demo", 5);
    p->flags = TDF_SDK_ACTIVE | TDF_ENGINE | TDF_ELECTRIC;
    if (ph == 1) p->flags |= 0x07ffffffu & ~(TDF_PAUSED | TDF_BLINK_L | TDF_BLINK_R);
    if (ph >= 1 && ((int)(t * 2.5f) & 1)) p->flags |= TDF_BLINK_L | TDF_BLINK_R;
    p->speed = (50.0f + 45.0f * r_sin(t * 0.5f)) / 3.6f;
    p->rpm = 1400.0f + 900.0f * r_sin(t * 0.8f); p->rpm_limit = 2500;
    p->cruise = ph == 1 ? 80 / 3.6f : 0; p->speed_limit = 80 / 3.6f;
    p->fuel_capacity = 700; p->fuel = 520.0f - t * 0.012f; p->fuel_range = p->fuel * 3; p->fuel_avg = 0.32f;
    p->adblue_capacity = 80; p->adblue = 40.0f + 38.0f * r_sin(t * 0.25f);
    p->air_pressure = 100.0f + 40.0f * r_sin(t * 0.4f); p->oil_pressure = 55; p->oil_temp = 95; p->water_temp = 80.0f + 15.0f * r_sin(t * 0.2f);
    p->battery_voltage = 27.6f; p->odometer = 123456.0f + t * 0.02f; p->nav_distance = 87000; p->nav_time = 4200;
    p->throttle = 0.5f + 0.5f * r_sin(t * 0.8f);
    p->gear = p->displayed_gear = 1 + (int)(t * 0.7f) % 12;
    p->retarder = ph == 1 ? 2 : 0; p->retarder_steps = 3;
    p->aux_front = p->aux_roof = ph == 1;
    p->game_time = 8 * 60 + (uint32_t)(t * 2);
    p->oil_temp = 70.0f + 25.0f * r_sin(t * 0.15f);
    if (ph == 2) p->speed_limit = 0;
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

    if (g_demo) { demo_packet(&g_dash.p, g_dash.t); g_dash.connected = 1; }
    panel_update(&g_dash, dt);

    if (!g_dash.connected) panel_draw_waiting(fb, &g_dash);
    else if (skin_ok(g_skin)) {
        if (g_page >= skin_pages(g_skin)) g_page = 0;
        skin_draw(fb, g_skin, &g_dash, g_page);
        if (g_dash.p.flags & TDF_PAUSED) r_text(fb, F_BOLD, fb->w * 0.5f, fb->h * 0.08f, fb->h * 0.04f, "PAUSADO", HEX(0xffc240), AL_CENTER);
    } else panel_draw_message(fb, "Painel nao carregado", g_dash.status, "Copie a pasta TruckDash para a raiz do tablet: /sdcard/" SKIN_FOLDER "/painel.txt");
    panel_draw_toast(fb, &g_dash);
}
