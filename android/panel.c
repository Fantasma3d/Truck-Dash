/* Paineis (clusters) do TruckDash. Coordenadas virtuais 1000 x 600. */
#include "panel.h"
#include "plat.h"

#define PI 3.14159265358979f
#define DEG (PI / 180.0f)

enum { L_TWIN = 0, L_DIGITAL = 1 };

typedef struct {
    const char *name;
    int layout;
    uint32_t bg0, bg1, face0, face1, bezel, tick, numeral, needle, hub, accent, disp_bg, disp_fg, red, eco;
    int font;
    int chrome;
} theme_t;

static const theme_t TH[THEME_COUNT] = {
    { "Scania S/R (Next Gen)", L_TWIN, HEX(0x0b0c0f), HEX(0x000000), HEX(0x16181c), HEX(0x040506), HEX(0x2b2f35),
      HEX(0xf2f2f2), HEX(0xffffff), HEX(0xff3b1f), HEX(0x1c1c1e), HEX(0xff6a2a), HEX(0x06080b), HEX(0xe8eef5), HEX(0xd82222), HEX(0x2fb153), F_SANS, 0 },
    { "Scania R / Streamline", L_TWIN, HEX(0x111216), HEX(0x020203), HEX(0x202226), HEX(0x0a0b0d), HEX(0x8a8f96),
      HEX(0xffffff), HEX(0xffffff), HEX(0xe3201b), HEX(0x2a2a2a), HEX(0xffb000), HEX(0x100b02), HEX(0xffc240), HEX(0xd82222), HEX(0x33b14f), F_COND, 1 },
    { "Volvo FH", L_DIGITAL, HEX(0x07101a), HEX(0x000000), HEX(0x0c1520), HEX(0x03060a), HEX(0x1f2b38),
      HEX(0xc9d6e3), HEX(0xe8f1fa), HEX(0x5ec8ff), HEX(0x101820), HEX(0x5ec8ff), HEX(0x07101a), HEX(0xe8f1fa), HEX(0xff4646), HEX(0x37d67a), F_SANS, 0 },
    { "Mercedes-Benz Actros", L_TWIN, HEX(0x0d0e10), HEX(0x000000), HEX(0x121417), HEX(0x050506), HEX(0xb7bcc2),
      HEX(0xf4f4f4), HEX(0xffffff), HEX(0xf04d23), HEX(0x303236), HEX(0xffffff), HEX(0x0a0d11), HEX(0xf0f3f6), HEX(0xd82222), HEX(0x34b45a), F_SANS, 1 },
    { "MAN TGX", L_TWIN, HEX(0x0e0e10), HEX(0x000000), HEX(0x141416), HEX(0x060606), HEX(0x3a3a3e),
      HEX(0xf5f5f5), HEX(0xf5f5f5), HEX(0xe1261c), HEX(0x1e1e20), HEX(0xff3a2e), HEX(0x0c0808), HEX(0xffe9e6), HEX(0xe1261c), HEX(0x34b45a), F_COND, 0 },
    { "DAF XF", L_TWIN, HEX(0x090c12), HEX(0x000000), HEX(0x111722), HEX(0x05070b), HEX(0x8c96a5),
      HEX(0xeef3fa), HEX(0xffffff), HEX(0xe8352b), HEX(0x1a2130), HEX(0x4aa8ff), HEX(0x060a12), HEX(0xdbeaff), HEX(0xe8352b), HEX(0x34b45a), F_SANS, 1 },
    { "Renault Trucks T", L_DIGITAL, HEX(0x0f0b06), HEX(0x000000), HEX(0x18130c), HEX(0x070503), HEX(0x3a2e1c),
      HEX(0xf1e6d2), HEX(0xfff4e0), HEX(0xffb31a), HEX(0x201a10), HEX(0xffb31a), HEX(0x0f0b06), HEX(0xfff4e0), HEX(0xff4a2a), HEX(0x8bd24a), F_SANS, 0 },
    { "Iveco S-Way", L_DIGITAL, HEX(0x050b14), HEX(0x000000), HEX(0x0a1424), HEX(0x03070d), HEX(0x1b2a40),
      HEX(0xd0e2f7), HEX(0xeaf3ff), HEX(0x3fa9ff), HEX(0x0f1a2a), HEX(0x3fa9ff), HEX(0x050b14), HEX(0xeaf3ff), HEX(0xff4646), HEX(0x37d67a), F_SANS, 0 },
    { "Americano classico", L_TWIN, HEX(0x1b140f), HEX(0x070504), HEX(0xf3ecd9), HEX(0xd6cbb0), HEX(0xc9ccd1),
      HEX(0x141414), HEX(0x141414), HEX(0xd0201a), HEX(0x2b2b2b), HEX(0xffb000), HEX(0x100b02), HEX(0xffc240), HEX(0xc81e1e), HEX(0x2f9a49), F_COND, 1 },
    { "Generico", L_TWIN, HEX(0x0c0d10), HEX(0x000000), HEX(0x16181b), HEX(0x060708), HEX(0x34383e),
      HEX(0xeeeeee), HEX(0xffffff), HEX(0xff5a1f), HEX(0x1e1e20), HEX(0x58c4ff), HEX(0x080a0d), HEX(0xeef2f6), HEX(0xd82222), HEX(0x34b45a), F_SANS, 0 },
};

/* ---------------- utilidades ---------------- */
static float S, OX, OY;               /* escala e deslocamento virtual -> tela */
#define X(v) (OX + (v) * S)
#define Y(v) (OY + (v) * S)
#define L(v) ((v) * S)

static void set_view(const surf_t *s) {
    float sx = s->w / 1000.0f, sy = s->h / 600.0f;
    S = sx < sy ? sx : sy;
    OX = (s->w - 1000.0f * S) * 0.5f;
    OY = (s->h - 600.0f * S) * 0.5f;
}

static int ci_contains(const char *h, const char *n) {
    for (; *h; h++) {
        const char *a = h, *b = n;
        while (*a && *b && ((*a | 32) == (*b | 32))) { a++; b++; }
        if (!*b) return 1;
    }
    return 0;
}
static int ci_eq(const char *a, const char *b) {
    while (*a && *b && ((*a | 32) == (*b | 32))) { a++; b++; }
    return *a == 0 && *b == 0;
}

