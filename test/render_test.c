/* Renderiza os paineis no Linux com dados falsos e salva PPM (para conferir o visual) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../android/panel.h"

static void save_ppm(const char *fn, surf_t *s) {
    FILE *f = fopen(fn, "wb");
    fprintf(f, "P6\n%d %d\n255\n", s->w, s->h);
    for (int y = 0; y < s->h; y++)
        for (int x = 0; x < s->w; x++) {
            uint32_t p = s->px[y * s->stride + x];
            unsigned char c[3] = { p & 255, (p >> 8) & 255, (p >> 16) & 255 };
            fwrite(c, 1, 3, f);
        }
    fclose(f);
}

int main(int argc, char **argv) {
    int W = argc > 1 ? atoi(argv[1]) : 1024, H = argc > 2 ? atoi(argv[2]) : 600;
    surf_t bg = { calloc(W * H, 4), W, H, W }, fb = { calloc(W * H, 4), W, H, W };
    dash_t d; memset(&d, 0, sizeof d);
    td_packet *p = &d.p;
    memcpy(p->magic, "TDSH", 4);
    strcpy(p->game_id, "eut2");
    p->flags = TDF_SDK_ACTIVE | TDF_ENGINE | TDF_ELECTRIC | TDF_LIGHT_LOW | TDF_BLINK_L | TDF_LIGHT_PARK | TDF_ON_JOB | TDF_FUEL_WARN;
    p->speed = 78 / 3.6f; p->rpm = 1350; p->rpm_limit = 2500; p->cruise = 80 / 3.6f; p->speed_limit = 80 / 3.6f;
    p->fuel = 180; p->fuel_capacity = 800; p->fuel_range = 620; p->adblue = 60; p->adblue_capacity = 80;
    p->air_pressure = 130; p->water_temp = 88; p->oil_temp = 95; p->oil_pressure = 60; p->battery_voltage = 27.4f;
    p->brake_temp = 60; p->odometer = 123456; p->displayed_gear = 11; p->gear = 11; p->retarder = 2;
    p->game_time = 2 * 1440 + 14 * 60 + 37; p->nav_distance = 345000; p->nav_time = 4.2f * 3600;
    strcpy(p->dest_city, "Munchen"); strcpy(p->cargo, "Maquinas agricolas");
    const char *brands[][3] = {
        { "scania", "Scania", "vehicle.scania.s_2016" }, { "scania", "Scania", "vehicle.scania.streamline" },
        { "volvo", "Volvo", "vehicle.volvo.fh16_2012" }, { "mercedes", "Mercedes-Benz", "vehicle.mercedes.actros" },
        { "man", "MAN", "vehicle.man.tgx" }, { "daf", "DAF", "vehicle.daf.xf" }, { "renault", "Renault", "vehicle.renault.t" },
        { "iveco", "Iveco", "vehicle.iveco.sway" }, { "kenworth", "Kenworth", "vehicle.kenworth.w900" }, { "xx", "Outro", "x" },
    };
    for (int i = 0; i < THEME_COUNT; i++) {
        strcpy(p->brand_id, brands[i][0]); strcpy(p->brand, brands[i][1]); strcpy(p->truck_id, brands[i][2]);
        strcpy(p->truck_name, "S"); strcpy(p->game_id, i == 8 ? "ats" : "eut2");
        d.connected = d.have = 1; d.manual = 0; d.t = 0;
        d.n_speed = d.n_rpm = 0;
        for (int k = 0; k < 120; k++) panel_update(&d, 0.05f);
        panel_draw_static(&bg, &d);
        r_blit(&fb, &bg);
        panel_draw_dynamic(&fb, &d);
        char fn[64]; snprintf(fn, sizeof fn, "/tmp/panel_%d.ppm", i);
        save_ppm(fn, &fb);
        printf("%d -> %s (%s)\n", i, fn, panel_theme_name(d.theme));
    }
    memset(&d, 0, sizeof d);
    strcpy(d.local_ip, "192.168.0.23"); d.t = 0.4f;
    panel_draw_static(&bg, &d); r_blit(&fb, &bg); panel_draw_dynamic(&fb, &d);
    save_ppm("/tmp/panel_wait.ppm", &fb);
    return 0;
}
