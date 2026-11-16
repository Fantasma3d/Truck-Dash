/* Painel por camadas PNG (ver docs/painel-png.md)
 *
 * Ideia: tudo que nao muda vira UMA imagem pronta (fundo + luzes apagadas),
 * copiada por quadro com memcpy. Por cima so entra o que esta "ligado":
 * PNG da luz acesa, pedaco revelado de uma barra/arco, ponteiro girado, numeros.
 */
#include "skin.h"
#include "plat_sys.h"

int uncompress(unsigned char *dst, unsigned long *dlen, const unsigned char *src, unsigned long slen);  /* zlib do sistema */

#define PI 3.14159265358979f
#define DEG (PI / 180.0f)

typedef struct { int w, h, ox, oy, fw, fh; uint32_t *px; } img_t;   /* original aparado; fw/fh = tamanho do arquivo */
typedef struct { int x, y, w, h; uint32_t *px; } spr_t;             /* ja na escala da tela */

enum { K_LUZ = 1, K_TEXTO, K_PONTEIRO, K_BARRA, K_ARCO, K_IMG };
#define MAX_TELAS 16
enum { DIR_R = 0, DIR_L, DIR_U, DIR_D };

/* dados numericos / de texto */
enum { D_NONE = 0, D_VEL, D_RPM, D_COMB, D_ARLA, D_AR, D_AGUA, D_OLEO, D_POLEO, D_BAT, D_CRUZ, D_LIM, D_ACEL, D_FREIO,
       D_EMBR, D_AUTON, D_ODO, D_CONS, D_RET, D_LITROS, D_NAV_KM, D_MARCHA, D_HORA, D_NAV_TEMPO, D_CAMINHAO, D_DESTINO, D_CARGA,
       D_VIAGEM_KM, D_VIAGEM_TEMPO, D_VIAGEM_VEL, D_CONS_HORA, D_ARLA_L, D_DESGASTE };
static const struct { const char *n; int id; } DATA[] = {
    { "velocidade", D_VEL }, { "rpm", D_RPM }, { "combustivel", D_COMB }, { "arla", D_ARLA }, { "ar", D_AR },
    { "agua", D_AGUA }, { "oleo", D_OLEO }, { "pressao_oleo", D_POLEO }, { "bateria", D_BAT }, { "cruzeiro", D_CRUZ },
    { "limite", D_LIM }, { "acelerador", D_ACEL }, { "freio", D_FREIO }, { "embreagem", D_EMBR }, { "autonomia", D_AUTON },
    { "hodometro", D_ODO }, { "consumo", D_CONS }, { "retarder", D_RET }, { "litros", D_LITROS }, { "nav_km", D_NAV_KM },
    { "marcha", D_MARCHA }, { "hora", D_HORA }, { "nav_tempo", D_NAV_TEMPO }, { "caminhao", D_CAMINHAO },
    { "destino", D_DESTINO }, { "carga", D_CARGA }, { "viagem_km", D_VIAGEM_KM }, { "viagem_tempo", D_VIAGEM_TEMPO },
    { "viagem_vel", D_VIAGEM_VEL }, { "consumo_hora", D_CONS_HORA }, { "arla_litros", D_ARLA_L }, { "desgaste", D_DESGASTE }, { 0, 0 } };

/* luzes */
enum { L_NONE = 0, L_SEMPRE, L_CRUZ, L_RET, L_LIM, L_AUX, L_TETO, L_FLAG = 100 };
static const struct { const char *n; int id; uint32_t flag; } LUZ[] = {
    { "seta_esq", L_FLAG, TDF_BLINK_L }, { "seta_dir", L_FLAG, TDF_BLINK_R }, { "alerta", L_FLAG, TDF_HAZARD },
    { "lanterna", L_FLAG, TDF_LIGHT_PARK }, { "farol_baixo", L_FLAG, TDF_LIGHT_LOW }, { "farol_alto", L_FLAG, TDF_LIGHT_HIGH },
    { "giroflex", L_FLAG, TDF_BEACON }, { "freio_mao", L_FLAG, TDF_PARKING_BRAKE }, { "freio_motor", L_FLAG, TDF_MOTOR_BRAKE },
    { "diferencial", L_FLAG, TDF_DIFF_LOCK }, { "eixo", L_FLAG, TDF_LIFT_AXLE }, { "eixo_carreta", L_FLAG, TDF_TRAILER_LIFT },
    { "luz_freio", L_FLAG, TDF_BRAKE_LIGHT }, { "re", L_FLAG, TDF_REVERSE }, { "limpador", L_FLAG, TDF_WIPERS },
    { "motor", L_FLAG, TDF_ENGINE }, { "aviso_combustivel", L_FLAG, TDF_FUEL_WARN }, { "aviso_arla", L_FLAG, TDF_ADBLUE_WARN },
    { "aviso_ar", L_FLAG, TDF_AIR_WARN }, { "emergencia_ar", L_FLAG, TDF_AIR_EMERG }, { "aviso_oleo", L_FLAG, TDF_OIL_WARN },
    { "aviso_agua", L_FLAG, TDF_WATER_WARN }, { "aviso_bateria", L_FLAG, TDF_BATT_WARN }, { "carga", L_FLAG, TDF_ON_JOB },
    { "carreta", L_FLAG, TDF_CARGO_LOADED }, { "pausado", L_FLAG, TDF_PAUSED },
    { "sempre", L_SEMPRE, 0 }, { "ignicao", L_SEMPRE, 0 }, { "cruzeiro", L_CRUZ, 0 }, { "retarder", L_RET, 0 },
    { "limite", L_LIM, 0 }, { "farol_aux", L_AUX, 0 }, { "farol_teto", L_TETO, 0 }, { 0, 0, 0 } };