int panel_auto_theme(const td_packet *p) {
    const char *b = p->brand_id[0] ? p->brand_id : p->brand;
    if (ci_eq(b, "scania")) {
        if (ci_contains(p->truck_id, "streamline")) return 1;
        size_t n = strlen(p->truck_id);
        if (n >= 8 && ci_eq(p->truck_id + n - 8, "scania.r")) return 1;
        return 0;
    }
    if (ci_contains(b, "volvo")) return 2;
    if (ci_contains(b, "mercedes")) return 3;
    if (ci_eq(b, "man")) return 4;
    if (ci_eq(b, "daf")) return 5;
    if (ci_contains(b, "renault")) return 6;
    if (ci_contains(b, "iveco")) return 7;
    if (ci_contains(b, "kenworth") || ci_contains(b, "peterbilt") || ci_contains(b, "mack") || ci_contains(b, "western"))
        return 8;
    return 9;
}

const char *panel_theme_name(int t) { return (t >= 0 && t < THEME_COUNT) ? TH[t].name : "?"; }

static int is_ats(const dash_t *d) { return d->p.game_id[0] == 'a'; }

static float speed_max(const dash_t *d) { return is_ats(d) ? 80.0f : 125.0f; }
static float rpm_limit(const dash_t *d) { float l = d->p.rpm_limit; return (l < 1000 || l > 8000) ? 2500.0f : l; }
static float rpm_max(const dash_t *d) {
    float m = ((int)((rpm_limit(d) + 300.0f) / 500.0f) + 1) * 500.0f;
    return m < 2500 ? 2500 : m;
}
static float speed_disp(const dash_t *d, float ms) { float v = ms * (is_ats(d) ? 2.2369363f : 3.6f); return v < 0 ? -v : v; }
static float air_disp(const dash_t *d, float psi) { return is_ats(d) ? psi : psi * 0.0689476f; }
static float air_max(const dash_t *d) { return is_ats(d) ? 150.0f : 12.0f; }

uint32_t panel_static_key(const dash_t *d, int w, int h) {
    uint32_t k = (uint32_t)d->theme * 2654435761u;
    k ^= (uint32_t)w * 40503u ^ (uint32_t)h * 2246822519u;
    k ^= (uint32_t)rpm_max(d) * 3266489917u ^ (uint32_t)rpm_limit(d);
    k ^= is_ats(d) ? 0x55aa55aau : 0;
    k ^= d->connected ? 0x1234u : 0x4321u;
    return k;
}

/* angulo de um mostrador: 135 graus (baixo-esq) ate 405 (baixo-dir), sentido horario */
static float dial_angle(float frac) { return (135.0f + 270.0f * r_clamp(frac, 0, 1.03f)) * DEG; }

/* ---------------- icones (luzes-espia) ---------------- */
enum {
    IC_BLINK_L, IC_BLINK_R, IC_LOW, IC_HIGH, IC_PARK_LIGHT, IC_BEACON, IC_DIFF, IC_LIFT,
    IC_PBRAKE, IC_AIR, IC_FUEL, IC_ADBLUE, IC_OIL, IC_WATER, IC_BATT, IC_RETARDER, IC_EBRAKE, IC_CRUISE, IC_WIPER, IC__N
};

