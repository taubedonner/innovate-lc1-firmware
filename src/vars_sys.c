/*
 * vars_sys.c: переменные блока ОЗУ 0x020A..0x025C.
 * Разбиение по файлам следует правилу "смежные адреса = один .c":
 * компоновщик кладёт .bss и .data каждого объектного файла подряд.
 */
#include "lc1.h"

volatile uint8_t g_is_chain_head;     /* @0x020A */
volatile uint8_t g_cmd_state;         /* @0x020D */
volatile uint8_t g_tick_flag_proto;   /* @0x020E */
volatile uint8_t g_state;             /* @0x020F */
volatile int16_t g_cal_err_integ;     /* @0x0210 */
volatile uint8_t g_var_0212;          /* @0x0212 */
volatile int32_t g_rheat_acc;         /* @0x0213 */
volatile int16_t g_pid_integ;         /* @0x0219 */
volatile int16_t g_pid_prev_err;      /* @0x021B */
volatile int16_t g_pid_out;           /* @0x021D */
volatile uint16_t g_rheat_cnt;        /* @0x021F */
volatile uint8_t g_var_0217;          /* @0x0217 */
volatile uint8_t g_var_0218;          /* @0x0218 */
volatile uint16_t g_var_0221;         /* @0x0221 */
volatile uint8_t g_var_0223;          /* @0x0223 */
volatile uint8_t g_air_cal_request;   /* @0x0224 */
volatile uint8_t g_var_0226;          /* @0x0226 */
volatile uint8_t g_ac_settled;        /* @0x0227 */
volatile uint8_t g_error_latched;     /* @0x0229 */
volatile uint16_t g_cnt_lambda;       /* @0x022A */
volatile uint16_t g_cnt_o2;           /* @0x022C */
volatile uint8_t g_var_022E;          /* @0x022E */
volatile uint16_t g_var_022F;         /* @0x022F */
volatile uint16_t g_var_0231;         /* @0x0231 */
volatile uint8_t g_adc_busy;          /* @0x0235 */
volatile uint8_t g_heater_cal_active; /* @0x0236 */
volatile uint8_t g_sensor_type;       /* @0x0237 */
volatile uint8_t g_tick_sub;          /* @0x0238 */
volatile uint8_t g_error_code;        /* @0x0239 */
volatile uint8_t g_retry_lo_r;        /* @0x0233 */
volatile uint8_t g_retry_misc;        /* @0x0234 */
volatile uint8_t g_spi_state;         /* @0x023A */
volatile uint16_t g_status;           /* @0x023E */
volatile uint8_t g_adc_data_ready;    /* @0x0240 */
volatile uint8_t g_var_0241;          /* @0x0241 */
volatile uint8_t g_ticks_since_ac;    /* @0x0242 */
volatile uint8_t g_var_0243;          /* @0x0243 */
volatile uint8_t g_var_0248;          /* @0x0248 */
volatile uint8_t g_cal_btn_count;     /* @0x0249 */
volatile uint8_t g_tick_flag_led;     /* @0x024A */
volatile uint8_t g_cal_btn_latched;   /* @0x024B */
volatile uint8_t g_nernst_ac_running; /* @0x024C */
volatile uint8_t g_test_mode;         /* @0x024D */
volatile uint16_t g_halfcycles_pos;   /* @0x024E */
volatile uint16_t g_halfcycles_neg;   /* @0x0250 */
volatile uint8_t g_var_0252;          /* @0x0252 */
volatile uint8_t g_led_prev_state;    /* @0x0253 */
volatile uint8_t g_led_prev_error;    /* @0x0254 */
volatile uint16_t g_err2_cycles;      /* @0x0255 */
volatile uint8_t g_behind_lm1;        /* @0x020B */
volatile uint8_t g_listen_role;       /* @0x020C */