typedef struct {
    int kind, data, invert, line;
    uint32_t flag, telas;   /* telas: bit 0 = tela 1 ... */
    img_t on, off;
    spr_t s_on, s_off;
    float x, y, cx, cy, pvx, pvy;
    int has_pivot;
    float v0, v1, a0, a1;
    int dir;
    float size; uint32_t color; int align, font, dec;
    char suffix[16], empty[16];
} item_t;

struct skin {
    int W, H;
    img_t bg;
    item_t *it; int n, cap;
    char err[160]; int nerr;
    char dir[256];
    surf_t base, cache;     /* base = fundo na escala da tela; cache = base + estaticos da tela atual */
    int cache_page, ntelas;
    uint32_t cur_mask;      /* telas das proximas linhas */
    float s, ox, oy;
};

/* ------------------------------------------------------------------ util */
static int ifloor(float v) { int i = (int)v; return (v < (float)i) ? i - 1 : i; }
static int iceil(float v) { int i = (int)v; return (v > (float)i) ? i + 1 : i; }

static void fail(skin_t *k, int line, const char *a, const char *b) {
    if (!k->nerr++) snprintf(k->err, sizeof k->err, "painel.txt linha %d: %s %s", line, a, b ? b : "");
}

static unsigned char *read_file(const char *path, long *len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    long n = (long)lseek(fd, 0, 2), got = 0;
    lseek(fd, 0, 0);
    if (n <= 0 || n > 48L * 1024 * 1024) { close(fd); return 0; }
    unsigned char *b = (unsigned char *)malloc((size_t)n + 1);
    if (!b) { close(fd); return 0; }
    while (got < n) { long r = (long)read(fd, b + got, (size_t)(n - got)); if (r <= 0) break; got += r; }
    close(fd);
    if (got != n) { free(b); return 0; }
    b[n] = 0; *len = n;
    return b;
}

/* ------------------------------------------------------------------ PNG */
static uint32_t be32(const unsigned char *p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;
    return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

/* decodifica PNG de 8/16 bits (cinza, RGB, paleta, com ou sem alfa), sem entrelacamento.
 * f e alterado (os blocos IDAT sao juntados no inicio do proprio buffer). */
static const char *png_decode(unsigned char *f, long n, img_t *o) {
    static const unsigned char SIG[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
    unsigned char pal[768], trns[256];
    int npal = 0, ntrns = 0, w = 0, h = 0, depth = 0, ct = 0;
    unsigned long zlen = 0;
    long pos = 8;
    if (n < 33 || memcmp(f, SIG, 8)) return "nao e um PNG";
    while (pos + 12 <= n) {
        uint32_t len = be32(f + pos);
        const unsigned char *ty = f + pos + 4;
        unsigned char *dt = f + pos + 8;
        if (len > (uint32_t)(n - pos - 12)) break;
        if (!memcmp(ty, "IHDR", 4) && len >= 13) {
            w = (int)be32(dt); h = (int)be32(dt + 4); depth = dt[8]; ct = dt[9];
            if (dt[12]) return "PNG entrelacado (salve sem 'interlace')";
        } else if (!memcmp(ty, "PLTE", 4)) { npal = (int)(len / 3); if (npal > 256) npal = 256; memcpy(pal, dt, (size_t)npal * 3); }
        else if (!memcmp(ty, "tRNS", 4)) { ntrns = (int)(len > 256 ? 256 : len); memcpy(trns, dt, (size_t)ntrns); }
        else if (!memcmp(ty, "IDAT", 4)) { memmove(f + zlen, dt, len); zlen += len; }
        else if (!memcmp(ty, "IEND", 4)) break;
        pos += 12 + (long)len;
    }
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || (long)w * h > 6L * 1024 * 1024) return "imagem grande demais (max. 4096 px)";
    int ch = ct == 0 ? 1 : ct == 2 ? 3 : ct == 3 ? 1 : ct == 4 ? 2 : ct == 6 ? 4 : 0;
    if (!ch || (depth != 8 && depth != 16 && !((ct == 0 || ct == 3) && (depth == 1 || depth == 2 || depth == 4)))) return "tipo de PNG nao suportado";
    if (ct == 3 && depth == 16) return "tipo de PNG nao suportado";
    int bpp = ch * depth, rb = (w * bpp + 7) / 8, fb = bpp >= 8 ? bpp / 8 : 1;
    unsigned long rawlen = (unsigned long)h * (unsigned long)(rb + 1), got = rawlen;
    unsigned char *raw = (unsigned char *)malloc(rawlen);
    if (!raw) return "sem memoria";
    if (uncompress(raw, &got, f, zlen) != 0 || got != rawlen) { free(raw); return "PNG corrompido"; }
    for (int y = 0; y < h; y++) {
        unsigned char *cur = raw + (unsigned long)y * (rb + 1), *pr = y ? cur - rb : 0;   /* pr = dados da linha anterior */
        int ft = *cur++;
        for (int i = 0; i < rb; i++) {
            int a = i >= fb ? cur[i - fb] : 0, b = pr ? pr[i] : 0, c = (pr && i >= fb) ? pr[i - fb] : 0, v = cur[i];
            switch (ft) { case 1: v += a; break; case 2: v += b; break; case 3: v += (a + b) >> 1; break; case 4: v += paeth(a, b, c); break; }
            cur[i] = (unsigned char)v;
        }
    }
    uint32_t *px = (uint32_t *)malloc((size_t)w * h * 4);
    if (!px) { free(raw); return "sem memoria"; }
    int st = depth == 16 ? 2 : 1;
    for (int y = 0; y < h; y++) {
        const unsigned char *r = raw + (unsigned long)y * (rb + 1) + 1;
        uint32_t *d = px + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            unsigned R, G, B, A = 255;
            if (ct == 0 || ct == 3) {
                unsigned v;
                if (depth >= 8) v = r[x * st];
                else { int sh = 8 - depth - ((x * depth) & 7); v = (r[(x * depth) >> 3] >> sh) & ((1u << depth) - 1); }
                if (ct == 3) {
                    if ((int)v >= npal) v = 0;
                    R = pal[v * 3]; G = pal[v * 3 + 1]; B = pal[v * 3 + 2];
                    if ((int)v < ntrns) A = trns[v];
                } else {
                    if (ntrns >= 2 && depth <= 8 && v == trns[1]) A = 0;
                    if (depth < 8) v = v * 255 / ((1u << depth) - 1);
                    R = G = B = v;
                }
            } else if (ct == 4) { R = G = B = r[x * 2 * st]; A = r[x * 2 * st + st]; }
            else {
                const unsigned char *q = r + x * ch * st;
                R = q[0]; G = q[st]; B = q[2 * st];
                if (ct == 6) A = q[3 * st];
                else if (ntrns >= 6 && depth == 8 && R == trns[1] && G == trns[3] && B == trns[5]) A = 0;
            }
            if (A != 255) { R = (R * A + 127) / 255; G = (G * A + 127) / 255; B = (B * A + 127) / 255; }   /* alfa pre-multiplicado */
            d[x] = R | (G << 8) | (B << 16) | (A << 24);
        }
    }
    free(raw);
    o->px = px; o->w = o->fw = w; o->h = o->fh = h; o->ox = o->oy = 0;
    return 0;
}

