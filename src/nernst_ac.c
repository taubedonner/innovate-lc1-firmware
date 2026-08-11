/*
 * nernst_ac.c: автоколебательный контур накачки ячейки и измерение T1/T2.
 *
 * Суть (подтверждено внешним описанием прибора):
 *   На ячейку насоса подаётся ток Ipump постоянной величины, знак которого
 *   переключается выводом DIR_CONTROL (PA1). Напряжение на ячейке Нернста
 *   линейно уходит в одну сторону; как только оно пересекает опору +2.95 В
 *   на входе AIN1, срабатывает аналоговый компаратор МК. В обработчике знак
 *   тока переворачивается, и процесс повторяется.
 *   Времена полуволн T1 (положительная) и T2 (отрицательная) измеряются
 *   Timer1 с шагом 0.5 мкс (CS=/8 при 16 МГц) и программным расширением до
 *   32 бит счётчиком переполнений.
 *   Далее вычисляется DC = (T1 - T2) / (T1 + T2), и из него - лямбда.
 *   Типичный период T1+T2 составляет 2...8 мс.
 */
#include "lc1.h"

/*
 * ISR(TIMER1_OVF_vect)
 *
 * Старшие 16 разрядов 32-битного счётчика времени полуволны.
 * Timer1 при CS=/8 переполняется каждые 65536 x 0.5 мкс = 32.768 мс.
 */
ISR(TIMER1_OVF_vect)
{
    g_timer1_ovf++;
}

/*
 * ISR(ANALOG_COMP_vect)
 *
 * Локальный фрейм (Y+1..Y+4) = 32-битная метка времени
 *   Y+1,Y+2 = TCNT1, Y+3,Y+4 = g_timer1_ovf.
 */
ISR(ANALOG_COMP_vect)
{
    uint8_t aco_at_entry;
    uint32_t timestamp;

    /* Состояние компаратора на входе в прерывание */
    aco_at_entry = ACSR & (1 << ACO);

    /* Метка времени снимается ПЕРВЫМ делом, до любых задержек - */
    timestamp = ((uint32_t)g_timer1_ovf << 16) | TCNT1;

    /* Программный антидребезг: пауза 3.125 мкс и повторная проверка ACO. */
    delay_loop_10cyc(5);

    if (ACSR & (1 << ACO)) {
        /* ================= Компаратор в "1" ============================= */

        /* Требуется, чтобы текущее направление тока было положительным. */
        if (!(PORTA & (1 << P_DIR_CONTROL)))
            return;

        if ((ACSR & (1 << ACO)) != aco_at_entry)
            return;

        /*
         * Врезка замера Rpump: если тип датчика задан и автомат Rpump ждёт
         * старта, запускаем преобразование VPUMP_SENSE по прерыванию и
         * ГЛУШИМ прерывание компаратора на время замера.
         */
        if (g_sensor_type != 0 && g_rpump_phase == 1 && g_adc_busy == 0) {
            adc_start(ADMUX_VPUMP_SENSE);
            ACSR &= 0x77;
            return;
        }

        /* --- Переворот тока в "минус" и перевод компаратора на спад --- */
        PORTA &= 0xF5;
        ACSR = 0x12;
        ACSR = 0x1A;
        delay_loop_10cyc(5);
        PORTA |= (1 << P_AC_CENT_ENABLE);
        delay_loop_10cyc(2);
        PORTA &= ~(1 << P_AC_CENT_ENABLE);

        g_halfcycles_pos++;

        /* Метка конца положительной полуволны T1 - только если предыдущая */
        if (g_t_pos_valid == 0)
            g_t_pos = timestamp;

    } else {
        /* ================= Компаратор в "0" ============================= */

        if (PORTA & (1 << P_DIR_CONTROL))
            return;

        if ((ACSR & (1 << ACO)) != aco_at_entry)
            return;

        /* --- Переворот тока в "плюс" и перевод компаратора на фронт --- */
        PORTA |= (1 << P_DIR_CONTROL) | (1 << P_VN_HYST);
        ACSR = 0x13;
        ACSR = 0x1B;
        delay_loop_10cyc(5);
        PORTA |= (1 << P_AC_CENT_ENABLE);
        delay_loop_10cyc(2);
        PORTA &= ~(1 << P_AC_CENT_ENABLE);

        g_halfcycles_neg++;

        /*
         * Метка конца отрицательной полуволны T2 + защёлка пары.
         * (g_sensor_type == 0 || g_rpump_phase == 0).
         */
        if (g_t_pos_valid == 0) {
            if (g_sensor_type == 0 || g_rpump_phase == 0) {
                g_t_neg       = timestamp;
                g_t_pos_valid = 1;
            }
        }

        /*
         * Сброс автомата Rpump после завершения его цикла
         * Условие: g_rpump_phase >= 4 И g_sensor_type != 0.
         */
        if (g_rpump_phase >= 4 && g_sensor_type != 0)
            g_rpump_phase = 0;
    }

    TCNT1            = 0;
    g_timer1_ovf     = 0;
    g_ticks_since_ac = 0;
    g_ac_settled     = 0;
    g_var_0243       = 0x28;
}

void nernst_ac_start(void)
{
    TCNT1  = 0;
    TCCR1A = 0;
    TCCR1B = 0x02;
    TIMSK |= (1 << TOIE1);

    g_t_pos_valid = 0;
    g_timer1_ovf  = 0;

    DDRA |= (1 << P_DIR_CONTROL) | (1 << P_VN_HYST);

    /*
     * Начальное направление тока - противофазно текущему состоянию
     * компаратора, чтобы контур сразу пошёл в автоколебания.
     */
    if (ACSR & (1 << ACO))
        PORTA &= 0xF5;
    else
        PORTA |= (1 << P_DIR_CONTROL) | (1 << P_VN_HYST);

    ACSR                = 0x18; /*
                                     *                   (прерывание по любому
                                     *                    изменению выхода)
                                     */
    g_nernst_ac_running = 1;
}

/*
 * Полный останов измерительного тракта: компаратор выключен (ACD=1),
 * управляющие линии в третьем состоянии, Timer1 остановлен, нагреватель снят.
 */
void meas_shutdown(void)
{
    ACSR = 0x80;
    PORTA &= 0xF5;
    DDRA &= 0xF5;
    TIMSK &= (uint8_t)~(1 << TOIE1);
    TCCR1B = 0;

    heater_pwm_set(0);
    g_nernst_ac_running = 0;
}

/*
 * Аварийный останов. Гасит измерения, ждёт ~100 мс, запрещает USART0 и
 * зависает в бесконечном цикле ( `rjmp .` - "мягкий" стоп без
 * сброса сторожевого таймера).
 */
void fatal_halt(uint8_t led_on)
{
    meas_shutdown();
    delay_ms(200);
    UCSR0B = 0;

    if (led_on) {
        PORTB |= (1 << P_CAL_PU_EN);
        PORTE &= (uint8_t)~(1 << P_PROG_PDO_IN_TX);
    } else {
        PORTB &= (uint8_t)~(1 << P_CAL_PU_EN);
        PORTE |= (1 << P_PROG_PDO_IN_TX);
    }

    for (;;)
        ;
}
