#ifndef PANEL_H
#define PANEL_H
#include "render.h"
#include "../common/td_proto.h"

/* estado compartilhado entre a rede, a tela de espera e o painel */
typedef struct {
    td_packet p;
    int have;          /* ja recebeu algum pacote */
    int connected;     /* recebeu pacote recentemente (ou modo demonstracao) */
    float t;           /* tempo em segundos */
    float dt;
    /* valores suavizados (ponteiros, barras e numeros nao "pulam") */
    float n_speed, n_rpm, n_fuel, n_adblue, n_air, n_water, n_oil;
    char server[32];   /* IP do PC */
    char local_ip[48]; /* IP do tablet */
    int usb;
    char status[160];  /* situacao do painel (carregado / erro) */
    char toast[64];
    float toast_until;
} dash_t;

void panel_update(dash_t *d, float dt);
int panel_is_ats(const dash_t *d);
void panel_draw_waiting(surf_t *s, dash_t *d);
void panel_draw_message(surf_t *s, const char *title, const char *l1, const char *l2);
void panel_draw_toast(surf_t *s, dash_t *d);

#endif
