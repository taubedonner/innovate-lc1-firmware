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
 *
 * Паспорт Bosch на LSU 4.9 (Y 258 E00 015e от 14.06.2005, п.1.5) единицы
 * подтверждает независимо:
 *   номинальное напряжение нагревателя            7.5 В
 *   мощность в равновесии на воздухе              ~7.5 Вт  -> 7.5 Ом
 *   холодное сопротивление при комнатной t        3.2 Ом
 *   минимальное холодное при -40 degC               1.8 Ом
 * То есть уставка 7500 - это ровно паспортная рабочая точка LSU 4.9, а
 * порог 1800 совпадает с паспортным минимумом буквально. Отношение
 * горячего к холодному 7500/2000 = 3.75 - обычная величина для платины;
 * в ваттах такое соотношение смысла не имело бы.
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
        return ERR_VBAT_LOW;

    if ((int16_t)g_iheat_raw < 10) {
        g_rheat_mohm = 0;
        return ERR_NO_HEAT_CURR;
    }

    /*
     * Rheat[мОм] = vbat * 442 / iheat
     * (__mulsi3 с 0x01BA, затем __divmodsi4)
     */
    rheat        = ((int32_t)vbat * 442) / (int32_t)(int16_t)g_iheat_raw;
    g_rheat_mohm = rheat;

    if (rheat < (int32_t)(int16_t)g_rheat_min_mohm)
        return ERR_RHEAT_LOW;

    if (rheat >= 30001)
        return ERR_NO_HEAT_CURR;

    g_rheat_acc += rheat;
    g_rheat_cnt++;

    if (g_rheat_cnt != 4)
        return ERR_NONE;

    g_rheat_avg_mohm = g_rheat_acc / 4;
    g_rheat_cnt      = 0;
    g_rheat_acc      = 0;

    return ERR_NONE;
}
