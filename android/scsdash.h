#ifndef SCSDASH_H
#define SCSDASH_H
/* Interpretador dos paineis "de verdade" do ETS2/ATS, convertidos para .tdp (tools/tdpack.py). */
#include "render.h"

typedef struct sd_pack sd_pack;

typedef struct {
    int page;                      /* pagina do display (1..n) */
    float ox, oy, scale;           /* unidades do painel -> pixels */
    void *user;
    /* texto do elemento (formato "valor|unidade"); NULL = esconde */
    const char *(*text)(void *user, int id, char *buf, int n);
    /* valor numerico de barras e ponteiros; retorna 0 se nao houver */
    int (*num)(void *user, int id, float *out);
    /* visibilidade de icones/grupos */
    int (*visible)(void *user, int id);
} sd_ctx;

sd_pack *sd_load(const char *path);
void sd_free(sd_pack *p);
int sd_width(const sd_pack *p);
int sd_height(const sd_pack *p);
int sd_pages(const sd_pack *p);
/* pass 0: so o que e fixo (vai para o fundo em cache); pass 1: so o que muda */
void sd_draw(surf_t *s, const sd_pack *p, const sd_ctx *c, int pass);

#endif
