/*
 * analog_out.c: два аналоговых выхода: усреднение, кусочно-линейное
 *                масштабирование по таблице настроек и выдача в ЦАП.
 *
 * Каждый канал настраивается блоком из 14 байт (aout_cfg_t), который
 * лежит в ОЗУ по адресам 0x038A (канал 0) и 0x0398 (канал 1) и грузится
 * из EEPROM 0x18 / 0x26 функцией config_block(). Формула - обычная прямая
 * через две точки с ограничением по краям:
 *
 *      v = сумма / число выборок              (среднее с прошлой выдачи)
 *      v <= in_lo            -> out_lo
 *      v >= in_hi            -> out_hi
 *      иначе  out_lo + (v - in_lo)*(out_hi - out_lo)/(in_hi - in_lo)
 *
 * Значения по умолчанию (flash 0x00E7 и 0x00F5) хорошо объясняют смысл
 * каналов, если считать входом лямбду x 1000:
 *   канал 0: 958...1022 -> 819...82   - эмуляция узкополосного датчика,
 *                                  1.00 В (богато) ... 0.10 В (бедно);
 *   канал 1: 500...1523 -> 0...4095   - 0...5 В на lambda 0.50...1.523
 *                                  (для бензина это AFR 7.35...22.4).
 */
#include "lc1.h"

/*
 * Обрезка стоит ТОЛЬКО на ветке интерполяции: out_lo и out_hi уходят в ЦАП
 * как есть, вместе со знаком (см. dac_write - отрицательное значение там
 * подменяется словом 0x3000).
 */
int16_t aout_scale(uint8_t ch)
{
    volatile const aout_cfg_t* c = &g_aout_cfg[ch];
    int16_t v;

    if (ch != 0)
        v = (int16_t)(g_aout1_acc / (int16_t)g_aout1_cnt);
    else
        v = (int16_t)(g_aout0_acc / (int16_t)g_aout0_cnt);

    if (c->in_lo >= v)
        return c->out_lo;

    if (v >= c->in_hi)
        return c->out_hi;

    return (int16_t)((c->out_lo + (int32_t)(v - c->in_lo) * (c->out_hi - c->out_lo) / (c->in_hi - c->in_lo)) & 0x0FFF);
}

void report_value_out(int16_t value, int16_t mode)
{
    if (mode == 0) {
        cli();
        g_aout0_timer = 0;
        g_aout1_timer = 0;
        sei();

    } else if (mode == 2) {
        dac_write(g_aout_cfg[0].out_warmup, 0);
        dac_write(g_aout_cfg[1].out_warmup, 1);
        return;
    } else if (mode == 3) {
        dac_write(g_aout_cfg[0].out_error, 0);
        dac_write(g_aout_cfg[1].out_error, 1);
        return;
    } else if (mode != 1) {
        return;
    }

    g_aout0_acc += value;
    g_aout1_acc += value;
    g_aout0_cnt++;
    g_aout1_cnt++;

    if (g_spi_state != 0)
        return;

    cli();
    if (g_aout0_timer == 0) {
        g_aout0_timer = g_aout_cfg[0].period;
        sei();
        dac_write(aout_scale(0), 0);
        g_aout0_cnt = 0;
        g_aout0_acc = 0;
    }

    cli();
    if (g_aout1_timer == 0) {
        g_aout1_timer = g_aout_cfg[1].period;
        sei();
        dac_write(aout_scale(1), 1);
        g_aout1_cnt = 0;
        g_aout1_acc = 0;
    }

    sei();
}
