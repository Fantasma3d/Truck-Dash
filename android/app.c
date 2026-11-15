/* Logica do app independente de plataforma: junta rede + paineis */
#include "app.h"
#include "panel.h"
#include "net.h"
#include "scsdash.h"

static net_t g_net;
static dash_t g_dash;
static surf_t g_bg;
static uint32_t g_bg_key;
static double g_last;
static char g_theme_file[256];

/* ---- painel real do jogo (.tdp no armazenamento do tablet) ---- */
static char g_pack_dir[256];
static sd_pack *g_real;
static char g_real_name[64];
static uint32_t g_real_cfg = 0xffffffffu;
static int g_real_off;          /* usuario preferiu os temas do TruckDash */
static int g_page = 1;
static float g_odo0 = -1;       /* inicio da viagem (para hodometro parcial) */
static uint32_t g_time0;

static void save_settings(void) {
    char b[48];
    if (!g_theme_file[0]) return;
    int fd = open(g_theme_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    int n = snprintf(b, sizeof b, "%d %d %d\n", g_dash.manual ? g_dash.theme + 1 : 0, g_real_off, g_page);
    write(fd, b, n);
    close(fd);
}

static void load_settings(void) {
    char b[48];
    int fd = open(g_theme_file, O_RDONLY);
    if (fd < 0) return;
    ssize_t n = read(fd, b, sizeof b - 1);
    close(fd);
    if (n <= 0) return;
    b[n] = 0;
    int v[3] = { 0, 0, 1 }, k = 0;
    for (int i = 0; b[i] && k < 3; i++) {
        if (b[i] >= '0' && b[i] <= '9') { v[k] = 0; while (b[i] >= '0' && b[i] <= '9') v[k] = v[k] * 10 + (b[i++] - '0'); k++; i--; }
    }
    if (v[0] > 0 && v[0] <= THEME_COUNT) { g_dash.manual = 1; g_dash.theme = v[0] - 1; }
    g_real_off = v[1] ? 1 : 0;
    if (v[2] >= 1 && v[2] <= 8) g_page = v[2];
}

void app_init(const char *data_dir, const char *ext_dir) {
    memset(&g_dash, 0, sizeof g_dash);
    g_dash.theme = 9;
    if (data_dir) { snprintf(g_theme_file, sizeof g_theme_file, "%s/tema.txt", data_dir); load_settings(); }
    if (ext_dir) {
        snprintf(g_net.cfg_path, sizeof g_net.cfg_path, "%s/truckdash.txt", ext_dir);
        snprintf(g_pack_dir, sizeof g_pack_dir, "%s/TruckDash/paineis", ext_dir);
    }
    net_start(&g_net);
    g_last = now_s();
}

static void toast(const char *m) {
    snprintf(g_dash.toast, sizeof g_dash.toast, "%s", m);
    g_dash.toast_until = g_dash.t + 2.0f;
}

void app_tap(int long_press) {
    char b[64];
    if (g_real && !g_real_off) {
        if (long_press) { g_real_off = 1; toast("Paineis do TruckDash"); }
        else {
            g_page = g_page % (sd_pages(g_real) > 0 ? sd_pages(g_real) : 1) + 1;
            snprintf(b, sizeof b, "Tela %d de %d", g_page, sd_pages(g_real)); toast(b);
        }
    } else if (long_press) {
        if (g_real) { g_real_off = 0; toast("Painel original do caminhao"); }
        else toast("Sem painel original para este caminhao");
    } else {
        /* Auto -> tema 0 -> 1 -> ... -> 9 -> Auto */
        if (!g_dash.manual) { g_dash.manual = 1; g_dash.theme = 0; }
        else if (g_dash.theme + 1 >= THEME_COUNT) g_dash.manual = 0;
        else g_dash.theme++;
        if (g_dash.manual) { snprintf(b, sizeof b, "Painel: %s", panel_theme_name(g_dash.theme)); toast(b); }
        else toast("Painel automatico (pela marca)");
    }
    save_settings();
    g_bg_key = 0;
}

/* ---------------- escolha do pacote pelo caminhao ---------------- */
static int tokenize(const char *s, char tok[][24], int max) {
    int n = 0, l = 0;
    for (;; s++) {
        char c = *s;
        if (c >= 'A' && c <= 'Z') c += 32;
        int dig = c >= '0' && c <= '9', alp = c >= 'a' && c <= 'z';
        /* separa letras de numeros: actros2014 -> actros 2014 */
        if (l && (dig || alp) && ((tok[n][l - 1] >= '0' && tok[n][l - 1] <= '9') != dig) && n < max - 1) { tok[n][l] = 0; n++; l = 0; }
        if (alp || dig) { if (l < 23) tok[n][l++] = c; }
        else {
            if (l && n < max) { tok[n][l] = 0; n++; }
            l = 0;
            if (!c || n >= max) break;
        }
    }
    return n;
}

static int is_digits(const char *s) { if (!*s) return 0; while (*s) { if (*s < '0' || *s > '9') return 0; s++; } return 1; }

/* nota de semelhanca entre o id do caminhao (ex.: vehicle.scania.s_2016) e o nome do painel */
static int match_score(const char *truck, const char *pack, int ats) {
    char a[8][24], b[8][24];
    int na = tokenize(truck, a, 8), nb = tokenize(pack, b, 8), sc = 0, start = 0;
    if (na && !strcmp(a[0], "vehicle")) start = 1;
    if (na <= start || !nb || strcmp(a[start], b[0]) != 0) return -1;       /* marca diferente */
    int mph = 0;
    for (int j = 0; j < nb; j++) {
        if (!strcmp(b[j], "gps") || !strcmp(b[j], "cam") || !strcmp(b[j], "mirrors") || !strcmp(b[j], "mirror") || !strcmp(b[j], "panel")) return -1;
        if (!strcmp(b[j], "mph")) mph = 1;
    }
    sc = 10;
    for (int i = start + 1; i < na; i++)
        for (int j = 1; j < nb; j++) {
            if (!strcmp(a[i], b[j])) { sc += 10; break; }
            size_t la = strlen(a[i]), lb = strlen(b[j]);
            /* 2012 x 12 */
            if (is_digits(a[i]) && is_digits(b[j]) && la == 4 && lb == 2 && !strcmp(a[i] + 2, b[j])) { sc += 8; break; }
            if (la >= 3 && lb >= 3 && (!strncmp(a[i], b[j], 3))) { sc += 3; break; }
            if (la == 1 && lb >= 4 && a[i][0] == b[j][0]) { sc += 6; break; }   /* r -> rcab */
        }
    if (mph != ats) sc -= 5;
    /* penaliza palavras do painel que nao existem no id do caminhao */
    for (int j = 1; j < nb; j++) {
        int hit = 0;
        for (int i = start + 1; i < na && !hit; i++)
            if (!strcmp(a[i], b[j]) || (is_digits(a[i]) && is_digits(b[j]) && strlen(a[i]) == 4 && strlen(b[j]) == 2 && !strcmp(a[i] + 2, b[j])) ||
                (strlen(a[i]) == 1 && strlen(b[j]) >= 4 && a[i][0] == b[j][0])) hit = 1;
        if (!hit && strcmp(b[j], "mph") && strcmp(b[j], "exc")) sc -= 2;
    }
    if (nb >= 3 && !strcmp(b[nb - 1], "exc")) sc += 1;   /* prefere a versao completa */
    return sc;
}

static void choose_real(void) {
    char idx[4096], best[64] = "", path[400];
    int best_sc = 14;   /* precisa bater a marca e mais alguma coisa */
    if (!g_pack_dir[0]) return;
    snprintf(path, sizeof path, "%s/indice.txt", g_pack_dir);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return;
    ssize_t n = read(fd, idx, sizeof idx - 1);
    close(fd);
    if (n <= 0) return;
    idx[n] = 0;
    int ats = panel_is_ats(&g_dash);
    for (char *line = idx; *line;) {
        char *e = line;
        while (*e && *e != '\n' && *e != '\r') e++;
        char save = *e; *e = 0;
        if (*line) {
            int sc = match_score(g_dash.p.truck_id, line, ats);
            if (sc > best_sc) { best_sc = sc; snprintf(best, sizeof best, "%s", line); }
        }
        *e = save;
        line = *e ? e + 1 : e;
    }
    if (!best[0]) { if (g_real) { sd_free(g_real); g_real = 0; g_real_name[0] = 0; } return; }
    if (g_real && !strcmp(best, g_real_name)) return;
    if (g_real) { sd_free(g_real); g_real = 0; }
    snprintf(path, sizeof path, "%s/%s.tdp", g_pack_dir, best);
    g_real = sd_load(path);
    snprintf(g_real_name, sizeof g_real_name, "%s", g_real ? best : "");
    if (g_real && g_page > sd_pages(g_real)) g_page = 1;
    g_bg_key = 0;
}

/* ---------------- valores pelos IDs do painel (documentacao SCS) ---------------- */
static float spd(float ms) { float v = ms * (panel_is_ats(&g_dash) ? 2.2369363f : 3.6f); return v < 0 ? -v : v; }
static float dist(float km) { return panel_is_ats(&g_dash) ? km * 0.621371f : km; }
static const char *U_SPD(void) { return panel_is_ats(&g_dash) ? "mph" : "km/h"; }
static const char *U_DST(void) { return panel_is_ats(&g_dash) ? "mi" : "km"; }

static void hhmm(char *b, int n, int minutes) {
    if (minutes < 0) minutes = 0;
    snprintf(b, n, "%02d:%02d", (minutes / 60) % 100, minutes % 60);
}

static int gear_disp(void) { return g_dash.p.displayed_gear ? g_dash.p.displayed_gear : g_dash.p.gear; }

static const char *cb_text(void *u, int id, char *b, int n) {
    (void)u;
    const td_packet *p = &g_dash.p;
    float trip_km = g_odo0 >= 0 ? p->odometer - g_odo0 : 0;
    int trip_min = g_odo0 >= 0 ? (int)(p->game_time - g_time0) : 0;
    switch (id) {
    case 1010: snprintf(b, n, "%.0f|C", p->oil_temp); return b;
    case 1020: snprintf(b, n, "%.0f|%s", g_dash.n_speed, U_SPD()); return b;
    case 1030: snprintf(b, n, "%.0f|%s", dist(p->odometer), U_DST()); return b;
    case 1040: {
        int g = gear_disp();
        if (g > 0) snprintf(b, n, "A%d", g); else if (g < 0) snprintf(b, n, "R%d", -g); else snprintf(b, n, "N");
        return b;
    }
    case 1050: hhmm(b, n, (int)(p->game_time % 1440)); return b;
    case 1060: snprintf(b, n, "%.0f|l", p->fuel); return b;
    case 1080: snprintf(b, n, "%.0f|%s", dist(p->fuel_range), U_DST()); return b;
    case 1090: snprintf(b, n, "%.0f|C", p->water_temp); return b;
    case 1100: snprintf(b, n, "%.0f", spd(p->cruise)); return b;
    case 1160: case 1170: snprintf(b, n, "%.1f|l/100km", p->fuel_avg * 100.0f); return b;
    case 1210: snprintf(b, n, "%.0f|%s", dist(trip_km), U_DST()); return b;
    case 1300: { int g = gear_disp(); if (g > 0) snprintf(b, n, "%d", g); else if (g < 0) snprintf(b, n, "R%d", -g); else snprintf(b, n, "N"); return b; }
    case 1310: snprintf(b, n, "%s", gear_disp() < 0 ? "R" : (gear_disp() == 0 ? "N" : "A")); return b;
    case 1320: snprintf(b, n, "%.1f|V", p->battery_voltage); return b;
    case 1370: snprintf(b, n, "%.0f", p->air_pressure); return b;
    case 1380: snprintf(b, n, "%s", gear_disp() > 0 ? "D" : (gear_disp() < 0 ? "R" : "N")); return b;
    case 1390: hhmm(b, n, trip_min); snprintf(b + strlen(b), n - strlen(b), "|h"); return b;
    case 1400: snprintf(b, n, "%.0f|%s", trip_min > 0 ? dist(trip_km) / (trip_min / 60.0f) : 0.0f, U_SPD()); return b;
    case 1510: snprintf(b, n, "%.0f", p->fuel_capacity > 0 ? 100.0f * p->fuel / p->fuel_capacity : 0.0f); return b;
    case 1520: snprintf(b, n, "%.0f", p->adblue_capacity > 0 ? 100.0f * p->adblue / p->adblue_capacity : 0.0f); return b;
    case 1530: if (p->nav_distance <= 10) return 0; hhmm(b, n, (int)((p->game_time + (uint32_t)(p->nav_time / 60.0f)) % 1440)); return b;
    case 1540: if (p->rest_stop <= 0) return 0; hhmm(b, n, p->rest_stop); snprintf(b + strlen(b), n - strlen(b), "|h"); return b;
    case 1610: if (p->speed_limit <= 0.5f) return 0; snprintf(b, n, "%.0f", spd(p->speed_limit)); return b;
    default: return 0;   /* 1000 (temp. externa) e outros: o jogo nao informa */
    }
}

static int cb_num(void *u, int id, float *o) {
    (void)u;
    const td_packet *p = &g_dash.p;
    int ats = panel_is_ats(&g_dash);
    switch (id) {
    case 1070: *o = g_dash.n_fuel; return 1;
    case 1110: *o = ats ? p->oil_pressure : p->oil_pressure * 0.0689476f; return 1;
    case 1120: *o = ats ? p->air_pressure : p->air_pressure * 0.0689476f; return 1;
    case 1130: *o = p->throttle * (p->rpm > 900 ? 1.0f : 0.3f); return 1;   /* turbo: o jogo nao informa, usa o acelerador */
    case 1140: *o = g_dash.n_adblue; return 1;
    case 1150: case 1460: *o = g_dash.n_water; return 1;
    case 1600: case 1340: *o = g_dash.n_oil; return 1;
    case 1700: case 1440: *o = g_dash.n_rpm; return 1;
    case 1430: *o = g_dash.n_speed; return 1;
    case 1450: *o = spd(p->cruise); return 1;
    case 1330: *o = p->battery_voltage; return 1;
    default: return 0;
    }
}

static int cb_vis(void *u, int id) {
    (void)u;
    const td_packet *p = &g_dash.p;
    uint32_t f = p->flags;
    float wear = p->wear_engine;
    if (p->wear_transmission > wear) wear = p->wear_transmission;
    if (p->wear_cabin > wear) wear = p->wear_cabin;
    if (p->wear_chassis > wear) wear = p->wear_chassis;
    if (p->wear_wheels > wear) wear = p->wear_wheels;
    switch (id) {
    case 1190: return wear >= 0.15f;
    case 1200: return !!(f & TDF_FUEL_WARN);
    case 1230: return !!(f & TDF_MOTOR_BRAKE);
    case 1240: return p->cruise > 0.1f;
    case 1260: return !!(f & (TDF_AIR_WARN | TDF_AIR_EMERG));
    case 1270: return !!(f & TDF_PARKING_BRAKE);
    case 1290: return p->retarder > 0;
    case 1420: return wear >= 0.5f;
    case 1470: return !!(f & TDF_LIGHT_PARK);
    case 1480: return !!(f & TDF_LIGHT_LOW);
    case 1490: return !!(f & TDF_DIFF_LOCK);
    case 1530: return p->nav_distance > 10;
    case 1540: return p->rest_stop > 0;
    case 1570: return !!(f & TDF_BLINK_L);
    case 1580: return !!(f & TDF_BLINK_R);
    case 1590: return !!(f & TDF_LIGHT_HIGH);
    case 1610: return p->speed_limit > 0.5f;
    default: return 0;   /* espelhos digitais, memoria do cruise etc. */
    }
}

/* ---------------- quadro ---------------- */
static void ensure_bg(surf_t *fb) {
    if (g_bg.w != fb->w || g_bg.h != fb->h) {
        free(g_bg.px);
        g_bg.px = (uint32_t *)malloc((size_t)fb->w * fb->h * 4);
        g_bg.w = fb->w; g_bg.h = fb->h; g_bg.stride = fb->w;
        g_bg_key = 0;
    }
}

static void frame_real(surf_t *fb) {
    sd_ctx c;
    float pw = (float)sd_width(g_real), ph = (float)sd_height(g_real);
    c.scale = fb->w / pw;
    if (fb->h / ph < c.scale) c.scale = fb->h / ph;
    c.ox = (fb->w - pw * c.scale) * 0.5f;
    c.oy = (fb->h - ph * c.scale) * 0.5f;
    c.page = g_page;
    c.user = 0; c.text = cb_text; c.num = cb_num; c.visible = cb_vis;
    uint32_t key = (uint32_t)(uintptr_t)g_real ^ ((uint32_t)g_page * 2654435761u) ^ ((uint32_t)fb->w << 16) ^ (uint32_t)fb->h ^ 0x5a5a0000u;
    if (key != g_bg_key) {
        r_clear(&g_bg, RGB(0, 0, 0));
        sd_draw(&g_bg, g_real, &c, 0);
        g_bg_key = key;
    }
    r_blit(fb, &g_bg);
    if (!(g_dash.p.flags & TDF_ELECTRIC)) {
        r_clear(fb, RGB(0, 0, 0));
        r_text(fb, F_SANS, fb->w * 0.5f, fb->h * 0.5f, fb->h * 0.045f, "Painel desligado (ignicao)", HEX(0x9aa2ab), AL_CENTER);
    } else {
        sd_draw(fb, g_real, &c, 1);
        panel_draw_extras(fb, &g_dash, c.oy, c.oy + ph * c.scale, fb->h - (c.oy + ph * c.scale));
    }
    if (g_dash.t < g_dash.toast_until) {
        float px = fb->h * 0.045f, w = r_text_width(F_SANS, px, g_dash.toast) + px * 2;
        r_round_rect(fb, (fb->w - w) / 2, fb->h * 0.42f, w, px * 2.2f, px * 0.6f, HEX(0x000000), 0.8f);
        r_text(fb, F_SANS, fb->w * 0.5f, fb->h * 0.42f + px * 1.5f, px, g_dash.toast, HEX(0xffffff), AL_CENTER);
    }
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

    if (g_dash.connected) {
        if (g_odo0 < 0 && g_dash.p.odometer > 0) { g_odo0 = g_dash.p.odometer; g_time0 = g_dash.p.game_time; }
        if (g_dash.p.config_seq != g_real_cfg) { g_real_cfg = g_dash.p.config_seq; choose_real(); }
    }

    ensure_bg(fb);
    if (!g_bg.px) return;
    if (g_dash.connected && g_real && !g_real_off) { frame_real(fb); return; }

    uint32_t key = panel_static_key(&g_dash, fb->w, fb->h) | 1;
    if (key != g_bg_key) {
        panel_draw_static(&g_bg, &g_dash);
        g_bg_key = key;
    }
    r_blit(fb, &g_bg);
    panel_draw_dynamic(fb, &g_dash);
}
