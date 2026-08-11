/*
 * air_cal.c: калибровка по свободному воздуху.
 *
 * Суть: на свободном воздухе накапливаются суммы T1 и T2, усредняются и
 * сохраняются в EEPROM. Из них считается DCair, относительно которого
 * дальше вычисляется лямбда (см. lambda_calc.c).
 */
#include "lc1.h"

/* Возврат: 0 - калибровка есть и DCair посчитан, 1 - калибровки нет. */
uint8_t load_air_cal(void)
{
    uint16_t t1 = eeprom_init_and_read_word(14);
    uint16_t t2 = eeprom_init_and_read_word(16);

    g_var_0311 = 0;
    g_var_0223 = 0;

    if (t1 == 0 || t2 == 0) {
        g_var_0241 = 0;
        return 1;
    }

    g_dc_air   = duty_cycle((int32_t)t1, (int32_t)t2);
    g_var_0311 = 1;
    return 0;
}

/*
 * Вызывается из главного цикла, накапливает по одной паре полуволн за раз.
 * Условие завершения - И набрано не меньше 11 пар, И прошло не меньше
 * 25 системных тиков по 20.48 мс (~0.5 с).
 */
void air_cal_step(void)
{
    uint16_t n;

    if (g_state != 8) {
        g_air_cal_request = 0;
        return;
    }

    g_status = 2;

    if (g_var_0241 == 0) {
        g_t1_meas  = 0;
        g_t2_meas  = 0;
        g_var_0241 = 1;
        g_var_0386 = 0;
        g_var_03AE = 0x03FF;
        g_var_0221 = 0;
    }

    if (g_var_022E != 0)
        g_var_022E = 0;

    /* Новая пара времён появляется только когда ISR компаратора защёлкнул */
    if (g_t_pos_valid == 0)
        return;

    g_t_pos_valid = 0;
    g_var_0241    = 1;

    g_t1_meas += (int32_t)g_t_pos;
    g_t2_meas += (int32_t)g_t_neg;

    n          = (uint16_t)(g_var_0221 + 1);
    g_var_0221 = n;

    if (n < 11)
        return;
    if (g_var_0386 < 25)
        return;

    /* Усреднение - деление БЕЗЗНАКОВОЕ (__udivmodsi4), хотя делитель */
    g_t1_meas = (int32_t)((uint32_t)g_t1_meas / n);
    g_t2_meas = (int32_t)((uint32_t)g_t2_meas / n);

    save_air_cal();

    g_t1_meas         = 0;
    g_t2_meas         = 0;
    g_var_0386        = 0;
    g_var_03AE        = 0x03FF;
    g_var_0221        = 0;
    g_cnt_lambda      = 0;
    g_cnt_o2          = 0;
    g_air_cal_request = 0;
    g_var_0241        = 0;

    load_air_cal();
}