static void icon(surf_t *s, int id, float cx, float cy, float sz, uint32_t c, float a) {
    float u = sz / 40.0f;
#define P(x, y) X(cx + (x) * u), Y(cy + (y) * u)
    switch (id) {
    case IC_BLINK_L: case IC_BLINK_R: {
        float k = id == IC_BLINK_L ? -1.0f : 1.0f;
        float tri[6] = { P(k * 18, 0), P(k * 2, -14), P(k * 2, 14) };
        float rect[8] = { P(k * 4, -6), P(-k * 16, -6), P(-k * 16, 6), P(k * 4, 6) };
        r_poly(s, tri, 3, c, a); r_poly(s, rect, 4, c, a);
        break;
    }
    case IC_LOW: case IC_HIGH: {
        r_arc(s, X(cx + 6 * u), Y(cy), 0, L(12 * u), -90 * DEG, 90 * DEG, c, a);
        r_fill_rect(s, X(cx + 2 * u), Y(cy - 12 * u), L(4 * u), L(24 * u), c, a);
        for (int i = -1; i <= 1; i++) {
            float y = cy + i * 8 * u;
            if (id == IC_HIGH) r_line(s, P(-18, y - cy), P(-4, y - cy), L(3.2f * u), c, a);
            else r_line(s, P(-18, y - cy + 6), P(-4, y - cy), L(3.2f * u), c, a);
        }
        break;
    }
    case IC_PARK_LIGHT:
        r_arc(s, X(cx - 8 * u), Y(cy), L(5 * u), L(8 * u), 90 * DEG, 270 * DEG, c, a);
        r_arc(s, X(cx + 8 * u), Y(cy), L(5 * u), L(8 * u), -90 * DEG, 90 * DEG, c, a);
        for (int i = -1; i <= 1; i++) {
            r_line(s, P(-22, i * 7), P(-14, i * 6), L(2.5f * u), c, a);
            r_line(s, P(14, i * 6), P(22, i * 7), L(2.5f * u), c, a);
        }
        break;
    case IC_BEACON:
        r_arc(s, X(cx), Y(cy + 4 * u), 0, L(11 * u), 180 * DEG, 360 * DEG, c, a);
        r_fill_rect(s, X(cx - 15 * u), Y(cy + 5 * u), L(30 * u), L(5 * u), c, a);
        r_line(s, P(0, -10), P(0, -18), L(2.5f * u), c, a);
        r_line(s, P(-12, -6), P(-18, -12), L(2.5f * u), c, a);
        r_line(s, P(12, -6), P(18, -12), L(2.5f * u), c, a);
        break;
    case IC_PBRAKE: case IC_AIR: case IC_EBRAKE:
        r_arc(s, X(cx), Y(cy), L(10.5f * u), L(13.5f * u), 0, 2 * PI, c, a);
        r_arc(s, X(cx), Y(cy), L(16 * u), L(19 * u), 140 * DEG, 220 * DEG, c, a);
        r_arc(s, X(cx), Y(cy), L(16 * u), L(19 * u), -40 * DEG, 40 * DEG, c, a);
        r_text(s, F_BOLD, X(cx), Y(cy + 6.5f * u), L(18 * u), id == IC_PBRAKE ? "P" : (id == IC_AIR ? "!" : "E"), c, AL_CENTER);
        break;
    case IC_FUEL:
        r_round_rect_outline(s, X(cx - 14 * u), Y(cy - 14 * u), L(16 * u), L(28 * u), L(2 * u), L(3 * u), c, a);
        r_fill_rect(s, X(cx - 11 * u), Y(cy - 11 * u), L(10 * u), L(8 * u), c, a);
        r_line(s, P(2, -4), P(10, -4), L(2.5f * u), c, a);
        r_line(s, P(10, -4), P(10, 8), L(2.5f * u), c, a);
        r_line(s, P(10, -4), P(6, -12), L(2.5f * u), c, a);
        break;
    case IC_ADBLUE:
        r_text(s, F_BOLD, X(cx), Y(cy + 6 * u), L(14 * u), "AdB", c, AL_CENTER);
        r_round_rect_outline(s, X(cx - 19 * u), Y(cy - 11 * u), L(38 * u), L(22 * u), L(4 * u), L(2.5f * u), c, a);
        break;
    case IC_OIL: {
        float can[10] = { P(-16, 6), P(-10, -4), P(8, -4), P(14, 2), P(14, 6) };
        r_poly(s, can, 5, c, a);
        r_line(s, P(14, 2), P(20, -2), L(2.5f * u), c, a);
        r_circle(s, X(cx + 20 * u), Y(cy + 4 * u), L(2.2f * u), c, a);
        r_fill_rect(s, X(cx - 6 * u), Y(cy - 8 * u), L(6 * u), L(4 * u), c, a);
        break;
    }
    case IC_WATER:
        r_fill_rect(s, X(cx - 1.8f * u), Y(cy - 16 * u), L(3.6f * u), L(18 * u), c, a);
        r_circle(s, X(cx), Y(cy + 4 * u), L(5 * u), c, a);
        for (int i = 0; i < 3; i++) r_line(s, P(2, -13 + i * 5), P(9, -13 + i * 5), L(2 * u), c, a);
        r_line(s, P(-16, 12), P(16, 12), L(2.5f * u), c, a);
        break;
    case IC_BATT:
        r_round_rect_outline(s, X(cx - 16 * u), Y(cy - 9 * u), L(32 * u), L(20 * u), L(2 * u), L(3 * u), c, a);
        r_fill_rect(s, X(cx - 12 * u), Y(cy - 13 * u), L(6 * u), L(4 * u), c, a);
        r_fill_rect(s, X(cx + 6 * u), Y(cy - 13 * u), L(6 * u), L(4 * u), c, a);
        r_line(s, P(-11, 1), P(-5, 1), L(2.4f * u), c, a);
        r_line(s, P(5, 1), P(11, 1), L(2.4f * u), c, a);
        r_line(s, P(8, -2), P(8, 4), L(2.4f * u), c, a);
        break;
    case IC_DIFF:
        r_round_rect_outline(s, X(cx - 8 * u), Y(cy - 8 * u), L(16 * u), L(16 * u), L(3 * u), L(3 * u), c, a);
        r_line(s, P(-18, 0), P(-8, 0), L(3 * u), c, a);
        r_line(s, P(8, 0), P(18, 0), L(3 * u), c, a);
        r_line(s, P(-18, -10), P(-18, 10), L(3 * u), c, a);
        r_line(s, P(18, -10), P(18, 10), L(3 * u), c, a);
        break;
    case IC_LIFT:
        r_line(s, P(-16, 8), P(16, 8), L(3 * u), c, a);
        r_circle(s, X(cx - 8 * u), Y(cy + 8 * u), L(5 * u), c, a);
        r_circle(s, X(cx + 8 * u), Y(cy + 2 * u), L(5 * u), c, a);
        r_line(s, P(8, -6), P(8, -16), L(2.5f * u), c, a);
        r_line(s, P(8, -16), P(4, -11), L(2.5f * u), c, a);
        r_line(s, P(8, -16), P(12, -11), L(2.5f * u), c, a);
        break;
    case IC_RETARDER:
        r_text(s, F_BOLD, X(cx), Y(cy + 7 * u), L(20 * u), "R", c, AL_CENTER);
        r_arc(s, X(cx), Y(cy), L(13 * u), L(16 * u), 200 * DEG, 340 * DEG, c, a);
        break;
    case IC_CRUISE:
        r_arc(s, X(cx), Y(cy + 4 * u), L(12 * u), L(15 * u), 180 * DEG, 360 * DEG, c, a);
        r_line(s, P(0, 4), P(9, -8), L(3 * u), c, a);
        r_circle(s, X(cx), Y(cy + 4 * u), L(3 * u), c, a);
        break;
    case IC_WIPER:
        r_arc(s, X(cx), Y(cy + 10 * u), L(15 * u), L(18 * u), 200 * DEG, 340 * DEG, c, a);
        r_line(s, P(0, 10), P(-8, -10), L(2.5f * u), c, a);
        break;
    }
#undef P
}

typedef struct { int id; float x; uint32_t col; } tell_t;
static const tell_t TELL[] = {
    { IC_LOW, 125, HEX(0x30d158) }, { IC_HIGH, 180, HEX(0x2f7bff) }, { IC_PARK_LIGHT, 235, HEX(0x30d158) },
    { IC_BEACON, 290, HEX(0xffa000) }, { IC_DIFF, 345, HEX(0xffa000) }, { IC_LIFT, 400, HEX(0xffa000) },
    { IC_RETARDER, 455, HEX(0x30d158) }, { IC_EBRAKE, 545, HEX(0x30d158) },
    { IC_PBRAKE, 600, HEX(0xff2a2a) }, { IC_AIR, 655, HEX(0xff2a2a) }, { IC_FUEL, 710, HEX(0xffa000) },
    { IC_ADBLUE, 765, HEX(0xffa000) }, { IC_OIL, 820, HEX(0xff2a2a) }, { IC_WATER, 875, HEX(0xff2a2a) },
};
#define TELL_Y 42.0f