/* corta as bordas transparentes: uma camada exportada no tamanho do painel inteiro custa so o pedaco usado */
static void img_trim(img_t *im) {
    int x0 = im->w, y0 = im->h, x1 = -1, y1 = -1;
    for (int y = 0; y < im->h; y++) {
        const uint32_t *r = im->px + (size_t)y * im->w;
        for (int x = 0; x < im->w; x++)
            if (r[x] >> 24) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; y1 = y; }
    }
    if (x1 < 0) { free(im->px); im->px = 0; im->w = im->h = 0; return; }
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if (w == im->w && h == im->h) return;
    uint32_t *px = (uint32_t *)malloc((size_t)w * h * 4);
    if (!px) return;
    for (int y = 0; y < h; y++) memcpy(px + (size_t)y * w, im->px + (size_t)(y + y0) * im->w + x0, (size_t)w * 4);
    free(im->px);
    im->px = px; im->ox = x0; im->oy = y0; im->w = w; im->h = h;
}

static int img_load(skin_t *k, int line, const char *name, img_t *o, int trim) {
    char path[400];
    long n;
    snprintf(path, sizeof path, "%s/%s", k->dir, name);
    unsigned char *f = read_file(path, &n);
    if (!f) { fail(k, line, "nao achei o arquivo", name); return 0; }
    const char *why = png_decode(f, n, o);
    free(f);
    if (why) { fail(k, line, name, why); return 0; }
    if (trim) img_trim(o);
    return 1;
}

/* ------------------------------------------------------------------ escala */
/* gera a imagem na escala da tela. Ponto (X,Y) do painel -> tela (ox + X*s, oy + Y*s). (px,py) desloca a imagem no painel. */
static void make_sprite(const img_t *im, float px, float py, float s, float ox, float oy, int cw, int ch, spr_t *sp) {
    memset(sp, 0, sizeof *sp);
    if (!im->px) return;
    float bx = ox + (px + im->ox) * s, by = oy + (py + im->oy) * s;   /* canto da imagem na tela */
    int x0 = ifloor(bx), y0 = ifloor(by), x1 = iceil(bx + im->w * s), y1 = iceil(by + im->h * s);
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > cw) x1 = cw; if (y1 > ch) y1 = ch;
    if (x1 <= x0 || y1 <= y0) return;
    int w = x1 - x0, h = y1 - y0;
    uint32_t *out = (uint32_t *)malloc((size_t)w * h * 4);
    if (!out) return;
    sp->x = x0; sp->y = y0; sp->w = w; sp->h = h; sp->px = out;
    float inv = 1.0f / s;
    if (s > 0.999f && s < 1.001f && bx == (float)ifloor(bx) && by == (float)ifloor(by)) {     /* 1:1 */
        int sx = x0 - (int)bx, sy = y0 - (int)by;
        for (int y = 0; y < h; y++) memcpy(out + (size_t)y * w, im->px + (size_t)(y + sy) * im->w + sx, (size_t)w * 4);
        return;
    }
    for (int y = 0; y < h; y++) {
        float fy0 = (y0 + y - by) * inv, fy1 = (y0 + y + 1 - by) * inv;
        for (int x = 0; x < w; x++) {
            float fx0 = (x0 + x - bx) * inv, fx1 = (x0 + x + 1 - bx) * inv;
            float r = 0, g = 0, b = 0, a = 0;
            if (s >= 1.0f) {                         /* ampliando: bilinear */
                float u = (fx0 + fx1) * 0.5f - 0.5f, v = (fy0 + fy1) * 0.5f - 0.5f;
                int iu = ifloor(u), iv = ifloor(v);
                float tu = u - iu, tv = v - iv;
                for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
                    int xx = iu + i, yy = iv + j;
                    if (xx < 0 || yy < 0 || xx >= im->w || yy >= im->h) continue;
                    float wgt = (i ? tu : 1 - tu) * (j ? tv : 1 - tv);
                    uint32_t p = im->px[(size_t)yy * im->w + xx];
                    r += (p & 255) * wgt; g += ((p >> 8) & 255) * wgt; b += ((p >> 16) & 255) * wgt; a += (p >> 24) * wgt;
                }
            } else {                                 /* reduzindo: media da area coberta */
                int ix0 = ifloor(fx0), ix1 = iceil(fx1), iy0 = ifloor(fy0), iy1 = iceil(fy1);
                for (int yy = iy0; yy < iy1; yy++) {
                    if (yy < 0 || yy >= im->h) continue;
                    float wy = (yy + 1 < fy1 ? yy + 1 : fy1) - (yy > fy0 ? yy : fy0);
                    for (int xx = ix0; xx < ix1; xx++) {
                        if (xx < 0 || xx >= im->w) continue;
                        float wgt = wy * ((xx + 1 < fx1 ? xx + 1 : fx1) - (xx > fx0 ? xx : fx0));
                        uint32_t p = im->px[(size_t)yy * im->w + xx];
                        r += (p & 255) * wgt; g += ((p >> 8) & 255) * wgt; b += ((p >> 16) & 255) * wgt; a += (p >> 24) * wgt;
                    }
                }
                float ar = s * s;                    /* 1 / area */
                r *= ar; g *= ar; b *= ar; a *= ar;
            }
            unsigned R = (unsigned)(r + 0.5f), G = (unsigned)(g + 0.5f), B = (unsigned)(b + 0.5f), A = (unsigned)(a + 0.5f);
            if (A > 255) A = 255; if (R > A) R = A; if (G > A) G = A; if (B > A) B = A;
            out[(size_t)y * w + x] = R | (G << 8) | (B << 16) | (A << 24);
        }
    }
}

