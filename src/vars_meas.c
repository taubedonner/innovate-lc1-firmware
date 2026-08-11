/* vars_meas.c: переменные блока ОЗУ 0x02EF..0x033E и 0x03B3. */
#include "lc1.h"

volatile int32_t g_rheat_mohm;        /* @0x02EF */
volatile int16_t g_vbat_scaled;       /* @0x02F3 */
volatile int16_t g_var_02F5;          /* @0x02F5 */
volatile uint16_t g_vheat_ramp;       /* @0x02FD */
volatile uint16_t g_heat_target;      /* @0x02FB */
volatile uint16_t g_cal_sample_cnt;   /* @0x0301 */
volatile int16_t g_var_0303;          /* @0x0303 */
volatile int16_t g_nominal_veff;      /* @0x0305 */
volatile int32_t g_rheat_avg_mohm;    /* @0x0307 */
volatile int16_t g_rnernst_acc;       /* @0x030B */
volatile uint16_t g_heat_duty;        /* @0x02F7 */
volatile int16_t g_rheat_target_mohm; /* @0x02F9 */
volatile int16_t g_rheat_cold_mohm;   /* @0x02FF */
volatile uint8_t g_var_0311;          /* @0x0311 */
volatile uint32_t g_uptime_20ms;      /* @0x030D */
volatile uint8_t g_dac_lsb;           /* @0x0312 */
volatile uint16_t g_led_blink_done;   /* @0x0313 */
volatile int16_t g_cell_r_meas;       /* @0x0315 */
volatile uint16_t g_aout0_cnt;        /* @0x0318 */
volatile uint8_t g_var_0317;          /* @0x0317 */
volatile float g_rich_slope;          /* @0x031A */
volatile uint16_t g_vbat_raw;         /* @0x031E */
volatile uint32_t g_t_neg;            /* @0x0320 */
volatile float g_dc_air;              /* @0x0324 */
volatile int32_t g_acc_o2;            /* @0x0328 */
volatile uint16_t g_aout1_cnt;        /* @0x032C */
volatile uint32_t g_t_pos;            /* @0x032E */
volatile uint16_t g_report_value;     /* @0x0334 */
volatile uint16_t g_iheat_raw;        /* @0x0337 */
volatile float g_lambda;              /* @0x0339 */
volatile uint8_t g_t_pos_valid;       /* @0x033D */
volatile uint8_t g_aout1_timer;       /* @0x033E */
volatile uint8_t g_rpump_phase;       /* @0x03B3 */
