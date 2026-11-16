/* Painel por camadas PNG: um fundo + luzes (PNG aceso / PNG apagado) +
 * ponteiros, barras, arcos e numeros, tudo descrito em painel.txt */
#ifndef SKIN_H
#define SKIN_H
#include "panel.h"

typedef struct skin skin_t;

skin_t *skin_load(const char *dir);          /* le <dir>/painel.txt e os PNGs */
void skin_free(skin_t *k);
int skin_ok(const skin_t *k);                /* tem tamanho/fundo valido */
int skin_items(const skin_t *k);
const char *skin_error(const skin_t *k);     /* "" se nao houve problema */
int skin_pages(const skin_t *k);              /* quantas telas (carrossel) */
void skin_draw(surf_t *fb, skin_t *k, const dash_t *d, int page);   /* page comeca em 0 */

#endif
