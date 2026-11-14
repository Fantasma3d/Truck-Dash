/* Renderizador por software com antialiasing (sem OpenGL, roda em qualquer Android) */
#include "render.h"
#include "font_data.h"

#define PI 3.14159265358979f

float r_fabs(float x) { return x < 0 ? -x : x; }
float r_sqrt(float x) { return x <= 0 ? 0 : __builtin_sqrtf(x); }
float r_clamp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

float r_sin(float x) {
    /* reduz para [-pi, pi] */
    float k = x * (1.0f / (2 * PI));
    int ki = (int)(k + (k >= 0 ? 0.5f : -0.5f));
    x -= ki * 2 * PI;
    /* aproximacao parabolica refinada */
    float y = 1.27323954f * x - 0.405284735f * x * r_fabs(x);
    return 0.225f * (y * r_fabs(y) - y) + y;
}
float r_cos(float x) { return r_sin(x + PI / 2); }

float r_atan2(float y, float x) {
    float ax = r_fabs(x), ay = r_fabs(y);
    float mn = ax < ay ? ax : ay, mx = ax < ay ? ay : ax;
    if (mx == 0) return 0;
    float a = mn / mx, s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = PI - r;
    if (y < 0) r = -r;
    return r;
}

uint32_t r_mix(uint32_t a, uint32_t b, float t) {
    int k = (int)(r_clamp(t, 0, 1) * 256);
    uint32_t rb = ((a & 0xff00ff) * (256 - k) + (b & 0xff00ff) * k) >> 8;
    uint32_t g = ((a & 0x00ff00) * (256 - k) + (b & 0x00ff00) * k) >> 8;
    return (rb & 0xff00ff) | (g & 0x00ff00) | 0xff000000u;
}
uint32_t r_scale(uint32_t c, float k) {
    int r = (int)((c & 255) * k), g = (int)(((c >> 8) & 255) * k), b = (int)(((c >> 16) & 255) * k);
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return RGB(r, g, b);
}

static inline void blend(uint32_t *p, uint32_t c, int a /*0..256*/) {
    if (a <= 0) return;
    if (a >= 256) { *p = c; return; }
    uint32_t d = *p;
    uint32_t rb = ((d & 0xff00ff) * (256 - a) + (c & 0xff00ff) * a) >> 8;
    uint32_t g = ((d & 0x00ff00) * (256 - a) + (c & 0x00ff00) * a) >> 8;
    *p = (rb & 0xff00ff) | (g & 0x00ff00) | 0xff000000u;
}
static inline int cov_i(float cov, float alpha) {
    if (cov <= 0) return 0;
    if (cov > 1) cov = 1;
    return (int)(cov * alpha * 256.0f + 0.5f);
}
static inline int fl(float v) { int i = (int)v; return (v < i) ? i - 1 : i; }

void r_clear(surf_t *s, uint32_t c) {
    for (int y = 0; y < s->h; y++) { uint32_t *p = s->px + y * s->stride; for (int x = 0; x < s->w; x++) p[x] = c; }
}

void r_blit(surf_t *dst, const surf_t *src) {
    int h = dst->h < src->h ? dst->h : src->h, w = dst->w < src->w ? dst->w : src->w;
    for (int y = 0; y < h; y++) {
        uint32_t *d = dst->px + y * dst->stride; const uint32_t *sp = src->px + y * src->stride;
        __builtin_memcpy(d, sp, (unsigned)w * 4);
    }
}

void r_fill_rect(surf_t *s, float x, float y, float w, float h, uint32_t c, float alpha) {
    int x0 = fl(x), y0 = fl(y), x1 = fl(x + w + 0.999f), y1 = fl(y + h + 0.999f);
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > s->w) x1 = s->w; if (y1 > s->h) y1 = s->h;
    for (int py = y0; py < y1; py++) {
        float cy = r_clamp(py + 1 - y, 0, 1) * r_clamp(y + h - py, 0, 1);
        uint32_t *row = s->px + py * s->stride;
        for (int px = x0; px < x1; px++) {
            float cx = r_clamp(px + 1 - x, 0, 1) * r_clamp(x + w - px, 0, 1);
            blend(row + px, c, cov_i(cx * cy, alpha));
        }
    }
}