static int tell_on(const dash_t *d, int id) {
    uint32_t f = d->p.flags;
    if (!(f & TDF_ELECTRIC)) return 0;
    switch (id) {
    case IC_LOW: return !!(f & TDF_LIGHT_LOW);
    case IC_HIGH: return !!(f & TDF_LIGHT_HIGH);
    case IC_PARK_LIGHT: return !!(f & TDF_LIGHT_PARK);
    case IC_BEACON: return !!(f & TDF_BEACON);
    case IC_DIFF: return !!(f & TDF_DIFF_LOCK);
    case IC_LIFT: return !!(f & (TDF_LIFT_AXLE | TDF_TRAILER_LIFT));
    case IC_RETARDER: return d->p.retarder > 0;
    case IC_EBRAKE: return !!(f & TDF_MOTOR_BRAKE);
    case IC_PBRAKE: return !!(f & TDF_PARKING_BRAKE);
    case IC_AIR: return !!(f & (TDF_AIR_WARN | TDF_AIR_EMERG));
    case IC_FUEL: return !!(f & TDF_FUEL_WARN);
    case IC_ADBLUE: return !!(f & TDF_ADBLUE_WARN);
    case IC_OIL: return !!(f & TDF_OIL_WARN);
    case IC_WATER: return !!(f & TDF_WATER_WARN);
    }
    return 0;
}

static void draw_tell_static(surf_t *s, const theme_t *t) {
    uint32_t dim = r_mix(t->bg0, HEX(0x808080), 0.16f);
    icon(s, IC_BLINK_L, 45, TELL_Y, 36, dim, 1);
    icon(s, IC_BLINK_R, 955, TELL_Y, 36, dim, 1);
    for (unsigned i = 0; i < sizeof TELL / sizeof TELL[0]; i++) icon(s, TELL[i].id, TELL[i].x, TELL_Y, 30, dim, 1);
}

static void draw_tell_dynamic(surf_t *s, dash_t *d) {
    uint32_t f = d->p.flags;
    if (f & TDF_BLINK_L) icon(s, IC_BLINK_L, 45, TELL_Y, 36, HEX(0x30d158), 1);
    if (f & TDF_BLINK_R) icon(s, IC_BLINK_R, 955, TELL_Y, 36, HEX(0x30d158), 1);
    for (unsigned i = 0; i < sizeof TELL / sizeof TELL[0]; i++)
        if (tell_on(d, TELL[i].id)) icon(s, TELL[i].id, TELL[i].x, TELL_Y, 30, TELL[i].col, 1);
    if (d->p.retarder > 0 && (f & TDF_ELECTRIC)) {
        char b[16]; snprintf(b, sizeof b, "%u", d->p.retarder);
        r_text(s, F_BOLD, X(478), Y(TELL_Y + 14), L(14), b, HEX(0x30d158), AL_LEFT);
    }
}

/* ---------------- mostradores ---------------- */
static void bezel(surf_t *s, const theme_t *t, float cx, float cy, float r) {
    if (t->chrome) {
        uint32_t dark = r_scale(t->bezel, 0.35f), light = r_mix(t->bezel, HEX(0xffffff), 0.55f);
        r_arc(s, X(cx), Y(cy), L(r), L(r + 12), 0, 2 * PI, dark, 1);
        for (int i = 0; i < 36; i++) {
            float a0 = i * 10 * DEG, a1 = a0 + 10.5f * DEG, am = a0 + 5 * DEG;
            float k = (1.0f - r_sin(am)) * 0.5f;          /* claro em cima, escuro embaixo */
            r_arc(s, X(cx), Y(cy), L(r + 1), L(r + 11), a0, a1, r_mix(dark, light, k * k), 1);
        }
        r_arc(s, X(cx), Y(cy), L(r - 1), L(r + 1.5f), 0, 2 * PI, HEX(0x000000), 0.8f);
    } else {
        r_arc(s, X(cx), Y(cy), L(r), L(r + 5), 0, 2 * PI, t->bezel, 1);
        r_arc(s, X(cx), Y(cy), L(r + 5), L(r + 7), 0, 2 * PI, HEX(0x000000), 0.6f);
    }
}

static void ticks(surf_t *s, const theme_t *t, float cx, float cy, float r, float vmax, float minor, float major,
                  float label_every, float label_div, float label_px) {
    int n = (int)(vmax / minor + 0.5f);
    for (int i = 0; i <= n; i++) {
        float v = i * minor, a = dial_angle(v / vmax);
        float ca = r_cos(a), sa = r_sin(a);
        int is_major = ((int)(v + 0.5f)) % (int)(major + 0.5f) == 0;
        float r0 = r - 6, r1 = is_major ? r - 30 : r - 18;
        r_line(s, X(cx + ca * r0), Y(cy + sa * r0), X(cx + ca * r1), Y(cy + sa * r1), L(is_major ? 4.2f : 2.2f), t->tick, 1);
        if (((int)(v + 0.5f)) % (int)(label_every + 0.5f) == 0) {
            char b[16]; snprintf(b, sizeof b, "%d", (int)(v / label_div + 0.5f));
            float rl = r - 30 - label_px * 0.9f;
            r_text(s, t->font, X(cx + ca * rl), Y(cy + sa * rl + label_px * 0.36f), L(label_px), b, t->numeral, AL_CENTER);
        }
    }
}

static void needle(surf_t *s, const theme_t *t, float cx, float cy, float r, float frac) {
    float a = dial_angle(frac), ca = r_cos(a), sa = r_sin(a);
    float px = -sa, py = ca;   /* perpendicular */
    float tip = r - 10, tail = 34, w0 = 6.5f, w1 = 2.0f;
    float poly[8] = {
        X(cx - ca * tail + px * w0), Y(cy - sa * tail + py * w0),
        X(cx + ca * tip + px * w1), Y(cy + sa * tip + py * w1),
        X(cx + ca * tip - px * w1), Y(cy + sa * tip - py * w1),
        X(cx - ca * tail - px * w0), Y(cy - sa * tail - py * w0),
    };
    /* brilho por baixo */
    r_line(s, X(cx + ca * 30), Y(cy + sa * 30), X(cx + ca * tip), Y(cy + sa * tip), L(10), t->needle, 0.18f);
    r_poly(s, poly, 4, t->needle, 1);
    r_circle(s, X(cx), Y(cy), L(24), t->hub, 1);
    r_arc(s, X(cx), Y(cy), L(20), L(24), 0, 2 * PI, r_mix(t->hub, HEX(0xffffff), 0.25f), 1);
    r_circle(s, X(cx), Y(cy), L(6), r_scale(t->needle, 0.6f), 1);
}

static void hbar(surf_t *s, float x, float y, float w, float h, float frac, uint32_t col, uint32_t track) {
    r_round_rect(s, X(x), Y(y), L(w), L(h), L(h * 0.5f), track, 1);
    frac = r_clamp(frac, 0, 1);
    if (frac > 0.01f) r_round_rect(s, X(x), Y(y), L(w * frac), L(h), L(h * 0.5f), col, 1);
}