static inline uint32_t over(uint32_t d, uint32_t s) {
    uint32_t a = s >> 24;
    if (a >= 255) return s;
    uint32_t ia = 255 - a; ia += ia >> 7;
    uint32_t rb = (((d & 0x00ff00ffu) * ia) >> 8) & 0x00ff00ffu, g = (((d & 0x0000ff00u) * ia) >> 8) & 0x0000ff00u;
    return ((s & 0x00ffffffu) + rb + g) | 0xff000000u;
}

/* copia com transparencia; (cx0..cy1) = recorte na tela */
static void blit(surf_t *fb, const spr_t *sp, int cx0, int cy0, int cx1, int cy1) {
    if (!sp->px) return;
    int x0 = sp->x > cx0 ? sp->x : cx0, y0 = sp->y > cy0 ? sp->y : cy0;
    int x1 = sp->x + sp->w < cx1 ? sp->x + sp->w : cx1, y1 = sp->y + sp->h < cy1 ? sp->y + sp->h : cy1;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > fb->w) x1 = fb->w; if (y1 > fb->h) y1 = fb->h;
    for (int y = y0; y < y1; y++) {
        uint32_t *d = fb->px + (size_t)y * fb->stride + x0;
        const uint32_t *s = sp->px + (size_t)(y - sp->y) * sp->w + (x0 - sp->x);
        for (int n = x1 - x0; n > 0; n--, d++, s++) if (*s >> 24) *d = over(*d, *s);
    }
}

/* revela o setor entre os angulos a0 e a1 (graus, 0 = para cima, sentido horario) em volta de (cx,cy) na tela */
static void blit_arc(surf_t *fb, const spr_t *sp, float cx, float cy, float a0, float a1) {
    if (!sp->px) return;
    if (a1 < a0) { float t = a0; a0 = a1; a1 = t; }
    float sw = a1 - a0;
    if (sw <= 0.01f) return;
    if (sw >= 359.9f) { blit(fb, sp, 0, 0, fb->w, fb->h); return; }
    float v0x = r_sin(a0 * DEG), v0y = -r_cos(a0 * DEG), v1x = r_sin(a1 * DEG), v1y = -r_cos(a1 * DEG);
    int big = sw > 180.0f;
    for (int y = 0; y < sp->h; y++) {
        int Y = sp->y + y;
        if (Y < 0 || Y >= fb->h) continue;
        float py = Y + 0.5f - cy;
        const uint32_t *s = sp->px + (size_t)y * sp->w;
        uint32_t *d = fb->px + (size_t)Y * fb->stride + sp->x;
        for (int x = 0; x < sp->w; x++) {
            uint32_t p = s[x];
            if (!(p >> 24) || sp->x + x < 0 || sp->x + x >= fb->w) continue;
            float px = sp->x + x + 0.5f - cx;
            float c0 = v0x * py - v0y * px + 0.5f, c1 = px * v1y - py * v1x + 0.5f;   /* distancia (px) ate cada borda */
            float k0 = c0 <= 0 ? 0 : (c0 >= 1 ? 1 : c0), k1 = c1 <= 0 ? 0 : (c1 >= 1 ? 1 : c1);
            float k = big ? (k0 > k1 ? k0 : k1) : (k0 < k1 ? k0 : k1);
            if (k <= 0) continue;
            if (k < 1) {
                uint32_t m = (uint32_t)(k * 256);
                p = ((((p & 0x00ff00ffu) * m) >> 8) & 0x00ff00ffu) | ((((p >> 8) & 0x00ff00ffu) * m) & 0xff00ff00u);
            }
            d[x] = over(d[x], p);
        }
    }
}

