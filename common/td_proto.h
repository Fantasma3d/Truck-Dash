/* TruckDash - protocolo PC <-> tablet (UDP, little-endian)
 *
 *  PC (plugin no ETS2/ATS)                    Tablet (app Android)
 *  -----------------------                    --------------------
 *  escuta UDP 45454  <---- HELLO (broadcast e unicast, 1x/s) ----
 *  ---- DATA (25x/s) ---> broadcast :45455 e unicast p/ quem mandou HELLO
 *
 *  Nenhuma configuracao e necessaria: o tablet descobre o PC pelo broadcast
 *  e o PC descobre o tablet pelo HELLO.
 */
#ifndef TD_PROTO_H
#define TD_PROTO_H

#include <stdint.h>

#define TD_PORT_PC      45454   /* plugin escuta aqui */
#define TD_PORT_TABLET  45455   /* tablet escuta aqui */
#define TD_PORT_USB     45456   /* TCP no tablet, via "adb forward tcp:45456 tcp:45456" */
#define TD_VERSION      1

/* flags (bitmask) */
#define TDF_PAUSED          (1u<<0)
#define TDF_ENGINE          (1u<<1)
#define TDF_ELECTRIC        (1u<<2)
#define TDF_PARKING_BRAKE   (1u<<3)
#define TDF_MOTOR_BRAKE     (1u<<4)
#define TDF_AIR_WARN        (1u<<5)
#define TDF_AIR_EMERG       (1u<<6)
#define TDF_FUEL_WARN       (1u<<7)
#define TDF_ADBLUE_WARN     (1u<<8)
#define TDF_OIL_WARN        (1u<<9)
#define TDF_WATER_WARN      (1u<<10)
#define TDF_BATT_WARN       (1u<<11)
#define TDF_BLINK_L         (1u<<12)  /* lampada esquerda acesa (pisca) */
#define TDF_BLINK_R         (1u<<13)
#define TDF_HAZARD          (1u<<14)
#define TDF_LIGHT_PARK      (1u<<15)
#define TDF_LIGHT_LOW       (1u<<16)
#define TDF_LIGHT_HIGH      (1u<<17)
#define TDF_BEACON          (1u<<18)
#define TDF_BRAKE_LIGHT     (1u<<19)
#define TDF_REVERSE         (1u<<20)
#define TDF_WIPERS          (1u<<21)
#define TDF_DIFF_LOCK       (1u<<22)
#define TDF_LIFT_AXLE       (1u<<23)
#define TDF_TRAILER_LIFT    (1u<<24)
#define TDF_CARGO_LOADED    (1u<<25)
#define TDF_ON_JOB          (1u<<26)
#define TDF_SDK_ACTIVE      (1u<<27)  /* jogo rodando e enviando dados */
/* ADAS: o SDK documentado nao tem estes canais; o plugin procura nomes nao documentados */
#define TDF_BLIND_L         (1u<<28)  /* veiculo no ponto cego esquerdo */
#define TDF_BLIND_R         (1u<<29)
#define TDF_LANE_L          (1u<<30)  /* saindo da faixa pela esquerda */
#define TDF_LANE_R          (1u<<31)

#pragma pack(push, 1)
typedef struct td_hello {
    char     magic[4];      /* "TDHI" */
    uint16_t version;
    uint16_t reserved;
    uint32_t seq;
} td_hello;

typedef struct td_packet {
    char     magic[4];      /* "TDSH" */
    uint16_t version;
    uint16_t size;          /* sizeof(td_packet) */
    uint32_t seq;
    uint32_t config_seq;    /* muda quando troca caminhao/config */
    uint32_t flags;

    float speed;            /* m/s (negativo = re) */
    float rpm;
    float rpm_limit;
    float cruise;           /* m/s, 0 = desligado */
    float speed_limit;      /* m/s, 0 = sem limite */
    float fuel;             /* litros */
    float fuel_capacity;
    float fuel_warn_factor;
    float fuel_range;       /* km */
    float fuel_avg;         /* l/km */
    float adblue;
    float adblue_capacity;
    float air_pressure;     /* psi */
    float air_warn;         /* psi */
    float air_emerg;        /* psi */
    float oil_pressure;     /* psi */
    float oil_temp;         /* C */
    float water_temp;       /* C */
    float battery_voltage;  /* V */
    float brake_temp;       /* C */
    float odometer;         /* km */
    float nav_distance;     /* m */
    float nav_time;         /* s */
    float throttle, brake, clutch;  /* 0..1 */
    float backlight;        /* 0..1 */
    float wear_engine, wear_transmission, wear_cabin, wear_chassis, wear_wheels;
    float cargo_mass;       /* kg */

    int32_t  gear;          /* marcha real */
    int32_t  displayed_gear;
    int32_t  fwd_gears, rev_gears;
    uint32_t retarder, retarder_steps;
    uint32_t aux_front, aux_roof;
    uint32_t game_time;     /* minutos desde o inicio do jogo */
    int32_t  rest_stop;     /* minutos ate descanso */
    uint32_t delivery_time; /* minuto absoluto do prazo */

    char game_id[8];        /* "eut2" ou "ats" */
    char brand_id[24];      /* ex: "scania" */
    char brand[24];         /* ex: "Scania" */
    char truck_id[48];      /* ex: "vehicle.scania.s_2016" */
    char truck_name[32];    /* ex: "S" */
    char cargo[32];
    char dest_city[32];
} td_packet;
#pragma pack(pop)

#endif