static void vbar(surf_t *s, float x, float y, float w, float h, float frac, uint32_t col, uint32_t track) {
    r_round_rect(s, X(x), Y(y), L(w), L(h), L(4), track, 1);
    frac = r_clamp(frac, 0, 1);
    if (frac > 0.01f) r_round_rect(s, X(x), Y(y + h * (1 - frac)), L(w), L(h * frac), L(4), col, 1);
}

static void speed_limit_sign(surf_t *s, float cx, float cy, float r, int v) {
    char b[8];
    r_circle(s, X(cx), Y(cy), L(r), HEX(0xffffff), 1);
    r_arc(s, X(cx), Y(cy), L(r * 0.78f), L(r), 0, 2 * PI, HEX(0xd71920), 1);
    snprintf(b, sizeof b, "%d", v);
    r_text(s, F_BOLD, X(cx), Y(cy + r * 0.33f), L(r * (v >= 100 ? 0.8f : 0.95f)), b, HEX(0x111111), AL_CENTER);
}

static void gear_str(const dash_t *d, char *b, int n) {
    int g = d->p.displayed_gear;
    if (g == 0 && d->p.gear != 0) g = d->p.gear;
    if (g > 0) snprintf(b, n, "%d", g);
    else if (g < 0) snprintf(b, n, "R%d", -g);
    else snprintf(b, n, "N");
}

static const char *DAYS[7] = { "Seg", "Ter", "Qua", "Qui", "Sex", "Sab", "Dom" };

/* ---------------- textos de informacao ---------------- */
static void info_strip(surf_t *s, dash_t *d, const theme_t *t, float y) {
    char b[64];
    const td_packet *p = &d->p;
    uint32_t lab = r_mix(t->disp_fg, t->bg0, 0.45f), val = t->disp_fg;
    float xs[5] = { 40, 245, 440, 620, 800 };
    const char *labels[5] = { "Hodometro", "Agua", "Oleo", "Bateria", "Pressao do ar" };
    if (is_ats(d)) snprintf(b, sizeof b, "%.0f mi", p->odometer * 0.621371f);
    else snprintf(b, sizeof b, "%.0f km", p->odometer);
    for (int i = 0; i < 5; i++) {
        r_text(s, F_SANS, X(xs[i]), Y(y), L(15), labels[i], lab, AL_LEFT);
        switch (i) {
        case 1: snprintf(b, sizeof b, "%.0f\xc2\xb0" "C", p->water_temp); break;
        case 2: snprintf(b, sizeof b, "%.0f\xc2\xb0" "C  %.1f bar", p->oil_temp, p->oil_pressure * 0.0689476f); break;
        case 3: snprintf(b, sizeof b, "%.1f V", p->battery_voltage); break;
        case 4: if (is_ats(d)) snprintf(b, sizeof b, "%.0f psi", p->air_pressure); else snprintf(b, sizeof b, "%.1f bar", air_disp(d, p->air_pressure)); break;
        }
        r_text(s, F_SANS, X(xs[i]), Y(y + 26), L(22), b, val, AL_LEFT);
    }
}

static void fmt_eta(char *b, int n, float sec) {
    int m = (int)(sec / 60.0f);
    snprintf(b, n, "%dh%02d", m / 60, m % 60);
}

/* display central (TFT) */
static void center_display(surf_t *s, dash_t *d, const theme_t *t, float x, float y, float w, float h) {
    char b[64], e[32];
    const td_packet *p = &d->p;
    uint32_t fg = t->disp_fg, dim = r_mix(t->disp_fg, t->disp_bg, 0.5f);
    float cx = x + w * 0.5f;
    uint32_t gt = p->game_time;
    snprintf(b, sizeof b, "%s %02u:%02u", DAYS[(gt / 1440) % 7], (gt % 1440) / 60, gt % 60);
    r_text(s, F_SANS, X(cx), Y(y + 30), L(20), b, fg, AL_CENTER);

    gear_str(d, e, sizeof e);
    r_text(s, F_BOLD, X(cx), Y(y + 128), L(84), e, t->accent, AL_CENTER);

    float yy = y + 165;
    if (p->cruise > 0.1f) {
        icon(s, IC_CRUISE, cx - 52, yy + 4, 26, HEX(0x30d158), 1);
        snprintf(b, sizeof b, "%.0f", speed_disp(d, p->cruise));
        r_text(s, F_BOLD, X(cx - 30), Y(yy + 14), L(30), b, HEX(0x30d158), AL_LEFT);
    } else {
        r_text(s, F_SANS, X(cx), Y(yy + 12), L(16), "Cruise desligado", dim, AL_CENTER);
    }
    yy += 50;
    if (p->speed_limit > 0.5f) speed_limit_sign(s, cx, yy + 18, 26, (int)(speed_disp(d, p->speed_limit) + 0.5f));
    yy += 66;

    float fuel = p->fuel, range = p->fuel_range;
    if (is_ats(d)) snprintf(b, sizeof b, "%.0f gal  %.0f mi", fuel * 0.264172f, range * 0.621371f);
    else snprintf(b, sizeof b, "%.0f L  %.0f km", fuel, range);
    r_text(s, F_SANS, X(cx), Y(yy), L(15), "Diesel / autonomia", dim, AL_CENTER);
    r_text(s, F_SANS, X(cx), Y(yy + 24), L(20), b, fg, AL_CENTER);
    yy += 56;
    if (p->nav_distance > 10) {
        fmt_eta(e, sizeof e, p->nav_time);
        if (is_ats(d)) snprintf(b, sizeof b, "%.0f mi  %s", p->nav_distance / 1609.34f, e);
        else snprintf(b, sizeof b, "%.0f km  %s", p->nav_distance / 1000.0f, e);
        r_text(s, F_SANS, X(cx), Y(yy), L(15), p->dest_city[0] ? p->dest_city : "Rota", dim, AL_CENTER);
        r_text(s, F_SANS, X(cx), Y(yy + 24), L(20), b, fg, AL_CENTER);
    } else if (p->flags & TDF_ON_JOB) {
        r_text(s, F_SANS, X(cx), Y(yy), L(15), "Carga", dim, AL_CENTER);
        r_text(s, F_SANS, X(cx), Y(yy + 24), L(18), p->cargo, fg, AL_CENTER);
    }
    (void)h;
}

