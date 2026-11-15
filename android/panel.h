#ifndef PANEL_H
#define PANEL_H
#include "render.h"
#include "../common/td_proto.h"

#define THEME_COUNT 10

typedef struct {
    td_packet p;
    int have;          /* ja recebeu algum pacote */
    int connected;     /* recebeu pacote recentemente */
    int theme;         /* tema em uso */
    int manual;        /* 0 = automatico pela marca */
    float t;           /* tempo em segundos (piscas) */
    float dt;
    /* valores suavizados para os ponteiros */
    float n_speed, n_rpm, n_fuel, n_adblue, n_air, n_water, n_oil;
    char server[32];   /* IP do PC */
    char local_ip[48]; /* IP do tablet */
    int usb;
    char toast[64];
    float toast_until;
} dash_t;

int panel_auto_theme(const td_packet *p);
const char *panel_theme_name(int theme);
/* chave que muda quando o fundo precisa ser redesenhado */
uint32_t panel_static_key(const dash_t *d, int w, int h);
void panel_draw_static(surf_t *s, const dash_t *d);
void panel_draw_dynamic(surf_t *s, dash_t *d);
void panel_update(dash_t *d, float dt);
void panel_draw_extras(surf_t *s, dash_t *d, float top_h, float bot_y, float bot_h);
int panel_is_ats(const dash_t *d);

#endif
