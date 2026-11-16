/* Tela de espera, avisos e suavizacao dos valores. O painel em si fica em skin.c */
#include "panel.h"
#include "plat.h"

#define PI 3.14159265358979f
static float S, OX, OY;               /* coordenadas virtuais 1000 x 600 -> tela */
#define X(v) (OX + (v) * S)
#define Y(v) (OY + (v) * S)
#define L(v) ((v) * S)

static void set_view(const surf_t *s) {
    float sx = s->w / 1000.0f, sy = s->h / 600.0f;
    S = sx < sy ? sx : sy;
    OX = (s->w - 1000.0f * S) * 0.5f;
    OY = (s->h - 600.0f * S) * 0.5f;
}

int panel_is_ats(const dash_t *d) { return d->p.game_id[0] == 'a'; }

void panel_draw_waiting(surf_t *s, dash_t *d) {
    char b[96];
    float a = d->t * 4.0f;
    set_view(s);
    r_clear(s, HEX(0x07090c));
    r_arc(s, X(500), Y(170), L(60), L(70), 0, 2 * PI, HEX(0x2a3038), 1);
    r_arc(s, X(500), Y(170), L(60), L(70), a, a + 1.4f, HEX(0xff5a1f), 1);
    r_text(s, F_BOLD, X(500), Y(300), L(50), "Scania Smart Dash", HEX(0xffffff), AL_CENTER);
    const char *msg = d->have ? (d->p.flags & TDF_SDK_ACTIVE ? "Conexao perdida - aguardando dados..." : "O jogo foi fechado.")
                              : "Aguardando o Euro Truck Simulator 2...";
    r_text(s, F_SANS, X(500), Y(350), L(24), msg, HEX(0xc9d1da), AL_CENTER);
    snprintf(b, sizeof b, "IP do tablet: %s", d->local_ip[0] ? d->local_ip : "sem rede (Wi-Fi/USB desligados?)");
    r_text(s, F_SANS, X(500), Y(400), L(18), b, HEX(0x8892a0), AL_CENTER);
    r_text(s, F_SANS, X(500), Y(430), L(16), d->usb ? "USB (adb): PC conectado pelo cabo" : "USB (adb): aguardando - rode o TruckDash-USB.bat no PC",
           d->usb ? HEX(0x30d158) : HEX(0x6b7480), AL_CENTER);
    r_text(s, F_SANS, X(500), Y(490), L(16), d->status, HEX(0xffc240), AL_CENTER);
    r_text(s, F_SANS, X(500), Y(530), L(16), "Toque longo a esquerda: recarrega os PNGs   |   Toque longo a direita: demonstracao sem o jogo",
           HEX(0x6b7480), AL_CENTER);
}

void panel_draw_message(surf_t *s, const char *title, const char *l1, const char *l2) {
    set_view(s);
    r_clear(s, HEX(0x07090c));
    r_text(s, F_BOLD, X(500), Y(240), L(36), title, HEX(0xffffff), AL_CENTER);
    r_text(s, F_SANS, X(500), Y(310), L(18), l1, HEX(0xffc240), AL_CENTER);
    r_text(s, F_SANS, X(500), Y(350), L(18), l2, HEX(0x8892a0), AL_CENTER);
}

void panel_draw_toast(surf_t *s, dash_t *d) {
    if (d->t >= d->toast_until) return;
    set_view(s);
    float w = r_text_width(F_SANS, L(24), d->toast) + L(40);
    r_round_rect(s, X(500) - w / 2, Y(250), w, L(56), L(14), HEX(0x000000), 0.8f);
    r_text(s, F_SANS, X(500), Y(286), L(24), d->toast, HEX(0xffffff), AL_CENTER);
}

static float approach(float v, float target, float k, float dt) {
    float a = dt * k; if (a > 1) a = 1;
    return v + (target - v) * a;
}

void panel_update(dash_t *d, float dt) {
    const td_packet *p = &d->p;
    int on = d->connected && (p->flags & TDF_ELECTRIC);
    int ats = panel_is_ats(d);
    d->t += dt;
    d->dt = dt;
    float sp = on ? p->speed * (ats ? 2.2369363f : 3.6f) : 0;
    if (sp < 0) sp = -sp;
    d->n_speed = approach(d->n_speed, sp, 10, dt);
    d->n_rpm = approach(d->n_rpm, on ? p->rpm : 0, 10, dt);
    d->n_fuel = approach(d->n_fuel, (on && p->fuel_capacity > 1) ? p->fuel / p->fuel_capacity : 0, 3, dt);
    d->n_adblue = approach(d->n_adblue, (on && p->adblue_capacity > 1) ? p->adblue / p->adblue_capacity : 0, 3, dt);
    d->n_air = approach(d->n_air, on ? (ats ? p->air_pressure : p->air_pressure * 0.0689476f) : 0, 4, dt);
    d->n_water = approach(d->n_water, on ? p->water_temp : 0, 3, dt);
    d->n_oil = approach(d->n_oil, on ? p->oil_temp : 0, 3, dt);

    /* viagem (hodometro parcial) e consumo em litros por hora, calculados no tablet */
    if (d->connected && p->odometer > 0) {
        if (d->trip_odo0 <= 0) { d->trip_odo0 = p->odometer; d->trip_t0 = p->game_time; d->last_fuel = p->fuel; }
        d->trip_km = p->odometer - d->trip_odo0;
        d->trip_min = (float)(p->game_time - (uint32_t)d->trip_t0);
        if (!(p->flags & TDF_PAUSED) && dt > 0) {
            float used = d->last_fuel - p->fuel, lh = 0;
            if (used >= 0 && used < 1) lh = used / dt * 3600.0f;   /* abastecer nao conta */
            if (!(p->flags & TDF_ENGINE)) lh = 0;
            d->fuel_lh = approach(d->fuel_lh, lh, 0.4f, dt);
        }
        d->last_fuel = p->fuel;
    }
}
