#ifndef RENDER_H
#define RENDER_H
#include <stdint.h>

/* pixel = 0xAABBGGRR (bytes R,G,B,A na memoria = RGBX_8888 do Android) */
typedef struct { uint32_t *px; int w, h, stride; } surf_t;

#define RGB(r,g,b) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | 0xff000000u)
#define HEX(h) RGB(((h) >> 16) & 255, ((h) >> 8) & 255, (h) & 255)

enum { F_SANS = 0, F_BOLD = 1, F_COND = 2 };
enum { AL_LEFT = 0, AL_CENTER = 1, AL_RIGHT = 2 };

float r_sqrt(float x);
float r_sin(float x);
float r_cos(float x);
float r_atan2(float y, float x);
float r_fabs(float x);
float r_clamp(float v, float a, float b);
uint32_t r_mix(uint32_t a, uint32_t b, float t);
uint32_t r_scale(uint32_t c, float k);

void r_clear(surf_t *s, uint32_t c);
void r_fill_rect(surf_t *s, float x, float y, float w, float h, uint32_t c, float alpha);
void r_round_rect(surf_t *s, float x, float y, float w, float h, float rad, uint32_t c, float alpha);
void r_round_rect_outline(surf_t *s, float x, float y, float w, float h, float rad, float lw, uint32_t c, float alpha);
void r_vgrad(surf_t *s, float x, float y, float w, float h, uint32_t top, uint32_t bot);
void r_circle(surf_t *s, float cx, float cy, float r, uint32_t c, float alpha);
void r_radial(surf_t *s, float cx, float cy, float r, uint32_t inner, uint32_t outer);
/* anel entre r0 e r1, angulos em radianos (0 = direita, sentido horario na tela) */
void r_arc(surf_t *s, float cx, float cy, float r0, float r1, float a0, float a1, uint32_t c, float alpha);
void r_poly(surf_t *s, const float *xy, int n, uint32_t c, float alpha);  /* convexo */
void r_line(surf_t *s, float x0, float y0, float x1, float y1, float w, uint32_t c, float alpha);
/* desenha texto; y = linha de base. retorna largura */
float r_text(surf_t *s, int font, float x, float y, float px, const char *str, uint32_t c, int align);
float r_text_width(int font, float px, const char *str);
void r_blit(surf_t *dst, const surf_t *src);

#endif
