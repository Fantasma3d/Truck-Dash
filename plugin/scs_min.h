/* Declaracoes minimas do SCS Telemetry SDK (ETS2/ATS), conferidas com os
 * headers oficiais do SDK (scssdk.h, scssdk_value.h, scssdk_telemetry_*.h). */
#ifndef SCS_MIN_H
#define SCS_MIN_H
#include <stdint.h>

typedef uint8_t  scs_u8_t;
typedef int32_t  scs_s32_t;
typedef uint32_t scs_u32_t;
typedef uint64_t scs_u64_t;
typedef float    scs_float_t;
typedef double   scs_double_t;
typedef const char *scs_string_t;
typedef scs_s32_t scs_result_t;
typedef scs_s32_t scs_log_type_t;
typedef scs_u32_t scs_value_type_t;
typedef scs_u32_t scs_event_t;
typedef void *scs_context_t;
typedef scs_u64_t scs_timestamp_t;

#if defined(_WIN32) && !defined(_WIN64)
#define SCSAPIFUNC __stdcall
#else
#define SCSAPIFUNC
#endif

#define SCS_MAKE_VERSION(major, minor) (((major) << 16) | (minor))
#define SCS_TELEMETRY_VERSION_1_00 SCS_MAKE_VERSION(1, 0)
#define SCS_TELEMETRY_VERSION_1_01 SCS_MAKE_VERSION(1, 1)

#define SCS_RESULT_ok           0
#define SCS_RESULT_unsupported  (-1)
#define SCS_RESULT_generic_error (-7)
#define SCS_LOG_TYPE_message 0
#define SCS_LOG_TYPE_warning 1
#define SCS_LOG_TYPE_error   2

#define SCS_VALUE_TYPE_INVALID    0
#define SCS_VALUE_TYPE_bool       1
#define SCS_VALUE_TYPE_s32        2
#define SCS_VALUE_TYPE_u32        3
#define SCS_VALUE_TYPE_u64        4
#define SCS_VALUE_TYPE_float      5
#define SCS_VALUE_TYPE_double     6
#define SCS_VALUE_TYPE_fvector    7
#define SCS_VALUE_TYPE_dvector    8
#define SCS_VALUE_TYPE_euler      9
#define SCS_VALUE_TYPE_fplacement 10
#define SCS_VALUE_TYPE_dplacement 11
#define SCS_VALUE_TYPE_string     12
#define SCS_VALUE_TYPE_s64        13

#define SCS_U32_NIL ((scs_u32_t)-1)

#define SCS_TELEMETRY_EVENT_frame_start   1
#define SCS_TELEMETRY_EVENT_frame_end     2
#define SCS_TELEMETRY_EVENT_paused        3
#define SCS_TELEMETRY_EVENT_started       4
#define SCS_TELEMETRY_EVENT_configuration 5
#define SCS_TELEMETRY_EVENT_gameplay      6

#define SCS_TELEMETRY_CHANNEL_FLAG_none       0
#define SCS_TELEMETRY_CHANNEL_FLAG_each_frame 1
#define SCS_TELEMETRY_CHANNEL_FLAG_no_value   2

typedef struct scs_value_t {
    scs_value_type_t type;
    scs_u32_t _padding;
    union {
        scs_u8_t     value_bool;
        scs_s32_t    value_s32;
        scs_u32_t    value_u32;
        scs_u64_t    value_u64;
        int64_t      value_s64;
        scs_float_t  value_float;
        scs_double_t value_double;
        scs_string_t value_string;
        unsigned char _raw[40];
    } v;
} scs_value_t;   /* 48 bytes */

typedef struct scs_named_value_t {
    scs_string_t name;
    scs_u32_t index;
#if UINTPTR_MAX > 0xffffffffu
    scs_u32_t _padding;
#endif
    scs_value_t value;
} scs_named_value_t;   /* 56 (x86) / 64 (x64) */

typedef struct scs_telemetry_configuration_t {
    scs_string_t id;
    const scs_named_value_t *attributes;
} scs_telemetry_configuration_t;

typedef void (SCSAPIFUNC *scs_log_t)(const scs_log_type_t type, const scs_string_t message);

typedef void (SCSAPIFUNC *scs_telemetry_event_callback_t)(const scs_event_t event, const void *const event_info, const scs_context_t context);
typedef scs_result_t (SCSAPIFUNC *scs_telemetry_register_for_event_t)(const scs_event_t event, const scs_telemetry_event_callback_t callback, const scs_context_t context);
typedef scs_result_t (SCSAPIFUNC *scs_telemetry_unregister_from_event_t)(const scs_event_t event);
typedef void (SCSAPIFUNC *scs_telemetry_channel_callback_t)(const scs_string_t name, const scs_u32_t index, const scs_value_t *const value, const scs_context_t context);
typedef scs_result_t (SCSAPIFUNC *scs_telemetry_register_for_channel_t)(const scs_string_t name, const scs_u32_t index, const scs_value_type_t type, const scs_u32_t flags, const scs_telemetry_channel_callback_t callback, const scs_context_t context);
typedef scs_result_t (SCSAPIFUNC *scs_telemetry_unregister_from_channel_t)(const scs_string_t name, const scs_u32_t index, const scs_value_type_t type);

typedef struct scs_sdk_init_params_v100_t {
    scs_string_t game_name;
    scs_string_t game_id;
    scs_u32_t game_version;
#if UINTPTR_MAX > 0xffffffffu
    scs_u32_t _padding;
#endif
    scs_log_t log;
} scs_sdk_init_params_v100_t;

typedef struct scs_telemetry_init_params_v100_t {
    scs_sdk_init_params_v100_t common;
    scs_telemetry_register_for_event_t register_for_event;
    scs_telemetry_unregister_from_event_t unregister_from_event;
    scs_telemetry_register_for_channel_t register_for_channel;
    scs_telemetry_unregister_from_channel_t unregister_from_channel;
} scs_telemetry_init_params_v100_t;   /* 32 (x86) / 64 (x64) */

_Static_assert(sizeof(scs_value_t) == 48, "scs_value_t");
_Static_assert(sizeof(scs_named_value_t) == (sizeof(void*) == 8 ? 64 : 56), "named");
_Static_assert(sizeof(scs_telemetry_init_params_v100_t) == (sizeof(void*) == 8 ? 64 : 32), "params");

#endif