/* ---------------- layout TWIN (dois mostradores) ---------------- */
#define SP_X 215.0f
#define RP_X 785.0f
#define D_Y 305.0f
#define D_R 180.0f

static void twin_static(surf_t *s, const dash_t *d, const theme_t *t) {
    float smax = speed_max(d), rmax = rpm_max(d), rl = rpm_limit(d);
    char b[32];
    r_vgrad(s, 0, 0, s->w, s->h, t->bg0, t->bg1);
    draw_tell_static(s, t);

    for (int k = 0; k < 2; k++) {
        float cx = k ? RP_X : SP_X;
        bezel(s, t, cx, D_Y, D_R);
        r_radial(s, X(cx), Y(D_Y), L(D_R), t->face0, t->face1);
    }
    /* velocimetro */
    if (is_ats(d)) ticks(s, t, SP_X, D_Y, D_R, smax, 5, 10, 10, 1, 30);
    else ticks(s, t, SP_X, D_Y, D_R, smax, 5, 10, 20, 1, 30);
    r_text(s, F_SANS, X(SP_X), Y(D_Y - 48), L(18), is_ats(d) ? "mph" : "km/h", r_mix(t->numeral, t->face1, 0.35f), AL_CENTER);
    /* conta-giros: faixa verde e faixa vermelha */
    {
        float e0 = rl * 0.42f, e1 = rl * 0.62f;
        r_arc(s, X(RP_X), Y(D_Y), L(D_R - 14), L(D_R - 6), dial_angle(e0 / rmax), dial_angle(e1 / rmax), t->eco, 0.95f);
        r_arc(s, X(RP_X), Y(D_Y), L(D_R - 16), L(D_R - 6), dial_angle(rl / rmax), dial_angle(1), t->red, 1);
    }
    ticks(s, t, RP_X, D_Y, D_R, rmax, 100, 500, 500, 100, 30);
    snprintf(b, sizeof b, "x100 rpm");
    r_text(s, F_SANS, X(RP_X), Y(D_Y - 48), L(16), b, r_mix(t->numeral, t->face1, 0.35f), AL_CENTER);

    /* moldura das barras pequenas */
    uint32_t lab = r_mix(t->numeral, t->face1, 0.4f);
    icon(s, IC_FUEL, SP_X - 66, D_Y + 112, 20, lab, 1);
    r_text(s, F_SANS, X(SP_X - 66), Y(D_Y + 143), L(12), "AdB", lab, AL_CENTER);
    icon(s, IC_WATER, RP_X - 66, D_Y + 112, 20, lab, 1);
    r_text(s, F_SANS, X(RP_X - 66), Y(D_Y + 143), L(12), "AR", lab, AL_CENTER);

    /* display central */
    r_round_rect(s, X(415), Y(110), L(170), L(400), L(14), t->disp_bg, 1);
    r_round_rect_outline(s, X(415), Y(110), L(170), L(400), L(14), L(2), r_mix(t->bezel, t->bg0, 0.3f), 1);
}

static void twin_dynamic(surf_t *s, dash_t *d, const theme_t *t) {
    char b[32];
    const td_packet *p = &d->p;
    uint32_t track = r_mix(t->face1, HEX(0x808080), 0.18f);
    draw_tell_dynamic(s, d);

    /* barras: combustivel, adblue, agua, ar */
    uint32_t cf = (p->flags & TDF_FUEL_WARN) ? HEX(0xffa000) : t->numeral;
    hbar(s, SP_X - 48, D_Y + 106, 110, 11, d->n_fuel, cf, track);
    hbar(s, SP_X - 48, D_Y + 134, 110, 11, d->n_adblue, (p->flags & TDF_ADBLUE_WARN) ? HEX(0xffa000) : HEX(0x4aa8ff), track);
    float wf = r_clamp((d->n_water - 40.0f) / 80.0f, 0, 1);
    hbar(s, RP_X - 48, D_Y + 106, 110, 11, wf, (p->flags & TDF_WATER_WARN) ? t->red : t->numeral, track);
    hbar(s, RP_X - 48, D_Y + 134, 110, 11, d->n_air / air_max(d), (p->flags & (TDF_AIR_WARN | TDF_AIR_EMERG)) ? t->red : t->numeral, track);

    /* velocidade digital */
    snprintf(b, sizeof b, "%.0f", speed_disp(d, p->speed));
    r_text(s, F_BOLD, X(SP_X), Y(D_Y + 80), L(30), b, t->numeral, AL_CENTER);
    snprintf(b, sizeof b, "%.0f", d->n_rpm);
    r_text(s, F_BOLD, X(RP_X), Y(D_Y + 80), L(24), b, r_mix(t->numeral, t->face1, 0.25f), AL_CENTER);

    /* marcador do cruise no velocimetro */
    if (p->cruise > 0.1f) {
        float a = dial_angle(speed_disp(d, p->cruise) / speed_max(d));
        r_arc(s, X(SP_X), Y(D_Y), L(D_R - 4), L(D_R + 2), a - 1.6f * DEG, a + 1.6f * DEG, HEX(0x30d158), 1);
    }
    if (p->speed_limit > 0.5f) {
        float a = dial_angle(speed_disp(d, p->speed_limit) / speed_max(d));
        r_arc(s, X(SP_X), Y(D_Y), L(D_R - 4), L(D_R + 2), a - 1.2f * DEG, a + 1.2f * DEG, HEX(0xd71920), 1);
    }

    needle(s, t, SP_X, D_Y, D_R, d->n_speed / speed_max(d));
    needle(s, t, RP_X, D_Y, D_R, d->n_rpm / rpm_max(d));

    center_display(s, d, t, 415, 110, 170, 400);
    info_strip(s, d, t, 548);
}

/* ---------------- layout DIGITAL ---------------- */
#define DG_X 500.0f
#define DG_Y 360.0f
#define DG_R 250.0f
#define DG_A0 (150.0f * DEG)
#define DG_SW (240.0f * DEG)

