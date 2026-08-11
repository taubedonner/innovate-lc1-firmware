/*
 * heater_ff.c: регулятор нагрева по сопротивлению НАГРЕВАТЕЛЯ.
 *
 * Работает в фазе прогрева, пока измерение Rnernst/Rpump ещё недостоверно:
 * обратной связью служит само сопротивление нагревателя, которое растёт с
 * температурой. Соответствует "участку с низкой ramprate до достижения
 * целевого Rheat" из описания прибора.
 *
 * Закон - ПИ (без дифференциальной части):
 *
 *      error = Rheat_изм - Rheat_целевое
 *      integ += error                       (ограничение +/-2150)
 *      Vheat(eff) = 2150 - (2*error + integ)  (ограничение +/-2150)
 *
 * Знак противоположен ПИД по ячейке (heater_pid.c): у нагревателя
 * сопротивление РАСТЁТ с температурой, у керамики ячейки - падает,
 * поэтому здесь error = измеренное - целевое, а там наоборот.
 */
#include "lc1.h"

#define VHEAT_NOMINAL 2150     /* 0x0866, ~10.5 В */
#define VHEAT_LIMIT_LO (-2150) /* 0xF79A            */

void heater_rheat_regulate(void)
{
    int16_t err;
    float acc;
    int16_t u;

    /* Невязка. Берутся только младшие 16 бит 32-битного g_rheat_avg_mohm */
    err = (int16_t)((int16_t)g_rheat_avg_mohm - g_rheat_target_mohm);

    /*
     * --- Интегратор с раздельными ветвями по знаку,
     * Каждая ветвь сначала проверяет, не упёрся ли интегратор в свой
     * предел, и только потом складывает; после сложения - повторное
     */
    if (err > 0) {
        if (g_pid_integ < VHEAT_NOMINAL) {
            g_pid_integ += err;
            if (g_pid_integ >= VHEAT_NOMINAL + 1)
                g_pid_integ = VHEAT_NOMINAL;
        }
    }
    if (err < 0) {
        if (g_pid_integ > VHEAT_LIMIT_LO + 1) {
            g_pid_integ += err;
            if (g_pid_integ < VHEAT_LIMIT_LO)
                g_pid_integ = VHEAT_LIMIT_LO;
        }
    }

    /*
     * --- Воздействие в плавающей точке,
     * Пропорциональный коэффициент = 2 получается тем, что результат
     * преобразования int->float складывается САМ С СОБОЙ: аргумент 1
     */
    acc = (float)(int32_t)err;
    acc = acc + acc;
    acc = acc + (float)(int32_t)g_pid_integ;

    u = (int16_t)acc;

    if (u < VHEAT_LIMIT_LO)
        u = VHEAT_LIMIT_LO;
    if (u >= VHEAT_NOMINAL + 1)
        u = VHEAT_NOMINAL;

    heater_duty_update((int16_t)(VHEAT_NOMINAL - u), 1);
}