/* ponteiro: gira a imagem (ja na escala da tela) em volta do pivo. ang em graus, horario */
static void blit_rot(surf_t *fb, const spr_t *sp, float pvx, float pvy, float cx, float cy, float ang) {
    if (!sp->px) return;
    float co = r_cos(ang * DEG), si = r_sin(ang * DEG);
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (int i = 0; i < 4; i++) {
        float lx = sp->x + ((i & 1) ? sp->w : 0) - pvx, ly = sp->y + ((i & 2) ? sp->h : 0) - pvy;
        float X = cx + lx * co - ly * si, Y = cy + lx * si + ly * co;
        if (X < minx) minx = X; if (X > maxx) maxx = X; if (Y < miny) miny = Y; if (Y > maxy) maxy = Y;
    }
    int x0 = ifloor(minx) - 1, y0 = ifloor(miny) - 1, x1 = iceil(maxx) + 1, y1 = iceil(maxy) + 1;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > fb->w) x1 = fb->w; if (y1 > fb->h) y1 = fb->h;
    for (int y = y0; y < y1; y++) {
        uint32_t *d = fb->px + (size_t)y * fb->stride;
        float qy = y + 0.5f - cy;
        for (int x = x0; x < x1; x++) {
            float qx = x + 0.5f - cx;
            float u = pvx + qx * co + qy * si - sp->x - 0.5f, v = pvy - qx * si + qy * co - sp->y - 0.5f;
            if (u <= -1 || v <= -1 || u >= sp->w || v >= sp->h) continue;
            int iu = ifloor(u), iv = ifloor(v);
            uint32_t fu = (uint32_t)((u - iu) * 256), fv = (uint32_t)((v - iv) * 256);
            uint32_t rb = 0, ga = 0;
            for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
                int xx = iu + i, yy = iv + j;
                if (xx < 0 || yy < 0 || xx >= sp->w || yy >= sp->h) continue;
                uint32_t wgt = ((i ? fu : 256 - fu) * (j ? fv : 256 - fv)) >> 8;      /* 0..256 */
                uint32_t p = sp->px[(size_t)yy * sp->w + xx];
                rb += (p & 0x00ff00ffu) * wgt; ga += ((p >> 8) & 0x00ff00ffu) * wgt;
            }
            if (!(ga >> 24)) continue;
            d[x] = over(d[x], ((rb >> 8) & 0x00ff00ffu) | (ga & 0xff00ff00u));
        }
    }
}

/* ------------------------------------------------------------------ painel.txt */
static float num(const char *s) {
    float v = 0, sg = 1, f = 0.1f;
    if (*s == '-') { sg = -1; s++; } else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    if (*s == '.' || *s == ',') { s++; while (*s >= '0' && *s <= '9') { v += (*s++ - '0') * f; f *= 0.1f; } }
    return v * sg;
}

static uint32_t hexcolor(const char *s) {
    uint32_t v = 0;
    if (*s == '#') s++;
    for (int i = 0; i < 6 && *s; i++, s++) {
        int c = *s, d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : 0;
        v = (v << 4) | (uint32_t)d;
    }
    return HEX(v);
}

static const char *kv(char **tok, int n, const char *key) {
    size_t l = strlen(key);
    for (int i = 0; i < n; i++) if (!strncmp(tok[i], key, l) && tok[i][l] == '=') return tok[i] + l + 1;
    return 0;
}
static float kvf(char **tok, int n, const char *key, float def) { const char *v = kv(tok, n, key); return v ? num(v) : def; }

static item_t *add_item(skin_t *k) {
    if (k->n == k->cap) {
        int nc = k->cap ? k->cap * 2 : 32;
        item_t *ni = (item_t *)malloc(sizeof(item_t) * (size_t)nc);
        if (!ni) return 0;
        if (k->it) { memcpy(ni, k->it, sizeof(item_t) * (size_t)k->n); free(k->it); }
        k->it = ni; k->cap = nc;
    }
    item_t *it = &k->it[k->n++];
    memset(it, 0, sizeof *it);
    return it;
}