static void digital_static(surf_t *s, const dash_t *d, const theme_t *t) {
    float rmax = rpm_max(d), rl = rpm_limit(d);
    r_vgrad(s, 0, 0, s->w, s->h, t->bg0, t->bg1);
    r_radial(s, X(DG_X), Y(DG_Y), L(DG_R + 30), r_mix(t->face0, t->accent, 0.06f), t->bg1);
    draw_tell_static(s, t);
    /* trilho do conta-giros */
    r_arc(s, X(DG_X), Y(DG_Y), L(DG_R - 22), L(DG_R), DG_A0, DG_A0 + DG_SW, r_mix(t->bg1, HEX(0xffffff), 0.08f), 1);
    r_arc(s, X(DG_X), Y(DG_Y), L(DG_R + 4), L(DG_R + 8), DG_A0 + DG_SW * (rl / rmax), DG_A0 + DG_SW, t->red, 1);
    int n = (int)(rmax / 500.0f + 0.5f);
    for (int i = 0; i <= n; i++) {
        float a = DG_A0 + DG_SW * (i * 500.0f / rmax), ca = r_cos(a), sa = r_sin(a);
        r_line(s, X(DG_X + ca * (DG_R - 26)), Y(DG_Y + sa * (DG_R - 26)), X(DG_X + ca * (DG_R - 40)), Y(DG_Y + sa * (DG_R - 40)), L(3), t->tick, 1);
        char b[16]; snprintf(b, sizeof b, "%d", i * 5);
        r_text(s, F_SANS, X(DG_X + ca * (DG_R - 62)), Y(DG_Y + sa * (DG_R - 62) + 8), L(22), b, t->numeral, AL_CENTER);
    }
    r_text(s, F_SANS, X(DG_X), Y(DG_Y + 30), L(18), is_ats(d) ? "mph" : "km/h", r_mix(t->numeral, t->bg1, 0.4f), AL_CENTER);

    /* paineis laterais */
    uint32_t pbg = r_mix(t->bg0, HEX(0xffffff), 0.04f);
    r_round_rect(s, X(30), Y(110), L(190), L(400), L(16), pbg, 1);
    r_round_rect(s, X(780), Y(110), L(190), L(400), L(16), pbg, 1);
    uint32_t lab = r_mix(t->disp_fg, t->bg0, 0.4f);
    icon(s, IC_FUEL, 80, 140, 28, lab, 1);
    icon(s, IC_ADBLUE, 170, 140, 28, lab, 1);
    icon(s, IC_AIR, 830, 140, 28, lab, 1);
    icon(s, IC_WATER, 920, 140, 28, lab, 1);
}

static void digital_dynamic(surf_t *s, dash_t *d, const theme_t *t) {
    char b[48];
    const td_packet *p = &d->p;
    float rmax = rpm_max(d), rl = rpm_limit(d);
    draw_tell_dynamic(s, d);

    float fr = r_clamp(d->n_rpm / rmax, 0, 1);
    uint32_t col = d->n_rpm > rl ? t->red : (d->n_rpm > rl * 0.42f && d->n_rpm < rl * 0.62f ? t->eco : t->accent);
    if (fr > 0.003f) {
        r_arc(s, X(DG_X), Y(DG_Y), L(DG_R - 22), L(DG_R), DG_A0, DG_A0 + DG_SW * fr, col, 1);
        float a = DG_A0 + DG_SW * fr;
        r_arc(s, X(DG_X), Y(DG_Y), L(DG_R - 26), L(DG_R + 4), a - 1.2f * DEG, a + 0.3f * DEG, HEX(0xffffff), 1);
    }

    snprintf(b, sizeof b, "%.0f", speed_disp(d, p->speed));
    r_text(s, F_BOLD, X(DG_X), Y(DG_Y - 5), L(150), b, t->disp_fg, AL_CENTER);
    gear_str(d, b, sizeof b);
    r_round_rect(s, X(DG_X - 55), Y(DG_Y + 50), L(110), L(70), L(12), r_mix(t->bg0, t->accent, 0.15f), 1);
    r_text(s, F_BOLD, X(DG_X), Y(DG_Y + 105), L(54), b, t->accent, AL_CENTER);

    if (p->cruise > 0.1f) {
        icon(s, IC_CRUISE, DG_X - 250, DG_Y - 192, 30, HEX(0x30d158), 1);
        snprintf(b, sizeof b, "%.0f", speed_disp(d, p->cruise));
        r_text(s, F_BOLD, X(DG_X - 225), Y(DG_Y - 180), L(32), b, HEX(0x30d158), AL_LEFT);
    }
    if (p->speed_limit > 0.5f) speed_limit_sign(s, DG_X + 215, DG_Y - 190, 30, (int)(speed_disp(d, p->speed_limit) + 0.5f));

    uint32_t track = r_mix(t->bg0, HEX(0xffffff), 0.10f);
    vbar(s, 66, 170, 28, 250, d->n_fuel, (p->flags & TDF_FUEL_WARN) ? HEX(0xffa000) : t->accent, track);
    vbar(s, 156, 170, 28, 250, d->n_adblue, (p->flags & TDF_ADBLUE_WARN) ? HEX(0xffa000) : HEX(0x4aa8ff), track);
    vbar(s, 816, 170, 28, 250, d->n_air / air_max(d), (p->flags & (TDF_AIR_WARN | TDF_AIR_EMERG)) ? t->red : t->accent, track);
    vbar(s, 906, 170, 28, 250, r_clamp((d->n_water - 40) / 80, 0, 1), (p->flags & TDF_WATER_WARN) ? t->red : t->accent, track);

    if (is_ats(d)) snprintf(b, sizeof b, "%.0f gal", p->fuel * 0.264172f);
    else snprintf(b, sizeof b, "%.0f L", p->fuel);
    r_text(s, F_SANS, X(80), Y(450), L(18), b, t->disp_fg, AL_CENTER);
    snprintf(b, sizeof b, "%.0f%%", d->n_adblue * 100);
    r_text(s, F_SANS, X(170), Y(450), L(18), b, t->disp_fg, AL_CENTER);
    if (is_ats(d)) snprintf(b, sizeof b, "%.0f mi", p->fuel_range * 0.621371f);
    else snprintf(b, sizeof b, "%.0f km", p->fuel_range);
    r_text(s, F_SANS, X(125), Y(490), L(18), b, r_mix(t->disp_fg, t->bg0, 0.3f), AL_CENTER);
    snprintf(b, sizeof b, "%.1f", d->n_air);
    r_text(s, F_SANS, X(830), Y(450), L(18), b, t->disp_fg, AL_CENTER);
    snprintf(b, sizeof b, "%.0f\xc2\xb0", d->n_water);
    r_text(s, F_SANS, X(920), Y(450), L(18), b, t->disp_fg, AL_CENTER);
    uint32_t gt = p->game_time;
    snprintf(b, sizeof b, "%s %02u:%02u", DAYS[(gt / 1440) % 7], (gt % 1440) / 60, gt % 60);
    r_text(s, F_SANS, X(875), Y(490), L(18), b, r_mix(t->disp_fg, t->bg0, 0.3f), AL_CENTER);

    if (p->nav_distance > 10) {
        char e[16]; fmt_eta(e, sizeof e, p->nav_time);
        if (is_ats(d)) snprintf(b, sizeof b, "%s  %.0f mi  %s", p->dest_city, p->nav_distance / 1609.34f, e);
        else snprintf(b, sizeof b, "%s  %.0f km  %s", p->dest_city, p->nav_distance / 1000.0f, e);
        r_text(s, F_SANS, X(DG_X), Y(DG_Y + 160), L(20), b, r_mix(t->disp_fg, t->bg0, 0.2f), AL_CENTER);
    }
    info_strip(s, d, t, 548);
}

