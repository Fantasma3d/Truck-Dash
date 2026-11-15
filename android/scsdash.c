/* Interpretador dos paineis do ETS2/ATS (.tdp). Mesmo algoritmo de layout de tools/tdpreview.py. */
#include "scsdash.h"
#include "plat_sys.h"

enum { OP_END, OP_COLOR, OP_FONT, OP_OFFSET, OP_RET, OP_ALIGN, OP_ALIGN_END, OP_IMG, OP_TEXT, OP_MARGIN, OP_MARGIN_END };
enum { K_RUNS = 1, K_VALUE = 2, K_BAR = 3, K_GAUGE = 4 };

typedef struct { int w, h; const uint8_t *px; } sd_img;
typedef struct { int code; float ax, ay, aw, ah, px, py, adv; } sd_glyph;
typedef struct { int aw, ah; float sc, pad, line; int ng; sd_glyph *g; const uint8_t *sdf; } sd_font;
typedef struct {
    int kind, page, vis[3], id, dynamic, late;
    float x, y, w, h;
    int gimg; float gref_x, gref_y, gsc, amin, amax, vmin, vmax;
    int vert; float bmin, bmax, bvmin, bvmax;
    const uint8_t *ops; int opslen;
} sd_elem;
struct sd_pack {
    uint8_t *buf;
    int W, H, pages;
    int nimg; sd_img *img;
    int nfont; sd_font *font;
    int nel; sd_elem *el;
};

/* ---------------- leitura ---------------- */
typedef struct { const uint8_t *p, *end; int bad; } rd_t;
static uint32_t rd_u(rd_t *r, int n) {
    uint32_t v = 0;
    if (r->p + n > r->end) { r->bad = 1; return 0; }
    for (int i = 0; i < n; i++) v |= (uint32_t)r->p[i] << (8 * i);
    r->p += n;
    return v;
}
static float rd_f(rd_t *r) { uint32_t v = rd_u(r, 4); float f; memcpy(&f, &v, 4); return f; }
static const uint8_t *rd_skip(rd_t *r, size_t n) {
    const uint8_t *q = r->p;
    if ((size_t)(r->end - r->p) < n) { r->bad = 1; return 0; }
    r->p += n;
    return q;
}

static int ops_dynamic(const uint8_t *o, int n) {
    for (int i = 0; i + 1 < n; i++) if (o[i] == '%' && (o[i + 1] == '0' || o[i + 1] == '1')) return 1;
    return 0;
}

