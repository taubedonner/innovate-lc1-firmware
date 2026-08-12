/*
 * heater_cal.c: калибровка нагревателя ("Heater cal").
 *
 * Смысл процедуры (полностью подтверждается внешним описанием прибора):
 *   1) регулятор стабилизирует температуру датчика по измеренному Rnernst,
 *      сравнивая его с целевым значением;
 *   2) как только невязка удерживается малой, измеряется фактическое Rpump
 *      для этой конкретной пары "датчик-плата";
 *   3) полученное Rpump сохраняется в EEPROM как targetRpump и дальше служит
 *      уставкой регулятора в рабочем режиме - Rpump меряется без возмущения
 *      основного измерения, в отличие от Rnernst.
 *
 * 80 для LSU 4.2 совпадает с паспортным Ri этого датчика. У LSU 4.9 паспорт
 * Bosch (Y 258 E00 015e, п.1.3) даёт Ri = 300 Ом, но с оговоркой "измерено
 * переменным током 1...4 кГц при токе не выше 250 мкА", а здесь замер
 * делается коротким импульсом постоянной нагрузки. Это разные измерения
 * одной величины, поэтому 200 против 300 само по себе не дефект.
 * Проверяется только на приборе, см. неопределённости.
 */
#include "lc1.h"

uint8_t heater_cal_step(void)
{
    uint16_t rnernst_target;
    uint8_t err;
    uint8_t countdown_low; /* r16: признак "обратный отсчёт < 2" */
    int16_t r_avg;

    if (g_heater_cal_active == 0) {
        g_heater_cal_active = 1;
        g_rnernst_acc       = 0;
        g_cal_sample_cnt    = 0;
        g_heat_target       = (g_sensor_type == 2) ? 200 : 80;
        g_cal_err_integ     = 0;
        g_vheat_ramp        = 0;
    }

    if (g_adc_data_ready == 0)
        return 0;

    rnernst_target = (g_sensor_type == 2) ? 200 : 80;

    g_adc_data_ready = 0;
    g_cal_sample_cnt++;

    g_rnernst_acc += cell_measure(CELL_MEAS_RNERNST);

    /*
     * ------- Индикация обратного отсчёта калибровки,
     * Пока g_vheat_ramp < 200 деление даёт 0 и блок пропускается.
     * Дальше в g_report_value выкладывается 9,8,7,... - это то, что прибор
     * показывает как обратный отсчёт; g_status = 5 - код состояния.
     * результат первого вызова используется только для сравнения с нулём.
     * Не объединяю: это изменило бы код.
     */
    if ((int16_t)(g_vheat_ramp / 200) != 0) {
        int16_t v      = 9 - (int16_t)(g_vheat_ramp / 200);
        g_status       = STATUS_HEATER_CAL;
        g_report_value = (uint16_t)v;
        countdown_low  = (v < 2) ? 1 : 0;
    } else {
        countdown_low = 0;
    }

    if (g_sensor_type == 2) {
        g_heat_target = 200;
    } else {
        g_heat_target = 80;
        if (g_sensor_type == 1)
            g_heat_target = 400;
    }

    /*
     * ------- Раз в 25 выборок: усреднение и шаг регулятора ------------
     * 25 выборок x 81.25 мс ~ 2 с - интервал усреднения, описанный
     */
    if (g_cal_sample_cnt == 25) {
        err = heater_resistance_step();
        if (err != ERR_NONE) {
            heater_duty_update(0, DUTY_BY_IHEAT);
            g_heater_cal_active = 0;
            return err;
        }

        /*
         * Среднее Rnernst. Делитель для LSU 4.2 - 5, для остальных - 25,
         * для того же типа тоже увеличена впятеро (400 вместо 80), так что
         * пара "делитель 5 + уставка 400" эквивалентна честному среднему
         * с уставкой 80.
         */
        if (g_sensor_type == 1)
            g_cell_r_meas = (int16_t)(g_rnernst_acc / 5);
        else
            g_cell_r_meas = (int16_t)(g_rnernst_acc / 25);

        g_var_0241 = 0;
        heater_pid_step();

        g_rnernst_acc    = 0;
        g_cal_sample_cnt = 0;

        r_avg = g_cell_r_meas;

        /* Интегратор невязки набирается только в конце обратного отсчёта - */
        if (countdown_low != 0)
            g_cal_err_integ += (int16_t)(r_avg - (int16_t)rnernst_target);
    }

    g_vheat_ramp++;

    if (g_vheat_ramp < 2000)
        return 0;

    {
        int16_t e = g_cal_err_integ;
        if (e < 0)
            e = (int16_t)(-e);

        if (e >= 4) {
            /* Не сошлось - сброс интегратора и счётчика, ещё круг. */
            g_cal_err_integ = 0;
            g_vheat_ramp    = 0;
            return 0;
        }
    }

    /* Сошлось: температура удерживается по Rnernst, значит текущее Rpump */
    {
        int16_t rpump = cell_measure(CELL_MEAS_RPUMP);

        g_heat_target = (uint16_t)rpump;
        g_cell_r_meas = rpump;

        eeprom_store_word(0x16, rpump);
        g_heater_cal_active = 0;
    }

    return 0;
}

/*
 * Упрощённая калибровка нагревателя для NTK: Rnernst не используется,
 * датчик просто прогревается при фиксированном Vheat = 2150 (~10.5 В)
 * в течение 400 циклов, после чего установившееся Rheat записывается
 * в EEPROM[10] как целевое.
 * Возврат - код ошибки от heater_resistance_step(), иначе 0.
 */
uint8_t ntk_heater_cal_step(void)
{
    uint8_t err;
    uint16_t n;

    if (g_var_0212 == 0) {
        g_var_0212   = 1;
        g_vheat_ramp = 0;
        heater_duty_update(2150, DUTY_BY_VBAT);
        return 0;
    }

    if (g_adc_data_ready == 0)
        return 0;

    heater_duty_update(2150, DUTY_BY_VBAT);

    err = heater_resistance_step();
    if (err != ERR_NONE) {
        g_var_0212 = 0;
        heater_duty_update(0, DUTY_BY_IHEAT);
        return err;
    }

    g_adc_data_ready = 0;
    n                = g_vheat_ramp;

    /* Обратный отсчёт 9...1 для индикации. Деление на 40 вызывается ДВАЖДЫ */
    if ((uint16_t)(n / 40) != 0) {
        g_status       = STATUS_HEATER_CAL;
        g_report_value = (uint16_t)(9 - (n / 40));
    }

    g_vheat_ramp = (uint16_t)(n + 1);

    if ((int16_t)(n + 1) < 400)
        return 0;

    g_var_0212       = 0;
    g_rheat_avg_mohm = g_rheat_mohm & 0xFFFF;
    eeprom_store_word(10, (int16_t)g_rheat_mohm);
    g_rheat_target_mohm = (int16_t)g_rheat_avg_mohm;

    return 0;
}