/* ---------------- tela de espera ---------------- */
static void waiting_static(surf_t *s, const dash_t *d) {
    (void)d;
    r_vgrad(s, 0, 0, s->w, s->h, HEX(0x101318), HEX(0x000000));
    r_arc(s, X(500), Y(210), L(70), L(82), 0, 2 * PI, HEX(0x2a3038), 1);
    r_text(s, F_BOLD, X(500), Y(340), L(54), "TruckDash", HEX(0xffffff), AL_CENTER);
}

static void waiting_dynamic(surf_t *s, dash_t *d) {
    char b[96];
    float a = d->t * 4.0f;
    r_arc(s, X(500), Y(210), L(70), L(82), a, a + 1.4f, HEX(0xff5a1f), 1);
    const char *msg = d->have ? (d->p.flags & TDF_SDK_ACTIVE ? "Conexao perdida - aguardando dados..." : "O jogo foi fechado.")
                              : "Aguardando o Euro Truck Simulator 2...";
    r_text(s, F_SANS, X(500), Y(390), L(24), msg, HEX(0xc9d1da), AL_CENTER);
    snprintf(b, sizeof b, "IP do tablet: %s", d->local_ip[0] ? d->local_ip : "sem rede (Wi-Fi/USB desligados?)");
    r_text(s, F_SANS, X(500), Y(440), L(18), b, HEX(0x8892a0), AL_CENTER);
    r_text(s, F_SANS, X(500), Y(530), L(16), d->usb ? "USB (adb): PC conectado pelo cabo" : "USB (adb): aguardando - rode o TruckDash-USB.bat no PC",
           d->usb ? HEX(0x30d158) : HEX(0x6b7480), AL_CENTER);
    r_text(s, F_SANS, X(500), Y(470), L(16), "PC e tablet na mesma rede Wi-Fi. Plugin: ETS2\\bin\\win_x64\\plugins\\TruckDash.dll",
           HEX(0x6b7480), AL_CENTER);
    r_text(s, F_SANS, X(500), Y(495), L(16), "Toque na tela para trocar o painel (Automatico = pela marca do caminhao)",
           HEX(0x6b7480), AL_CENTER);
}

/* ---------------- API ---------------- */
void panel_draw_static(surf_t *s, const dash_t *d) {
    set_view(s);
    if (!d->connected) { waiting_static(s, d); return; }
    const theme_t *t = &TH[d->theme];
    if (t->layout == L_DIGITAL) digital_static(s, d, t); else twin_static(s, d, t);
}

void panel_draw_dynamic(surf_t *s, dash_t *d) {
    set_view(s);
    if (!d->connected) waiting_dynamic(s, d);
    else {
        const theme_t *t = &TH[d->theme];
        if (t->layout == L_DIGITAL) digital_dynamic(s, d, t); else twin_dynamic(s, d, t);
        if (!(d->p.flags & TDF_ELECTRIC)) {
            r_fill_rect(s, 0, 0, s->w, s->h, HEX(0x000000), 0.55f);
            r_text(s, F_SANS, X(500), Y(300), L(26), "Painel desligado (ignicao)", HEX(0xbfc6cf), AL_CENTER);
        }
        if (d->p.flags & TDF_PAUSED)
            r_text(s, F_BOLD, X(500), Y(95), L(20), "PAUSADO", HEX(0xffc240), AL_CENTER);
        char b[128];
        snprintf(b, sizeof b, "%s %s  |  %s%s  |  %s", d->p.brand, d->p.truck_name, d->manual ? "" : "Auto: ", TH[d->theme].name, d->usb ? "USB" : "Wi-Fi");
        r_text(s, F_SANS, X(500), Y(592), L(13), b, HEX(0x5d6570), AL_CENTER);
    }
    if (d->t < d->toast_until) {
        float w = r_text_width(F_SANS, L(24), d->toast) + L(40);
        r_round_rect(s, X(500) - w / 2, Y(250), w, L(56), L(14), HEX(0x000000), 0.8f);
        r_text(s, F_SANS, X(500), Y(286), L(24), d->toast, HEX(0xffffff), AL_CENTER);
    }
}

static float approach(float v, float target, float k, float dt) {
    float a = dt * k; if (a > 1) a = 1;
    return v + (target - v) * a;
}

void panel_update(dash_t *d, float dt) {
    const td_packet *p = &d->p;
    int on = d->connected && (p->flags & TDF_ELECTRIC);
    d->t += dt;
    d->dt = dt;
    float sp = on ? speed_disp(d, p->speed) : 0;
    float rpm = on ? p->rpm : 0;
    float fuel = (on && p->fuel_capacity > 1) ? p->fuel / p->fuel_capacity : 0;
    float adb = (on && p->adblue_capacity > 1) ? p->adblue / p->adblue_capacity : 0;
    d->n_speed = approach(d->n_speed, sp, 10, dt);
    d->n_rpm = approach(d->n_rpm, rpm, 10, dt);
    d->n_fuel = approach(d->n_fuel, fuel, 3, dt);
    d->n_adblue = approach(d->n_adblue, adb, 3, dt);
    d->n_air = approach(d->n_air, on ? air_disp(d, p->air_pressure) : 0, 4, dt);
    d->n_water = approach(d->n_water, on ? p->water_temp : 0, 3, dt);
    d->n_oil = approach(d->n_oil, on ? p->oil_temp : 0, 3, dt);
    if (!d->manual) d->theme = panel_auto_theme(p);
}