sd_pack *sd_load(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    size_t cap = 1 << 20, len = 0;
    uint8_t *buf = (uint8_t *)malloc(cap);
    for (;;) {
        if (!buf) { close(fd); return 0; }
        if (len == cap) {
            uint8_t *nb = (uint8_t *)malloc(cap * 2);
            if (!nb) { free(buf); close(fd); return 0; }
            memcpy(nb, buf, len); free(buf); buf = nb; cap *= 2;
        }
        ssize_t r = read(fd, buf + len, cap - len);
        if (r <= 0) break;
        len += (size_t)r;
    }
    close(fd);
    sd_pack *p = (sd_pack *)calloc(1, sizeof *p);
    rd_t r = { buf, buf + len, 0 };
    if (!p || len < 16 || memcmp(buf, "TDP1", 4) != 0) { free(buf); free(p); return 0; }
    p->buf = buf;
    r.p += 4;
    p->W = (int)rd_u(&r, 2); p->H = (int)rd_u(&r, 2); rd_u(&r, 2); p->pages = (int)rd_u(&r, 1);
    p->nimg = (int)rd_u(&r, 2);
    p->img = (sd_img *)calloc((size_t)p->nimg + 1, sizeof(sd_img));
    for (int i = 0; i < p->nimg && !r.bad; i++) {
        p->img[i].w = (int)rd_u(&r, 2); p->img[i].h = (int)rd_u(&r, 2);
        p->img[i].px = rd_skip(&r, (size_t)p->img[i].w * p->img[i].h * 4);
    }
    p->nfont = (int)rd_u(&r, 1);
    p->font = (sd_font *)calloc((size_t)p->nfont + 1, sizeof(sd_font));
    for (int i = 0; i < p->nfont && !r.bad; i++) {
        sd_font *f = &p->font[i];
        f->aw = (int)rd_u(&r, 2); f->ah = (int)rd_u(&r, 2);
        f->sc = rd_f(&r); f->pad = rd_f(&r); f->line = rd_f(&r);
        f->ng = (int)rd_u(&r, 2);
        f->g = (sd_glyph *)calloc((size_t)f->ng + 1, sizeof(sd_glyph));
        for (int k = 0; k < f->ng && !r.bad; k++) {
            sd_glyph *g = &f->g[k];
            g->code = (int)rd_u(&r, 2);
            g->ax = rd_f(&r); g->ay = rd_f(&r); g->aw = rd_f(&r); g->ah = rd_f(&r);
            g->px = rd_f(&r); g->py = rd_f(&r); g->adv = rd_f(&r);
        }
        f->sdf = rd_skip(&r, (size_t)f->aw * f->ah);
    }
    p->nel = (int)rd_u(&r, 2);
    p->el = (sd_elem *)calloc((size_t)p->nel + 1, sizeof(sd_elem));
    for (int i = 0; i < p->nel && !r.bad; i++) {
        sd_elem *e = &p->el[i];
        e->kind = (int)rd_u(&r, 1); e->page = (int)rd_u(&r, 1);
        for (int k = 0; k < 3; k++) e->vis[k] = (int)rd_u(&r, 2);
        e->id = (int)rd_u(&r, 2);
        e->x = rd_f(&r); e->y = rd_f(&r); e->w = rd_f(&r); e->h = rd_f(&r);
        if (e->kind == K_GAUGE) {
            e->gimg = (int)rd_u(&r, 2);
            e->gref_x = rd_f(&r); e->gref_y = rd_f(&r); e->gsc = rd_f(&r);
            e->amin = rd_f(&r); e->amax = rd_f(&r); e->vmin = rd_f(&r); e->vmax = rd_f(&r);
        } else {
            if (e->kind == K_BAR) {
                e->vert = (int)rd_u(&r, 1);
                e->bmin = rd_f(&r); e->bmax = rd_f(&r); e->bvmin = rd_f(&r); e->bvmax = rd_f(&r);
            }
            e->opslen = (int)rd_u(&r, 2);
            e->ops = rd_skip(&r, (size_t)e->opslen);
        }
        e->dynamic = e->kind != K_RUNS || e->vis[0] != 0 || (e->ops && ops_dynamic(e->ops, e->opslen));
    }
    if (r.bad) { sd_free(p); return 0; }
    /* elemento fixo desenhado por cima de algo dinamico precisa ser redesenhado a cada quadro */
    for (int i = 0; i < p->nel; i++) {
        sd_elem *e = &p->el[i];
        if (e->dynamic) continue;
        for (int k = 0; k < i && !e->late; k++) {
            sd_elem *d = &p->el[k];
            if (!(d->dynamic || d->late) || !(d->page & e->page)) continue;
            float dx0 = d->x, dy0 = d->y, dx1 = d->x + d->w, dy1 = d->y + d->h;
            if (d->kind == K_GAUGE) { float r = 2048; dx0 -= r; dy0 -= r; dx1 += r; dy1 += r; }
            if (d->kind == K_BAR) { if (d->vert) dy0 -= d->bmax; else dx1 += d->bmax; }
            if (e->x < dx1 && e->x + e->w > dx0 && e->y < dy1 && e->y + e->h > dy0) e->late = 1;
        }
    }
    return p;
}

void sd_free(sd_pack *p) {
    if (!p) return;
    for (int i = 0; i < p->nfont; i++) free(p->font[i].g);
    free(p->font); free(p->img); free(p->el); free(p->buf); free(p);
}
int sd_width(const sd_pack *p) { return p->W; }
int sd_height(const sd_pack *p) { return p->H; }
int sd_pages(const sd_pack *p) { return p->pages; }

/* ---------------- desenho de baixo nivel ---------------- */
typedef struct { int x0, y0, x1, y1; } clip_t;

static inline void blend_px(uint32_t *d, int r, int g, int b, int a) {
    if (a <= 0) return;
    if (a >= 255) { *d = RGB(r, g, b); return; }
    uint32_t v = *d;
    int dr = v & 255, dg = (v >> 8) & 255, db = (v >> 16) & 255;
    dr += ((r - dr) * a) >> 8; dg += ((g - dg) * a) >> 8; db += ((b - db) * a) >> 8;
    *d = RGB(dr, dg, db);
}
static inline int ifloor(float v) { int i = (int)v; return v < i ? i - 1 : i; }