static void parse_line(skin_t *k, int line, char **tok, int n) {
    const char *ty = tok[0], *v;
    if (!strcmp(ty, "tamanho") && n >= 3) { k->W = (int)num(tok[1]); k->H = (int)num(tok[2]); return; }
    if (!strcmp(ty, "tela")) {
        uint32_t m = 0;
        for (int i = 1; i < n; i++) {
            if (!strcmp(tok[i], "todas")) { m = 0xffffffffu; continue; }
            int t = (int)num(tok[i]);
            if (t < 1 || t > MAX_TELAS) { fail(k, line, "tela invalida:", tok[i]); continue; }
            m |= 1u << (t - 1);
            if (t > k->ntelas) k->ntelas = t;
        }
        k->cur_mask = m ? m : 0xffffffffu;
        return;
    }
    if (!strcmp(ty, "imagem") && n >= 2) {
        item_t *it = add_item(k);
        if (!it) return;
        it->kind = K_IMG; it->line = line; it->telas = k->cur_mask;
        it->x = kvf(tok + 2, n - 2, "x", 0); it->y = kvf(tok + 2, n - 2, "y", 0);
        img_load(k, line, tok[1], &it->on, 1);
        return;
    }
    if (!strcmp(ty, "fundo") && n >= 2) {
        if (img_load(k, line, tok[1], &k->bg, 0) && !k->W) { k->W = k->bg.w; k->H = k->bg.h; }
        return;
    }
    int kind = !strcmp(ty, "luz") ? K_LUZ : !strcmp(ty, "texto") ? K_TEXTO : !strcmp(ty, "ponteiro") ? K_PONTEIRO :
               !strcmp(ty, "barra") ? K_BARRA : !strcmp(ty, "arco") ? K_ARCO : 0;
    if (!kind || n < 2) { fail(k, line, "nao entendi:", ty); return; }
    int data = 0; uint32_t flag = 0;
    if (kind == K_LUZ) { for (int i = 0; LUZ[i].n; i++) if (!strcmp(LUZ[i].n, tok[1])) { data = LUZ[i].id; flag = LUZ[i].flag; } }
    else for (int i = 0; DATA[i].n; i++) if (!strcmp(DATA[i].n, tok[1])) data = DATA[i].id;
    if (!data) { fail(k, line, "dado desconhecido:", tok[1]); return; }
    item_t *it = add_item(k);
    if (!it) return;
    it->kind = kind; it->data = data; it->flag = flag; it->line = line; it->telas = k->cur_mask;
    tok += 2; n -= 2;
    it->x = kvf(tok, n, "x", 0); it->y = kvf(tok, n, "y", 0);
    it->cx = kvf(tok, n, "cx", 0); it->cy = kvf(tok, n, "cy", 0);
    it->v0 = kvf(tok, n, "de", 0); it->v1 = kvf(tok, n, "ate", 100);
    it->a0 = kvf(tok, n, "ang_de", 0); it->a1 = kvf(tok, n, "ang_ate", 0);
    if ((v = kv(tok, n, "inverte"))) it->invert = v[0] == 's' || v[0] == '1';
    if (kind == K_TEXTO) {
        it->size = kvf(tok, n, "altura", 40);
        it->color = (v = kv(tok, n, "cor")) ? hexcolor(v) : HEX(0xffffff);
        it->align = (v = kv(tok, n, "alinha")) ? (v[0] == 'c' ? AL_CENTER : v[0] == 'd' ? AL_RIGHT : AL_LEFT) : AL_CENTER;
        it->font = (v = kv(tok, n, "fonte")) ? (v[0] == 'n' ? F_BOLD : v[0] == 'e' ? F_COND : F_SANS) : F_SANS;   /* normal? negrito / estreita */
        if (v && !strcmp(v, "normal")) it->font = F_SANS;
        it->dec = (int)kvf(tok, n, "casas", 0);
        if ((v = kv(tok, n, "sufixo"))) snprintf(it->suffix, sizeof it->suffix, "%s", v);
        if ((v = kv(tok, n, "vazio"))) snprintf(it->empty, sizeof it->empty, "%s", v);
        return;
    }
    const char *on = kv(tok, n, kind == K_LUZ ? "on" : "img");
    if (!on && kind == K_LUZ) on = kv(tok, n, "img");
    if (on) img_load(k, line, on, &it->on, 1);
    else if (kind != K_LUZ) fail(k, line, "faltou img=", "");
    if (kind == K_LUZ && (v = kv(tok, n, "off"))) img_load(k, line, v, &it->off, 1);
    if (kind == K_LUZ && !on && !kv(tok, n, "off")) fail(k, line, "faltou on= ou off=", "");
    if (kind == K_BARRA && (v = kv(tok, n, "sentido"))) it->dir = v[0] == 'e' ? DIR_L : v[0] == 'c' ? DIR_U : v[0] == 'b' ? DIR_D : DIR_R;
    if (kind == K_PONTEIRO) {
        if (kv(tok, n, "pivo_x") || kv(tok, n, "pivo_y")) { it->pvx = kvf(tok, n, "pivo_x", 0); it->pvy = kvf(tok, n, "pivo_y", 0); it->has_pivot = 1; }
    }
}

skin_t *skin_load(const char *dir) {
    skin_t *k = (skin_t *)calloc(1, sizeof *k);
    char path[400];
    long len;
    if (!k) return 0;
    snprintf(k->dir, sizeof k->dir, "%s", dir);
    k->cur_mask = 0xffffffffu;
    snprintf(path, sizeof path, "%s/painel.txt", dir);
    char *txt = (char *)read_file(path, &len);
    if (!txt) { k->nerr = 1; snprintf(k->err, sizeof k->err, "nao achei %s", path); return k; }
    int line = 0;
    char *p = txt;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;   /* BOM do Bloco de Notas */
    while (*p) {
        char *tok[24]; int n = 0;
        char *e = p;
        while (*e && *e != '\n') e++;
        char *next = *e ? e + 1 : e;
        *e = 0;
        line++;
        for (char *c = p; *c;) {
            while (*c == ' ' || *c == '\t' || *c == '\r') c++;
            if (!*c || *c == '#') break;
            if (n >= 24) break;
            tok[n++] = c;
            char *w = c;                       /* copia removendo aspas: sufixo=" km/h" */
            int q = 0;
            while (*c && (q || (*c != ' ' && *c != '\t' && *c != '\r'))) { if (*c == '"') q = !q; else *w++ = *c; c++; }
            if (*c) c++;
            *w = 0;
        }
        if (n) parse_line(k, line, tok, n);
        p = next;
    }
    free(txt);
    if (k->ntelas < 1) k->ntelas = 1;
    if (!k->W || !k->H) { if (!k->nerr++) snprintf(k->err, sizeof k->err, "painel.txt: faltou a linha 'fundo arquivo.png'"); }
    return k;
}

