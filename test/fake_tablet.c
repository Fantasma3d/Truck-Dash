/* Roda o app do tablet no Linux (mesma rede/logica), salvando quadros em PPM */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../android/app.h"
static void save_ppm(const char *fn, surf_t *s) {
    FILE *f = fopen(fn, "wb");
    fprintf(f, "P6\n%d %d\n255\n", s->w, s->h);
    for (int i = 0; i < s->w * s->h; i++) { uint32_t p = s->px[i]; unsigned char c[3] = { p & 255, (p >> 8) & 255, (p >> 16) & 255 }; fwrite(c, 1, 3, f); }
    fclose(f);
}
int main(int argc, char **argv) {
    int W = getenv("TD_W") ? atoi(getenv("TD_W")) : 1024, H = getenv("TD_H") ? atoi(getenv("TD_H")) : 600, n = 0;
    double secs = argc > 1 ? atof(argv[1]) : 10;
    surf_t fb = { calloc(W * H, 4), W, H, W };
    app_init("/tmp", argc > 2 ? argv[2] : "/tmp");
    for (int i = 0; i < secs * 30; i++) {
        app_frame(&fb);
        if (i == 5 && getenv("TD_DEMO")) app_tap(1);   /* modo demonstracao */
        if (i == 120) app_tap(0);                      /* recarrega os PNGs */
        if (i % 60 == 59) { char fn[64]; snprintf(fn, sizeof fn, "/tmp/tab_%02d.ppm", n++); save_ppm(fn, &fb); }
        usleep(33333);
    }
    printf("quadros salvos: %d\n", n);
    return 0;
}