/* imagem RGBA com escala bilinear, cor multiplicada (ARGB) e recorte */
static void blit_img(surf_t *s, const sd_img *im, float dx, float dy, float dw, float dh, uint32_t argb, const clip_t *c) {
    if (!im->px || dw < 0.5f || dh < 0.5f) return;
    int x0 = ifloor(dx), y0 = ifloor(dy), x1 = ifloor(dx + dw + 0.999f), y1 = ifloor(dy + dh + 0.999f);
    if (x0 < c->x0) x0 = c->x0; if (y0 < c->y0) y0 = c->y0; if (x1 > c->x1) x1 = c->x1; if (y1 > c->y1) y1 = c->y1;
    int ta = (argb >> 24) & 255, tr = (argb >> 16) & 255, tg = (argb >> 8) & 255, tb = argb & 255;
    float sx = im->w / dw, sy = im->h / dh;
    for (int py = y0; py < y1; py++) {
        float v = (py + 0.5f - dy) * sy - 0.5f;
        int iv = ifloor(v); float fv = v - iv;
        int v0 = iv < 0 ? 0 : (iv >= im->h ? im->h - 1 : iv), v1 = iv + 1 >= im->h ? im->h - 1 : (iv + 1 < 0 ? 0 : iv + 1);
        int wv = (int)(fv * 256);
        const uint8_t *r0 = im->px + (size_t)v0 * im->w * 4, *r1 = im->px + (size_t)v1 * im->w * 4;
        uint32_t *row = s->px + py * s->stride;
        for (int px = x0; px < x1; px++) {
            float u = (px + 0.5f - dx) * sx - 0.5f;
            int iu = ifloor(u); float fu = u - iu;
            int u0 = iu < 0 ? 0 : (iu >= im->w ? im->w - 1 : iu), u1 = iu + 1 >= im->w ? im->w - 1 : (iu + 1 < 0 ? 0 : iu + 1);
            int wu = (int)(fu * 256);
            const uint8_t *a = r0 + u0 * 4, *b = r0 + u1 * 4, *cc = r1 + u0 * 4, *d = r1 + u1 * 4;
            int w00 = (256 - wu) * (256 - wv), w01 = wu * (256 - wv), w10 = (256 - wu) * wv, w11 = wu * wv;
            int al = (a[3] * w00 + b[3] * w01 + cc[3] * w10 + d[3] * w11) >> 16;
            if (al <= 0) continue;
            /* cor ponderada pelo alfa (evita franjas escuras) */
            int a00 = a[3] * w00 >> 8, a01 = b[3] * w01 >> 8, a10 = cc[3] * w10 >> 8, a11 = d[3] * w11 >> 8;
            int sum = a00 + a01 + a10 + a11; if (sum <= 0) continue;
            int rr = (a[0] * a00 + b[0] * a01 + cc[0] * a10 + d[0] * a11) / sum;
            int gg = (a[1] * a00 + b[1] * a01 + cc[1] * a10 + d[1] * a11) / sum;
            int bb = (a[2] * a00 + b[2] * a01 + cc[2] * a10 + d[2] * a11) / sum;
            blend_px(row + px, rr * tr / 255, gg * tg / 255, bb * tb / 255, al * ta / 255);
        }
    }
}

static void draw_glyph(surf_t *s, const sd_font *f, const sd_glyph *g, float x, float y, float w, float h, uint32_t argb, const clip_t *c) {
    float kx = w / g->aw, ky = h / g->ah;          /* px de tela por px do atlas */
    float gx = x - f->pad * kx, gy = y - f->pad * ky;
    float gw = (g->aw + 2 * f->pad) * kx, gh = (g->ah + 2 * f->pad) * ky;
    int x0 = ifloor(gx), y0 = ifloor(gy), x1 = ifloor(gx + gw) + 1, y1 = ifloor(gy + gh) + 1;
    if (x0 < c->x0) x0 = c->x0; if (y0 < c->y0) y0 = c->y0; if (x1 > c->x1) x1 = c->x1; if (y1 > c->y1) y1 = c->y1;
    int ta = (argb >> 24) & 255, tr = (argb >> 16) & 255, tg = (argb >> 8) & 255, tb = argb & 255;
    float k = (kx + ky) * 0.5f;
    float gain = (2 * f->pad) * k / 255.0f;
    float sx0 = g->ax - f->pad, sy0 = g->ay - f->pad;
    for (int py = y0; py < y1; py++) {
        float v = sy0 + (py + 0.5f - gy) / ky - 0.5f;
        int iv = ifloor(v); float fv = v - iv;
        if (iv < 0 || iv + 1 >= f->ah) continue;
        const uint8_t *r0 = f->sdf + (size_t)iv * f->aw, *r1 = r0 + f->aw;
        uint32_t *row = s->px + py * s->stride;
        for (int px = x0; px < x1; px++) {
            float u = sx0 + (px + 0.5f - gx) / kx - 0.5f;
            int iu = ifloor(u); float fu = u - iu;
            if (iu < 0 || iu + 1 >= f->aw) continue;
            float d = (r0[iu] * (1 - fu) + r0[iu + 1] * fu) * (1 - fv) + (r1[iu] * (1 - fu) + r1[iu + 1] * fu) * fv;
            float al = (d - 127.5f) * gain + 0.5f;
            if (al <= 0) continue;
            if (al > 1) al = 1;
            blend_px(row + px, tr, tg, tb, (int)(al * ta));
        }
    }
}