static void free_scaled(skin_t *k) {
    free(k->cache.px); k->cache.px = 0; k->cache.w = k->cache.h = 0;
    free(k->base.px); k->base.px = 0; k->base.w = k->base.h = 0;
    for (int i = 0; i < k->n; i++) { free(k->it[i].s_on.px); k->it[i].s_on.px = 0; free(k->it[i].s_off.px); k->it[i].s_off.px = 0; }
}

void skin_free(skin_t *k) {
    if (!k) return;
    free_scaled(k);
    for (int i = 0; i < k->n; i++) { free(k->it[i].on.px); free(k->it[i].off.px); }
    free(k->it); free(k->bg.px); free(k);
}

int skin_ok(const skin_t *k) { return k && k->W > 0 && k->H > 0; }
int skin_items(const skin_t *k) { return k ? k->n : 0; }
int skin_pages(const skin_t *k) { return k ? k->ntelas : 1; }
const char *skin_error(const skin_t *k) { return k ? k->err : "sem memoria"; }

/* escala tudo uma vez para a resolucao da tela */
static void prepare(skin_t *k, int w, int h) {
    free_scaled(k);
    k->base.px = (uint32_t *)malloc((size_t)w * h * 4);
    k->cache.px = (uint32_t *)malloc((size_t)w * h * 4);
    if (!k->base.px || !k->cache.px) { free_scaled(k); return; }
    k->base.w = k->cache.w = w; k->base.h = k->cache.h = h; k->base.stride = k->cache.stride = w;
    k->cache_page = -1;
    float s = (float)w / k->W;
    if ((float)h / k->H < s) s = (float)h / k->H;
    k->s = s; k->ox = ifloor((w - k->W * s) * 0.5f); k->oy = ifloor((h - k->H * s) * 0.5f);
    r_clear(&k->base, RGB(0, 0, 0));
    spr_t t;
    make_sprite(&k->bg, 0, 0, s, k->ox, k->oy, w, h, &t);
    blit(&k->base, &t, 0, 0, w, h); free(t.px);
    for (int i = 0; i < k->n; i++) {
        item_t *it = &k->it[i];
        if (it->kind == K_TEXTO) continue;
        if (it->kind == K_PONTEIRO) make_sprite(&it->on, 0, 0, s, 0, 0, 1 << 20, 1 << 20, &it->s_on);   /* so reduz; a posicao vem do pivo */
        else make_sprite(&it->on, it->x, it->y, s, k->ox, k->oy, w, h, &it->s_on);
        if (it->kind == K_LUZ && it->off.px) make_sprite(&it->off, it->x, it->y, s, k->ox, k->oy, w, h, &it->s_off);
    }
}

/* tela atual: base + imagens fixas + luzes apagadas. Feito so quando a tela muda. */
static void build_page(skin_t *k, int page) {
    int w = k->cache.w, h = k->cache.h;
    uint32_t bit = 1u << page;
    memcpy(k->cache.px, k->base.px, (size_t)w * h * 4);
    for (int i = 0; i < k->n; i++) {
        item_t *it = &k->it[i];
        if (!(it->telas & bit)) continue;
        if (it->kind == K_IMG) blit(&k->cache, &it->s_on, 0, 0, w, h);
        else if (it->kind == K_LUZ) blit(&k->cache, &it->s_off, 0, 0, w, h);
    }
    k->cache_page = page;
}

/* ------------------------------------------------------------------ valores */
static float value(const dash_t *d, int id, int *smooth) {
    const td_packet *p = &d->p;
    int ats = panel_is_ats(d);
    float kv_ = ats ? 2.2369363f : 3.6f, kd = ats ? 0.621371f : 1.0f;
    *smooth = 1;
    switch (id) {
    case D_VEL: return d->n_speed;
    case D_RPM: return d->n_rpm;
    case D_COMB: return d->n_fuel * 100.0f;
    case D_ARLA: return d->n_adblue * 100.0f;
    case D_AR: return d->n_air;
    case D_AGUA: return d->n_water;
    case D_OLEO: return d->n_oil;
    }
    *smooth = 0;
    switch (id) {
    case D_POLEO: return ats ? p->oil_pressure : p->oil_pressure * 0.0689476f;
    case D_BAT: return p->battery_voltage;
    case D_CRUZ: return r_fabs(p->cruise) * kv_;
    case D_LIM: return p->speed_limit * kv_;
    case D_ACEL: return p->throttle * 100.0f;
    case D_FREIO: return p->brake * 100.0f;
    case D_EMBR: return p->clutch * 100.0f;
    case D_AUTON: return p->fuel_range * kd;
    case D_ODO: return p->odometer * kd;
    case D_CONS: return p->fuel_avg * 100.0f;
    case D_RET: return (float)p->retarder;
    case D_LITROS: return p->fuel;
    case D_NAV_KM: return p->nav_distance * 0.001f * kd;
    case D_CARGA: return p->cargo_mass * 0.001f;
    case D_MARCHA: return (float)(p->displayed_gear ? p->displayed_gear : p->gear);
    case D_VIAGEM_KM: return d->trip_km * kd;
    case D_VIAGEM_TEMPO: return d->trip_min;
    case D_VIAGEM_VEL: return d->trip_min > 1 ? d->trip_km * kd / (d->trip_min / 60.0f) : 0;
    case D_CONS_HORA: return d->fuel_lh;
    case D_ARLA_L: return p->adblue;
    case D_DESGASTE: {
        float w = p->wear_engine;
        if (p->wear_transmission > w) w = p->wear_transmission;
        if (p->wear_cabin > w) w = p->wear_cabin;
        if (p->wear_chassis > w) w = p->wear_chassis;
        if (p->wear_wheels > w) w = p->wear_wheels;
        return w * 100.0f;
    }
    }
    return 0;
}