static float rr_sdf(float px, float py, float x, float y, float w, float h, float r) {
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float qx = r_fabs(px - cx) - (w * 0.5f - r), qy = r_fabs(py - cy) - (h * 0.5f - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float in = qx > qy ? qx : qy; if (in > 0) in = 0;
    return r_sqrt(ox * ox + oy * oy) + in - r;
}

void r_round_rect(surf_t *s, float x, float y, float w, float h, float rad, uint32_t c, float alpha) {
    int x0 = fl(x) - 1, y0 = fl(y) - 1, x1 = fl(x + w) + 2, y1 = fl(y + h) + 2;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > s->w) x1 = s->w; if (y1 > s->h) y1 = s->h;
    for (int py = y0; py < y1; py++) {
        uint32_t *row = s->px + py * s->stride;
        for (int px = x0; px < x1; px++) {
            float d = rr_sdf(px + 0.5f, py + 0.5f, x, y, w, h, rad);
            blend(row + px, c, cov_i(0.5f - d, alpha));
        }
    }
}

void r_round_rect_outline(surf_t *s, float x, float y, float w, float h, float rad, float lw, uint32_t c, float alpha) {
    int x0 = fl(x) - 2, y0 = fl(y) - 2, x1 = fl(x + w) + 3, y1 = fl(y + h) + 3;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > s->w) x1 = s->w; if (y1 > s->h) y1 = s->h;
    for (int py = y0; py < y1; py++) {
        uint32_t *row = s->px + py * s->stride;
        for (int px = x0; px < x1; px++) {
            float d = rr_sdf(px + 0.5f, py + 0.5f, x, y, w, h, rad);
            float cov = r_clamp(0.5f - d, 0, 1) * r_clamp(d + lw + 0.5f, 0, 1);
            blend(row + px, c, cov_i(cov, alpha));
        }
    }
}

void r_vgrad(surf_t *s, float x, float y, float w, float h, uint32_t top, uint32_t bot) {
    int x0 = fl(x), y0 = fl(y), x1 = fl(x + w), y1 = fl(y + h);
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > s->w) x1 = s->w; if (y1 > s->h) y1 = s->h;
    for (int py = y0; py < y1; py++) {
        uint32_t c = r_mix(top, bot, (py - y) / (h > 1 ? h : 1));
        uint32_t *row = s->px + py * s->stride;
        for (int px = x0; px < x1; px++) row[px] = c;
    }
}

/* anel / arco com spans por linha (rapido o suficiente para uso dinamico) */
void r_arc(surf_t *s, float cx, float cy, float r0, float r1, float a0, float a1, uint32_t c, float alpha) {
    if (r0 < 0) r0 = 0;
    float span = a1 - a0;
    int full = span >= 2 * PI - 0.0001f;
    if (span <= 0) return;
    float mid = a0 + span * 0.5f, half = span * 0.5f;
    int y0 = fl(cy - r1 - 1), y1 = fl(cy + r1 + 2);
    if (y0 < 0) y0 = 0; if (y1 > s->h) y1 = s->h;
    float ro = r1 + 1, ri = r0 - 1;
    for (int py = y0; py < y1; py++) {
        float dy = py + 0.5f - cy;
        if (dy * dy > ro * ro) continue;
        float xo = r_sqrt(ro * ro - dy * dy);
        float xi = (ri > 0 && dy * dy < ri * ri) ? r_sqrt(ri * ri - dy * dy) : 0;
        uint32_t *row = s->px + py * s->stride;
        for (int side = 0; side < 2; side++) {
            int xa, xb;
            if (xi > 0) {
                if (side == 0) { xa = fl(cx - xo); xb = fl(cx - xi) + 1; }
                else { xa = fl(cx + xi); xb = fl(cx + xo) + 1; }
            } else {
                if (side == 1) break;
                xa = fl(cx - xo); xb = fl(cx + xo) + 1;
            }
            if (xa < 0) xa = 0; if (xb > s->w) xb = s->w;
            for (int px = xa; px < xb; px++) {
                float dx = px + 0.5f - cx;
                float d = r_sqrt(dx * dx + dy * dy);
                float cov = r_clamp(r1 - d + 0.5f, 0, 1);
                if (r0 > 0) cov *= r_clamp(d - r0 + 0.5f, 0, 1);
                if (cov <= 0) continue;
                if (!full) {
                    float t = r_atan2(dy, dx) - mid;
                    while (t > PI) t -= 2 * PI;
                    while (t < -PI) t += 2 * PI;
                    float dist = (half - r_fabs(t)) * (d > 1 ? d : 1);
                    cov *= r_clamp(dist + 0.5f, 0, 1);
                }
                blend(row + px, c, cov_i(cov, alpha));
            }
        }
    }
}