/* imagem girada em torno do pivo (rx, ry), que e colocado em (cx, cy) */
static void draw_rotated(surf_t *s, const sd_img *im, float rx, float ry, float cx, float cy, float sc, float deg, const clip_t *c) {
    float a = deg * 3.14159265f / 180.0f, ca = r_cos(a), sa = r_sin(a);
    float xs[4] = { -rx, im->w - rx, im->w - rx, -rx }, ys[4] = { -ry, -ry, im->h - ry, im->h - ry };
    float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
    for (int i = 0; i < 4; i++) {
        float X = cx + (xs[i] * ca - ys[i] * sa) * sc, Y = cy + (xs[i] * sa + ys[i] * ca) * sc;
        if (X < mnx) mnx = X; if (X > mxx) mxx = X; if (Y < mny) mny = Y; if (Y > mxy) mxy = Y;
    }
    int x0 = ifloor(mnx), y0 = ifloor(mny), x1 = ifloor(mxx) + 1, y1 = ifloor(mxy) + 1;
    if (x0 < c->x0) x0 = c->x0; if (y0 < c->y0) y0 = c->y0; if (x1 > c->x1) x1 = c->x1; if (y1 > c->y1) y1 = c->y1;
    float inv = 1.0f / sc;
    for (int py = y0; py < y1; py++) {
        uint32_t *row = s->px + py * s->stride;
        float dy = py + 0.5f - cy;
        for (int px = x0; px < x1; px++) {
            float dx = px + 0.5f - cx;
            /* rotacao inversa */
            float u = (dx * ca + dy * sa) * inv + rx - 0.5f, v = (-dx * sa + dy * ca) * inv + ry - 0.5f;
            int iu = ifloor(u), iv = ifloor(v);
            if (iu < -1 || iv < -1 || iu >= im->w || iv >= im->h) continue;
            float fu = u - iu, fv = v - iv;
            float acc[4] = { 0, 0, 0, 0 }, wa = 0;
            for (int k = 0; k < 4; k++) {
                int xx = iu + (k & 1), yy = iv + (k >> 1);
                if (xx < 0 || yy < 0 || xx >= im->w || yy >= im->h) continue;
                const uint8_t *q = im->px + ((size_t)yy * im->w + xx) * 4;
                float wgt = ((k & 1) ? fu : 1 - fu) * ((k >> 1) ? fv : 1 - fv) * q[3];
                acc[0] += q[0] * wgt; acc[1] += q[1] * wgt; acc[2] += q[2] * wgt; wa += wgt;
            }
            if (wa <= 0.5f) continue;
            blend_px(row + px, (int)(acc[0] / wa), (int)(acc[1] / wa), (int)(acc[2] / wa), (int)(wa / 255.0f));
        }
    }
}

/* ---------------- layout (igual ao tdpreview.py) ---------------- */
typedef struct { int type, idx, font; const sd_glyph *g; float x, y, w, h; uint32_t color; } item_t;
#define MAX_ITEMS 400

static const sd_glyph *find_glyph(const sd_font *f, int code) {
    int lo = 0, hi = f->ng - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (f->g[m].code == code) return &f->g[m];
        if (f->g[m].code < code) lo = m + 1; else hi = m - 1;
    }
    return 0;
}

static int next_cp(const unsigned char **ps, const unsigned char *end) {
    const unsigned char *p = *ps;
    int c = *p++;
    if (c >= 0xC0 && p < end) {
        if (c < 0xE0) c = ((c & 0x1F) << 6) | (*p++ & 0x3F);
        else if (c < 0xF0 && p + 1 < end) { c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F); p += 2; }
        else { while (p < end && (*p & 0xC0) == 0x80) p++; c = '?'; }
    }
    *ps = p;
    return c;
}

