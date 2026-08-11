/*
 * heater_meas.c: измерение сопротивления нагревателя и диагностика
 *                 силовой части.
 *
 * Сопротивление нагревателя считается как Rheat = Ubat / Iheat и хранится
 * В МИЛЛИОМАХ. Единицы доказываются тремя порогами, которые совпадают с
 * опубликованной таблицей LC-1 (документация к версии 1.10F):
 *   g_rheat_min_mohm  = 2000 / 1800 / 1600  -> 2.0 / 1.8 / 1.6 Ohm
 *   константа 30001                          -> максимум 30 Ohm
 *   g_rheat_target_mohm = 9100 / 6000 / 7500 -> 9.1 / 6.0 / 7.5 Ohm
 */
#include "lc1.h"

/*
 * Возврат - код ошибки, 0 если всё в порядке:
 *   9 - напряжение питания ниже минимума для этого типа датчика;
 *   2 - нет тока нагревателя (< 10 отсчётов) либо Rheat > 30 Ohm (обрыв);
 *   1 - Rheat ниже минимума (замыкание нагревателя).
 */
uint8_t heater_resistance_step(void)
{
    int16_t vbat;
    int32_t rheat;

    /* Напряжение питания: отсчёт x 5 (делитель 1/5). */
    vbat          = (int16_t)((int16_t)g_vbat_raw * 5);
    g_vbat_scaled = vbat;

    if (vbat < (int16_t)g_vbat_min_raw)
        return 9;

    if ((int16_t)g_iheat_raw < 10) {
        g_rheat_mohm = 0;
        return 2;
    }

    /*
     * Rheat[мОм] = vbat * 442 / iheat
     * (__mulsi3 с 0x01BA, затем __divmodsi4)
     */
    rheat        = ((int32_t)vbat * 442) / (int32_t)(int16_t)g_iheat_raw;
    g_rheat_mohm = rheat;

    if (rheat < (int32_t)(int16_t)g_rheat_min_mohm)
        return 1;

    if (rheat >= 30001)
        return 2;

    g_rheat_acc += rheat;
    g_rheat_cnt++;

    if (g_rheat_cnt != 4)
        return 0;

    g_rheat_avg_mohm = g_rheat_acc / 4;
    g_rheat_cnt      = 0;
    g_rheat_acc      = 0;

    return 0;
}