void r_circle(surf_t *s, float cx, float cy, float r, uint32_t c, float alpha) {
    r_arc(s, cx, cy, 0, r, 0, 2 * PI, c, alpha);
}

void r_radial(surf_t *s, float cx, float cy, float r, uint32_t inner, uint32_t outer) {
    int y0 = fl(cy - r - 1), y1 = fl(cy + r + 2), x0 = fl(cx - r - 1), x1 = fl(cx + r + 2);
    if (y0 < 0) y0 = 0; if (y1 > s->h) y1 = s->h; if (x0 < 0) x0 = 0; if (x1 > s->w) x1 = s->w;
    for (int py = y0; py < y1; py++) {
        uint32_t *row = s->px + py * s->stride;
        float dy = py + 0.5f - cy;
        for (int px = x0; px < x1; px++) {
            float dx = px + 0.5f - cx, d = r_sqrt(dx * dx + dy * dy);
            float t = d / r;
            blend(row + px, r_mix(inner, outer, t * t), cov_i(r - d + 0.5f, 1));
        }
    }
}

void r_poly(surf_t *s, const float *xy, int n, uint32_t c, float alpha) {
    float nx[16], ny[16], nd[16];
    float area = 0, miny = 1e9f, maxy = -1e9f;
    if (n < 3 || n > 16) return;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area += xy[2 * i] * xy[2 * j + 1] - xy[2 * j] * xy[2 * i + 1];
        if (xy[2 * i + 1] < miny) miny = xy[2 * i + 1];
        if (xy[2 * i + 1] > maxy) maxy = xy[2 * i + 1];
    }
    float sgn = area > 0 ? 1.0f : -1.0f;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        float ex = xy[2 * j] - xy[2 * i], ey = xy[2 * j + 1] - xy[2 * i + 1];
        float len = r_sqrt(ex * ex + ey * ey); if (len < 1e-6f) len = 1e-6f;
        /* normal para fora */
        nx[i] = sgn * ey / len; ny[i] = -sgn * ex / len;
        nd[i] = nx[i] * xy[2 * i] + ny[i] * xy[2 * i + 1];
    }
    int y0 = fl(miny) - 1, y1 = fl(maxy) + 2;
    if (y0 < 0) y0 = 0; if (y1 > s->h) y1 = s->h;
    for (int py = y0; py < y1; py++) {
        /* faixa x do poligono entre py-1 e py+2 */
        float xmin = 1e9f, xmax = -1e9f, ya = py - 1.0f, yb = py + 2.0f;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            float ax = xy[2 * i], ay = xy[2 * i + 1], bx = xy[2 * j], by = xy[2 * j + 1];
            if (ay >= ya && ay <= yb) { if (ax < xmin) xmin = ax; if (ax > xmax) xmax = ax; }
            float lo = ay < by ? ay : by, hi = ay < by ? by : ay;
            for (int k = 0; k < 2; k++) {
                float yy = k ? yb : ya;
                if (yy >= lo && yy <= hi && hi > lo) {
                    float xx = ax + (bx - ax) * (yy - ay) / (by - ay);
                    if (xx < xmin) xmin = xx; if (xx > xmax) xmax = xx;
                }
            }
        }
        if (xmin > xmax) continue;
        int xa = fl(xmin) - 1, xb = fl(xmax) + 2;
        if (xa < 0) xa = 0; if (xb > s->w) xb = s->w;
        uint32_t *row = s->px + py * s->stride;
        float fy = py + 0.5f;
        for (int px = xa; px < xb; px++) {
            float fx = px + 0.5f, m = -1e9f;
            for (int i = 0; i < n; i++) { float d = nx[i] * fx + ny[i] * fy - nd[i]; if (d > m) m = d; }
            blend(row + px, c, cov_i(0.5f - m, alpha));
        }
    }
}