static int layout(const sd_pack *pk, const uint8_t *o, int n, float rx, float ry, float rw, float rh,
                  const char *v0, const char *v1, item_t *it) {
    float px = rx, py = ry;
    uint32_t color = 0xFFFFFFFFu;
    int font = 0, cnt = 0, sp = 0;
    float fsx = 1, fsy = 1;
    int st_start[8]; float st_h[8], st_v[8], st_ml[8], st_mr[8];
    float mg_l = 0, mg_r = 0, mst_l[8], mst_r[8]; int msp = 0;
    rd_t r = { o, o + n, 0 };
    while (!r.bad && r.p < r.end) {
        int k = (int)rd_u(&r, 1);
        if (k == OP_END) break;
        switch (k) {
        case OP_COLOR: color = rd_u(&r, 4); break;
        case OP_FONT: font = (int)rd_u(&r, 1); fsx = rd_f(&r); fsy = rd_f(&r); if (font >= pk->nfont) font = 0; break;
        case OP_OFFSET: px += rd_f(&r); py += rd_f(&r); break;
        case OP_RET: px = rx; py = ry; break;
        case OP_ALIGN: {
            int h = (int)rd_u(&r, 1), v = (int)rd_u(&r, 1);
            float ml = rd_f(&r), mr = rd_f(&r);
            if (sp < 8) { st_start[sp] = cnt; st_h[sp] = (float)h; st_v[sp] = (float)v; st_ml[sp] = ml; st_mr[sp] = mr; sp++; }
            break;
        }
        case OP_ALIGN_END:
            if (sp > 0) {
                sp--;
                int s0 = st_start[sp];
                if (cnt > s0) {
                    float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
                    for (int i = s0; i < cnt; i++) {
                        if (it[i].x < mnx) mnx = it[i].x; if (it[i].x + it[i].w > mxx) mxx = it[i].x + it[i].w;
                        if (it[i].y < mny) mny = it[i].y; if (it[i].y + it[i].h > mxy) mxy = it[i].y + it[i].h;
                    }
                    float dx = 0, dy = 0;
                    int h = (int)st_h[sp], v = (int)st_v[sp];
                    if (h == 1) dx = rx + (rw - (mxx - mnx)) / 2 - mnx;
                    else if (h == 2) dx = rx + rw - st_mr[sp] - mxx;
                    else if (st_ml[sp] != 0) dx = rx + st_ml[sp] - mnx;
                    if (v == 1) dy = ry + (rh - (mxy - mny)) / 2 - mny;
                    else if (v == 2) dy = ry + rh - mxy;
                    for (int i = s0; i < cnt; i++) { it[i].x += dx; it[i].y += dy; }
                    px += dx; py += dy;
                }
            }
            break;
        case OP_MARGIN:
            if (msp < 8) { mst_l[msp] = mg_l; mst_r[msp] = mg_r; msp++; }
            mg_l = rd_f(&r); mg_r = rd_f(&r);
            px += mg_l;
            break;
        case OP_MARGIN_END:
            if (msp > 0) { msp--; mg_l = mst_l[msp]; mg_r = mst_r[msp]; }
            break;
        case OP_IMG: {
            int idx = (int)rd_u(&r, 2);
            float w = rd_f(&r), h = rd_f(&r);
            if (w < 0) w = rw - mg_l - mg_r;
            if (h < 0) h = rh;
            if (cnt < MAX_ITEMS && idx < pk->nimg) {
                item_t *q = &it[cnt++];
                q->type = 0; q->idx = idx; q->x = px; q->y = py; q->w = w; q->h = h; q->color = color;
            }
            px += w;
            break;
        }
        case OP_TEXT: {
            int len = (int)rd_u(&r, 2);
            const unsigned char *t = rd_skip(&r, (size_t)len), *te = t + len;
            if (!t || pk->nfont == 0) break;
            const sd_font *f = &pk->font[font];
            /* substitui %0 e %1 pelos valores */
            const unsigned char *sub = 0, *sube = 0;
            while (t < te || sub) {
                int c;
                if (sub) {
                    if (sub >= sube) { sub = 0; continue; }
                    c = next_cp(&sub, sube);
                } else if (t[0] == '%' && t + 1 < te && (t[1] == '0' || t[1] == '1')) {
                    const char *val = t[1] == '0' ? v0 : v1;
                    t += 2;
                    if (val) { sub = (const unsigned char *)val; sube = sub + strlen(val); }
                    continue;
                } else c = next_cp(&t, te);
                const sd_glyph *g = find_glyph(f, c);
                if (!g) g = find_glyph(f, '?');
                if (!g) continue;
                if (g->aw > 0 && cnt < MAX_ITEMS) {
                    item_t *q = &it[cnt++];
                    q->type = 1; q->font = font; q->g = g;
                    q->x = px + g->px * f->sc * fsx; q->y = py + g->py * f->sc * fsy;
                    q->w = g->aw * f->sc * fsx; q->h = g->ah * f->sc * fsy; q->color = color;
                }
                px += g->adv * f->sc * fsx;
            }
            break;
        }
        default: r.bad = 1; break;
        }
    }
    return cnt;
}

