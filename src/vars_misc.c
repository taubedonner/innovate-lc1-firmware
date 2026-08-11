/*
 * vars_misc.c: оставшиеся блоки ОЗУ:
 * Разнесение по отдельным файлам будет уточнено при восстановлении
 * соответствующих модулей.
 */
#include "lc1.h"

volatile int32_t g_var_0341;        /* @0x0341 */
volatile uint16_t g_vbat_min_raw;   /* @0x0345 */
volatile int32_t g_aout0_acc;       /* @0x0347 */
volatile int32_t g_t1_meas;         /* @0x034B */
uint8_t g_cfg_034F[8];              /* @0x034F - умолчание "LC-1" из flash 0x00DA */
volatile uint16_t g_var_035B;       /* @0x035B */
volatile uint16_t g_led_timer;      /* @0x035D */
volatile uint16_t g_rheat_min_mohm; /* @0x035F */
volatile uint16_t g_afr_multiplier; /* @0x0361 */
volatile float g_lean_offset;       /* @0x037B */
volatile uint16_t g_var_037F;       /* @0x037F */
volatile int32_t g_aout1_acc;       /* @0x0381 */
volatile uint16_t g_var_0386;       /* @0x0386 */
volatile uint8_t g_aout0_timer;     /* @0x03A6 */
volatile aout_cfg_t g_aout_cfg[2];  /* @0x038A и @0x0398, по 14 байт */
volatile int32_t g_acc_lambda;      /* @0x03A8 */
volatile uint16_t g_var_03AE;       /* @0x03AE */
volatile uint16_t g_timer1_ovf;     /* @0x03AC */
volatile int32_t g_t2_meas;         /* @0x03B6 */
