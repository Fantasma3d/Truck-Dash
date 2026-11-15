/* Simula o ETS2: carrega o plugin (.so no Linux), registra canais e gera dados. */
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include "../plugin/scs_min.h"

typedef struct { char name[64]; scs_value_type_t type; scs_telemetry_channel_callback_t cb; scs_context_t ctx; } chan_t;
static chan_t ch[128]; static int nch;
static scs_telemetry_event_callback_t ev_cb[8]; static scs_context_t ev_ctx[8];

static scs_result_t reg_event(const scs_event_t e, const scs_telemetry_event_callback_t cb, const scs_context_t c) { ev_cb[e] = cb; ev_ctx[e] = c; return 0; }
static scs_result_t unreg_event(const scs_event_t e) { ev_cb[e] = 0; return 0; }
static scs_result_t reg_chan(const scs_string_t n, const scs_u32_t i, const scs_value_type_t t, const scs_u32_t f, const scs_telemetry_channel_callback_t cb, const scs_context_t c) {
    (void)i; (void)f; snprintf(ch[nch].name, 64, "%s", n); ch[nch].type = t; ch[nch].cb = cb; ch[nch].ctx = c; nch++; return 0;
}
static scs_result_t unreg_chan(const scs_string_t n, const scs_u32_t i, const scs_value_type_t t) { (void)n; (void)i; (void)t; return 0; }
static void game_log(const scs_log_type_t t, const scs_string_t m) { printf("[jogo log %d] %s\n", t, m); }

static void setf(const char *n, float v) { for (int i = 0; i < nch; i++) if (!strcmp(ch[i].name, n)) { scs_value_t x = { SCS_VALUE_TYPE_float }; x.v.value_float = v; ch[i].cb(n, SCS_U32_NIL, &x, ch[i].ctx); } }
static void seti(const char *n, int v) { for (int i = 0; i < nch; i++) if (!strcmp(ch[i].name, n)) { scs_value_t x = { ch[i].type }; x.v.value_s32 = v; ch[i].cb(n, SCS_U32_NIL, &x, ch[i].ctx); } }
static void setb(const char *n, int v) { for (int i = 0; i < nch; i++) if (!strcmp(ch[i].name, n)) { scs_value_t x = { SCS_VALUE_TYPE_bool }; x.v.value_bool = v; ch[i].cb(n, SCS_U32_NIL, &x, ch[i].ctx); } }

static scs_named_value_t A_s(const char *n, const char *v) { scs_named_value_t a; memset(&a, 0, sizeof a); a.name = n; a.index = SCS_U32_NIL; a.value.type = SCS_VALUE_TYPE_string; a.value.v.value_string = v; return a; }
static scs_named_value_t A_f(const char *n, float v) { scs_named_value_t a; memset(&a, 0, sizeof a); a.name = n; a.index = SCS_U32_NIL; a.value.type = SCS_VALUE_TYPE_float; a.value.v.value_float = v; return a; }
static scs_named_value_t A_u(const char *n, unsigned v) { scs_named_value_t a; memset(&a, 0, sizeof a); a.name = n; a.index = SCS_U32_NIL; a.value.type = SCS_VALUE_TYPE_u32; a.value.v.value_u32 = v; return a; }

static void send_truck(const char *brand_id, const char *brand, const char *id, const char *name) {
    scs_named_value_t at[16]; int k = 0;
    at[k++] = A_s("brand_id", brand_id); at[k++] = A_s("brand", brand); at[k++] = A_s("id", id); at[k++] = A_s("name", name);
    at[k++] = A_f("fuel.capacity", 900); at[k++] = A_f("fuel.warning.factor", 0.15f); at[k++] = A_f("adblue.capacity", 80);
    at[k++] = A_f("brake.air.pressure.warning", 65); at[k++] = A_f("brake.air.pressure.emergency", 30);
    at[k++] = A_f("rpm.limit", 2500); at[k++] = A_u("gears.forward", 12); at[k++] = A_u("gears.reverse", 2); at[k++] = A_u("retarder.steps", 3);
    memset(&at[k], 0, sizeof at[k]);
    scs_telemetry_configuration_t cfg = { "truck", at };
    ev_cb[SCS_TELEMETRY_EVENT_configuration](SCS_TELEMETRY_EVENT_configuration, &cfg, 0);
}

int main(int argc, char **argv) {
    double secs = argc > 2 ? atof(argv[2]) : 8;
    void *h = dlopen(argv[1], RTLD_NOW);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 1; }
    scs_result_t (*init)(scs_u32_t, const void *) = dlsym(h, "scs_telemetry_init");
    void (*shutdown)(void) = dlsym(h, "scs_telemetry_shutdown");
    scs_telemetry_init_params_v100_t p; memset(&p, 0, sizeof p);
    p.common.game_name = "Euro Truck Simulator 2"; p.common.game_id = "eut2";
    p.common.game_version = SCS_MAKE_VERSION(1, 18); p.common.log = game_log;
    p.register_for_event = reg_event; p.unregister_from_event = unreg_event;
    p.register_for_channel = reg_chan; p.unregister_from_channel = unreg_chan;
    printf("init -> %d, canais registrados: %d\n", init(SCS_TELEMETRY_VERSION_1_01, &p), nch);
    send_truck("scania", "Scania", "vehicle.scania.s_2016", "S");
    ev_cb[SCS_TELEMETRY_EVENT_started](SCS_TELEMETRY_EVENT_started, 0, 0);
    setb("truck.electric.enabled", 1); setb("truck.engine.enabled", 1); setb("truck.light.beam.low", 1);
    setf("truck.fuel.amount", 420); setf("truck.adblue", 50); setf("truck.brake.air.pressure", 125);
    setf("truck.water.temperature", 86); setf("truck.oil.temperature", 92); setf("truck.oil.pressure", 62);
    setf("truck.battery.voltage", 27.6f); setf("truck.odometer", 54321); setf("truck.fuel.range", 1300);
    setf("truck.navigation.distance", 210000); setf("truck.navigation.time", 9000); setf("truck.navigation.speed.limit", 80 / 3.6f);
    seti("game.time", 1440 * 4 + 9 * 60 + 5);
    int frames = (int)(secs * 60);
    for (int f = 0; f < frames; f++) {
        double t = f / 60.0;
        float sp = (float)(40 + 40 * sin(t * 0.8));
        setf("truck.speed", sp / 3.6f);
        setf("truck.engine.rpm", (float)(1100 + 500 * sin(t * 2.1)));
        seti("truck.displayed.gear", 8 + (int)(sp / 20)); seti("truck.engine.gear", 8 + (int)(sp / 20));
        setb("truck.light.lblinker", ((int)(t * 2.5)) % 2);
        if (f == frames / 2) send_truck("volvo", "Volvo", "vehicle.volvo.fh_2024", "FH");
        ev_cb[SCS_TELEMETRY_EVENT_frame_end](SCS_TELEMETRY_EVENT_frame_end, 0, 0);
        usleep(16666);
    }
    shutdown();
    printf("jogo encerrado\n");
    return 0;
}
