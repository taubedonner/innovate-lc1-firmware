/*
 * led.c: индикация состояния светодиодом D7 на выводе CAL_PU_EN (PB4).
 *
 * Вызывается первой в главном цикле; работает по флагу g_tick_flag_led,
 * который взводится обработчиком Timer2 раз в 20.48 мс. Все интервалы
 * ниже - в этих "шагах" по 20.48 мс.
 */
#include "lc1.h"

void led_task(void)
{
    uint16_t t;

    if (g_tick_flag_led == 0)
        return;
    g_tick_flag_led = 0;

    if (g_error_code != 0) {
        /* ============ Есть ошибка: мигаем её кодом ==================== */

        if (g_led_prev_error != g_error_code) {
            /* Код ошибки сменился - начинаем серию заново */
            PORTB &= ~(1 << P_CAL_PU_EN);
            g_led_prev_error = g_error_code;
            g_led_blink_done = 0;
            g_led_timer      = 0;
        } else {
            g_led_timer--;
            if (g_led_timer != 0)
                return;
        }

        if ((int16_t)(int8_t)g_error_code != (int16_t)g_led_blink_done) {
            if (PORTB & (1 << P_CAL_PU_EN)) {
                /* горит -> пауза 4 шага (~82 мс), вспышка зачтена */
                g_led_timer = 4;
                PORTB &= ~(1 << P_CAL_PU_EN);
                g_led_blink_done++;
            } else {
                PORTB |= (1 << P_CAL_PU_EN);
                g_led_timer = 2;
            }
        } else {
            /* Серия отмигана - длинная пауза 12 шагов (~246 мс). */
            PORTB &= ~(1 << P_CAL_PU_EN);
            g_led_timer      = 12;
            g_led_blink_done = 0;
        }
        return;
    }

    /* ============ Ошибок нет: индикация фазы работы ==================== */

    if (g_led_prev_error != g_error_code) {
        g_led_prev_error = g_error_code;
        g_led_timer      = 1;
        g_led_blink_done = 0;
    }

    if (g_state != g_led_prev_state) {
        g_led_timer      = 0;
        g_led_prev_state = g_state;
    }

    if ((int8_t)g_state >= 6) {
        if ((int8_t)g_state >= 8) {
            if (g_state != 8)
                goto slow_blink;
            /* --- Рабочий режим (g_state == 8): ровное свечение ------- * */
            if (g_air_cal_request != 0)
                PORTB &= ~(1 << P_CAL_PU_EN);
            else
                PORTB |= (1 << P_CAL_PU_EN);
            return;
        }

        /* --- Фазы 6..7: быстрое мигание, период 4 шага (~82 мс) ------ * */
        t = g_led_timer;
        if (t >= 4) {
            g_led_timer = 0;
            t           = 0;
        }
        if (t < 2)
            PORTB |= (1 << P_CAL_PU_EN);
        else
            PORTB &= ~(1 << P_CAL_PU_EN);
        g_led_timer = t + 1;
        return;
    }

slow_blink:
    /*
     * --- Фазы < 6 (прогрев): медленное мигание, период 12 шагов ------ *
     * моменту всегда истинно (их уравняли выше) - след оптимизации
     */
    t = g_led_timer;
    if (g_state != g_led_prev_state || t >= 12) {
        g_led_timer = 0;
        t           = 0;
    }
    if (t < 6)
        PORTB |= (1 << P_CAL_PU_EN);
    else
        PORTB &= ~(1 << P_CAL_PU_EN);
    g_led_timer = t + 1;
}