static void draw_items(surf_t *s, const sd_pack *pk, const sd_ctx *c, const item_t *it, int n, const clip_t *cl) {
    for (int i = 0; i < n; i++) {
        float x = c->ox + it[i].x * c->scale, y = c->oy + it[i].y * c->scale;
        float w = it[i].w * c->scale, h = it[i].h * c->scale;
        if (it[i].type == 0) blit_img(s, &pk->img[it[i].idx], x, y, w, h, it[i].color, cl);
        else draw_glyph(s, &pk->font[it[i].font], it[i].g, x, y, w, h, it[i].color, cl);
    }
}

void sd_draw(surf_t *s, const sd_pack *pk, const sd_ctx *c, int pass) {
    static item_t items[MAX_ITEMS];
    clip_t full = { 0, 0, s->w, s->h };
    for (int i = 0; i < pk->nel; i++) {
        const sd_elem *e = &pk->el[i];
        if ((e->dynamic || e->late) != pass) continue;
        if (!((e->page >> (c->page - 1)) & 1)) continue;   /* mascara de paginas */
        int vis = 1;
        for (int k = 0; k < 3; k++) if (e->vis[k] && !c->visible(c->user, e->vis[k])) vis = 0;
        if (!vis) continue;
        if (e->kind == K_GAUGE) {
            float v;
            if (!c->num(c->user, e->id, &v)) v = e->vmin;
            float t = (e->vmax != e->vmin) ? (v - e->vmin) / (e->vmax - e->vmin) : 0;
            if (t < 0) t = 0; if (t > 1) t = 1;
            float ang = e->amin + (e->amax - e->amin) * t;
            draw_rotated(s, &pk->img[e->gimg], e->gref_x, e->gref_y,
                         c->ox + (e->x + e->w / 2) * c->scale, c->oy + (e->y + e->h / 2) * c->scale,
                         e->gsc * c->scale, ang, &full);
        } else if (e->kind == K_BAR) {
            float v;
            if (!c->num(c->user, e->id, &v)) continue;
            float t = (e->bvmax != e->bvmin) ? (v - e->bvmin) / (e->bvmax - e->bvmin) : 0;
            if (t < 0) t = 0; if (t > 1) t = 1;
            float size = e->bmin + (e->bmax - e->bmin) * t;
            float bx = e->x, by = e->y, bw = e->w, bh = e->h;
            if (e->vert) { by = e->y + e->h - size; bh = size; } else bw = size;
            if (size <= 0.01f) continue;
            int n = layout(pk, e->ops, e->opslen, bx, by, bw, bh, 0, 0, items);
            clip_t cl = { ifloor(c->ox + bx * c->scale), ifloor(c->oy + by * c->scale),
                          ifloor(c->ox + (bx + bw) * c->scale + 0.5f), ifloor(c->oy + (by + bh) * c->scale + 0.5f) };
            if (cl.x0 < 0) cl.x0 = 0; if (cl.y0 < 0) cl.y0 = 0; if (cl.x1 > s->w) cl.x1 = s->w; if (cl.y1 > s->h) cl.y1 = s->h;
            draw_items(s, pk, c, items, n, &cl);
        } else {
            const char *v0 = 0, *v1 = 0;
            char buf[64], *bar;
            if (e->kind == K_VALUE) {
                const char *t = c->text(c->user, e->id, buf, sizeof buf);
                if (!t) continue;
                if (t != buf) { snprintf(buf, sizeof buf, "%s", t); }
                v0 = buf;
                bar = buf;
                while (*bar && *bar != '|') bar++;
                if (*bar) { *bar = 0; v1 = bar + 1; } else v1 = "";
            }
            int n = layout(pk, e->ops, e->opslen, e->x, e->y, e->w, e->h, v0, v1, items);
            draw_items(s, pk, c, items, n, &full);
        }
    }
}