static int light_on(const dash_t *d, const item_t *it) {
    const td_packet *p = &d->p;
    int on;
    if (!(p->flags & TDF_ELECTRIC)) return 0;            /* chave desligada: painel apagado */
    switch (it->data) {
    case L_SEMPRE: return 1;
    case L_CRUZ: on = p->cruise > 0.1f; break;
    case L_RET: on = p->retarder > 0; break;
    case L_LIM: on = p->speed_limit > 0.5f; break;
    case L_AUX: on = p->aux_front > 0; break;
    case L_TETO: on = p->aux_roof > 0; break;
    default: on = (p->flags & it->flag) != 0;
    }
    return it->invert ? !on : on;
}

static int text_of(const dash_t *d, const item_t *it, char *b, int n) {
    const td_packet *p = &d->p;
    int sm;
    switch (it->data) {
    case D_MARCHA: {
        int g = p->displayed_gear ? p->displayed_gear : p->gear;
        if (g > 0) snprintf(b, n, "%d", g); else if (g < 0) snprintf(b, n, "R%d", -g); else snprintf(b, n, "N");
        break;
    }
    case D_HORA: snprintf(b, n, "%02d:%02d", (int)((p->game_time / 60) % 24), (int)(p->game_time % 60)); break;
    case D_NAV_TEMPO: { int m = (int)(p->nav_time / 60.0f); snprintf(b, n, "%d:%02d", m / 60, m % 60); break; }
    case D_VIAGEM_TEMPO: { int m = (int)d->trip_min; snprintf(b, n, "%02d:%02d", m / 60, m % 60); break; }
    case D_CAMINHAO: snprintf(b, n, "%s %s", p->brand, p->truck_name); break;
    case D_DESTINO: snprintf(b, n, "%s", p->dest_city); break;
    case D_CRUZ: if (p->cruise <= 0.1f) return 0; goto number;
    case D_LIM: if (p->speed_limit <= 0.5f) return 0; goto number;
    default: number: {
        float v = value(d, it->data, &sm);
        if (it->dec <= 0) snprintf(b, n, "%d", (int)(v + (v < 0 ? -0.5f : 0.5f)));
        else snprintf(b, n, "%.*f", it->dec > 3 ? 3 : it->dec, (double)v);
    }
    }
    if (it->suffix[0]) { int l = (int)strlen(b); snprintf(b + l, (size_t)(n - l), "%s", it->suffix); }
    return 1;
}

void skin_draw(surf_t *fb, skin_t *k, const dash_t *d, int page) {
    if (!skin_ok(k)) return;
    if (k->cache.w != fb->w || k->cache.h != fb->h) prepare(k, fb->w, fb->h);
    if (!k->cache.px) return;
    if (page < 0 || page >= k->ntelas) page = 0;
    if (page != k->cache_page) build_page(k, page);
    uint32_t bit = 1u << page;
    r_blit(fb, &k->cache);
    int on = (d->p.flags & TDF_ELECTRIC) != 0;
    float s = k->s;
    for (int i = 0; i < k->n; i++) {
        const item_t *it = &k->it[i];
        if (!(it->telas & bit) || it->kind == K_IMG) continue;
        if (it->kind == K_LUZ) { if (light_on(d, it)) blit(fb, &it->s_on, 0, 0, fb->w, fb->h); continue; }
        if (it->kind == K_TEXTO) {
            char b[96];
            const char *str = b;
            if (!on) continue;
            if (!text_of(d, it, b, sizeof b)) { if (!it->empty[0]) continue; str = it->empty; }
            r_text(fb, it->font, k->ox + it->x * s, k->oy + it->y * s, it->size * s, str, it->color, it->align);
            continue;
        }
        int sm;
        float v = value(d, it->data, &sm), f;
        if (!on && !sm) v = it->v0;
        f = (it->v1 != it->v0) ? (v - it->v0) / (it->v1 - it->v0) : 0;
        f = r_clamp(f, 0, 1);
        if (it->kind == K_BARRA) {
            const spr_t *sp = &it->s_on;
            int x0 = sp->x, y0 = sp->y, x1 = sp->x + sp->w, y1 = sp->y + sp->h;
            if (it->dir == DIR_R) x1 = x0 + (int)(sp->w * f + 0.5f);
            else if (it->dir == DIR_L) x0 = x1 - (int)(sp->w * f + 0.5f);
            else if (it->dir == DIR_U) y0 = y1 - (int)(sp->h * f + 0.5f);
            else y1 = y0 + (int)(sp->h * f + 0.5f);
            blit(fb, sp, x0, y0, x1, y1);
        } else if (it->kind == K_ARCO) {
            blit_arc(fb, &it->s_on, k->ox + it->cx * s, k->oy + it->cy * s, it->a0, it->a0 + (it->a1 - it->a0) * f);
        } else if (it->kind == K_PONTEIRO) {
            float pvx, pvy;
            if (it->has_pivot) { pvx = it->pvx; pvy = it->pvy; }
            else if (it->on.fw == k->W && it->on.fh == k->H) { pvx = it->cx; pvy = it->cy; }   /* camada do tamanho do painel */
            else { pvx = it->on.fw * 0.5f; pvy = (float)it->on.fh; }                           /* meio da base */
            blit_rot(fb, &it->s_on, pvx * s, pvy * s, k->ox + it->cx * s, k->oy + it->cy * s, it->a0 + (it->a1 - it->a0) * f);
        }
    }
}
