/*
 * tick_isr.c: системный тик на переполнении Timer2 (он же генератор ШИМ
 *              нагревателя) и планировщик фоновых измерений АЦП.
 *
 * Timer2: TCCR2 = 0x6C -> Fast PWM, неинвертирующий выход OC2 (PB7),
 * предделитель /64. При 16 МГц: частота ШИМ 16e6/64/256 = 976.5625 Гц,
 * период переполнения = 1.024 мс.
 * Каждые 20 переполнений (20 x 1.024 мс = 20.48 мс) выполняется "большой" тик.
 */
#include "lc1.h"

ISR(TIMER2_OVF_vect)
{
    /*
     * Планировщик запуска цепочки фоновых измерений АЦП.
     * Точный момент внутри окна 20.48 мс зависит от типа датчика.
     */
    if (g_sensor_type != 0) {
        if (g_tick_sub == 0 || g_tick_sub == 10) {
            if (g_adc_data_ready == 0 && g_adc_busy == 0)
                adc_start(ADMUX_VBAT_SENSE);
        }
    } else {
        if (g_tick_sub == 0 && g_heater_cal_active == 0) {
            if (g_adc_data_ready == 0 && g_adc_busy == 0)
                adc_start(ADMUX_VBAT_SENSE);
        } else if (g_heater_cal_active != 0 && g_adc_data_ready == 0) {
            if ((g_tick_sub & 1) == 0)
                g_adc_data_ready = 1;
        }
    }

    /* Счётчик тактов внутри окна 20.48 мс */
    g_tick_sub++;

    if (g_tick_sub == 20) {
        g_tick_flag_led   = 1;
        g_tick_flag_proto = 1;

        /*
         * Насыщающийся счётчик "тиков без переключения компаратора":
         * используется в main как детектор пропавших автоколебаний.
         */
        if (g_ticks_since_ac != 0xFF)
            g_ticks_since_ac++;

        g_uptime_20ms++;
        g_var_0386++;
        g_tick_sub = 0;

        if (g_aout0_timer != 0)
            g_aout0_timer--;
        if (g_aout1_timer != 0)
            g_aout1_timer--;
    }
}
