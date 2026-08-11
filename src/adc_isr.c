/*
 * adc_isr.c: обработчик прерывания АЦП: конвейер фоновых измерений
 *             и автомат дифференциального замера Rpump.
 *
 * Цепочка фоновых измерений, запускаемая из тика Timer2:
 *      ADC1 (VBAT) --+- если ключ нагревателя открыт (PB7 = 1) -> ADC2 (IHEAT)
 *                    +- иначе                                  -> ADC5 (CPU_CAL)
 *      ADC2 (IHEAT) ------------------------------------------> ADC5 (CPU_CAL)
 *      ADC5 (CPU_CAL) - опрос кнопки калибровки, цепочка заканчивается.
 *
 * Отдельно: ADC0 (VPUMP_SENSE) - двухфазный замер Rpump, инициируется
 * прерыванием компаратора (см. nernst_ac.c).
 */
#include "lc1.h"

ISR(ADC_vect)
{
    g_adc_busy = 0;

    /* ---------------- ADC1: напряжение бортовой сети ------------------- */
    if (ADMUX == ADMUX_VBAT_SENSE) {
        g_vbat_raw = ADC;

        /*
         * - проверка знакового бита, т.е. состояния PB7 (HEATER_PWM).
         * Ток нагревателя имеет смысл мерить только при открытом ключе.
         */
        if (PINB & (1 << P_HEATER_PWM)) {
            adc_start(ADMUX_IHEAT_SENSE);
        } else {
            g_adc_data_ready = 1;
            adc_start(ADMUX_CPU_CAL);
        }
        return;
    }

    /* ---------------- ADC2: ток нагревателя ---------------------------- */
    if (ADMUX == ADMUX_IHEAT_SENSE) {
        if (PINB & (1 << P_HEATER_PWM))
            g_iheat_raw = ADC;

        g_adc_data_ready = 1;
        adc_start(ADMUX_CPU_CAL);
        return;
    }

    /* ---------------- ADC5: кнопка калибровки -------------------------- */
    if (ADMUX == ADMUX_CPU_CAL) {
        /* Опрос имеет смысл только в рабочей фазе и без активной ошибки. */
        /* -> продолжаем только при g_state == 8 И отсутствии ошибки. */
        if (g_state == 8 && g_error_code == 0) {
            /* включённой подтяжке CAL_PU_EN (иначе просто выходим). */
            if (PORTB & (1 << P_CAL_PU_EN)) {
                if (ADC < 0x00A4) {
                    if (g_cal_btn_count < 100)
                        g_cal_btn_count++;
                } else {
                    g_cal_btn_count = 0;
                }
            }
        } else {
            g_cal_btn_count = 0;
        }
    }

    /* ---------------- ADC0: двухфазный замер Rpump --------------------- */
    if (ADMUX != ADMUX_VPUMP_SENSE)
        return;

    if (g_rpump_phase == 1) {
        /*
         * Фаза 1 (
         * перевернуть знак и запустить второе преобразование.
         */
        g_cell_r_meas = ADC;

        PORTA &= 0xF5;
        delay_loop_10cyc(5);
        g_rpump_phase++;
        adc_start(ADMUX_VPUMP_SENSE);

        /* Импульс "центрирования" AC-связи параллельно с преобразованием */
        PORTA |= (1 << P_AC_CENT_ENABLE);
        delay_loop_10cyc(2);
        PORTA &= ~(1 << P_AC_CENT_ENABLE);

    } else if (g_rpump_phase == 2) {
        /*
         * Фаза 2 (
         * Rpump (Upump(+) - Upump(-) в отсчётах АЦП); компаратор возвращается
         * в работу с прерыванием по спаду.
         */
        g_cell_r_meas = (int16_t)(g_cell_r_meas - (int16_t)ADC);

        ACSR = 0x12;
        ACSR = 0x1A;
        g_rpump_phase++;
    }

    TCNT1            = 0;
    g_timer1_ovf     = 0;
    g_ticks_since_ac = 0;
    g_var_0243       = 0x28; /* 40 */
}