void r_line(surf_t *s, float x0, float y0, float x1, float y1, float w, uint32_t c, float alpha) {
    float dx = x1 - x0, dy = y1 - y0, len = r_sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) return;
    float hx = -dy / len * w * 0.5f, hy = dx / len * w * 0.5f;
    float p[8] = { x0 + hx, y0 + hy, x1 + hx, y1 + hy, x1 - hx, y1 - hy, x0 - hx, y0 - hy };
    r_poly(s, p, 4, c, alpha);
}

/* ---------------- texto ---------------- */
static int glyph_index(const unsigned char **ps) {
    const unsigned char *p = *ps;
    unsigned c = *p++;
    if (c >= 0xC0 && (*p & 0xC0) == 0x80) {          /* UTF-8 de 2 bytes */
        c = ((c & 0x1F) << 6) | (*p++ & 0x3F);
    } else if (c >= 0xE0) {                          /* outros: pula */
        while ((*p & 0xC0) == 0x80) p++;
        *ps = p; return '?' - 32;
    }
    *ps = p;
    if (c >= 32 && c < 127) return (int)c - 32;
    if (c == 0xB0 || c == 0xBA) return 95;           /* grau */
    static const char map[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
    if (c >= 0xC0 && c <= 0xFF) return map[c - 0xC0] - 32;
    return '?' - 32;
}

static const fontsize_t *pick(int font, float px) {
    const fontsize_t *best = &g_fonts[font][0];
    for (int i = 0; i < FONT_SIZES; i++)
        if (g_fonts[font][i].px >= px * 0.95f) best = &g_fonts[font][i];
    return best;
}

float r_text_width(int font, float px, const char *str) {
    const fontsize_t *f = pick(font, px);
    float sc = px / f->px, w = 0;
    const unsigned char *p = (const unsigned char *)str;
    while (*p) w += f->g[glyph_index(&p)].adv * sc;
    return w;
}

float r_text(surf_t *s, int font, float x, float y, float px, const char *str, uint32_t c, int align) {
    const fontsize_t *f = pick(font, px);
    float sc = px / f->px, inv = 1.0f / sc;
    float w = r_text_width(font, px, str);
    if (align == AL_CENTER) x -= w * 0.5f; else if (align == AL_RIGHT) x -= w;
    const unsigned char *p = (const unsigned char *)str;
    while (*p) {
        const glyph_t *g = &f->g[glyph_index(&p)];
        if (g->w > 0) {
            float gx = x + g->xoff * sc, gy = y + g->yoff * sc;
            int dx0 = fl(gx), dy0 = fl(gy), dx1 = fl(gx + g->w * sc) + 1, dy1 = fl(gy + g->h * sc) + 1;
            if (dx0 < 0) dx0 = 0; if (dy0 < 0) dy0 = 0; if (dx1 > s->w) dx1 = s->w; if (dy1 > s->h) dy1 = s->h;
            for (int py = dy0; py < dy1; py++) {
                float sy = (py + 0.5f - gy) * inv - 0.5f;
                int iy = fl(sy); float fy = sy - iy;
                uint32_t *row = s->px + py * s->stride;
                for (int px2 = dx0; px2 < dx1; px2++) {
                    float sx = (px2 + 0.5f - gx) * inv - 0.5f;
                    int ix = fl(sx); float fx = sx - ix;
                    float a = 0;
                    for (int k = 0; k < 4; k++) {
                        int xx = ix + (k & 1), yy = iy + (k >> 1);
                        if (xx < 0 || yy < 0 || xx >= g->w || yy >= g->h) continue;
                        float wgt = ((k & 1) ? fx : 1 - fx) * ((k >> 1) ? fy : 1 - fy);
                        a += wgt * f->atlas[(g->y + yy) * f->atlas_w + g->x + xx];
                    }
                    if (a > 0.5f) blend(row + px2, c, (int)(a * (256.0f / 255.0f) + 0.5f));
                }
            }
        }
        x += g->adv * sc;
    }
    return w;
}
