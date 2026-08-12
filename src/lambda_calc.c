/*
 * lambda_calc.c: вычисление лямбды из времён полуволн накачки.
 *
 * Базовая величина прибора - Duty Cycle:
 *
 *      DC = (T1 - T2) / (T1 + T2)
 *
 * где T1 и T2 - длительности положительной и отрицательной полуволн тока
 * накачки в тиках Timer1 (0.5 мкс), измеренные ISR аналогового компаратора
 * (см. nernst_ac.c). DC лежит в диапазоне -1...+1, при T1 = T2 равен нулю,
 * что соответствует lambda = 1.
 */
#include "lc1.h"

float duty_cycle(int32_t t1, int32_t t2)
{
    float diff;
    float sum;

    diff = (float)(t1 - t2);
    sum  = (float)(t2 + t1);

    return diff / sum;
}

/*
 * Полный цикл: DC -> lambda -> значение для вывода -> усреднение для протокола.
 * Все три формулы совпали с опубликованным описанием прибора:
 *
 *      DC       = (T1 - T2) / (T1 + T2)
 *      lambda        = DCair / (DCair - DCexh)                      (бедная смесь)
 *      lambda        = lambda * 0.71428573 + (1 - 0.71428573)            (богатая смесь)
 *      %O2 * 10 = DCexh / DCair * 209 + 6
 */
void lambda_update(void)
{
    float dc;
    float lam;
    int16_t out;

    dc = duty_cycle(g_t1_meas, g_t2_meas);

    /*
     * Сравнение с калибровочным DCair
     * Если текущий DC не меньше DCair, смесь беднее свободного воздуха:
     * lambda фиксируется на 999.0, а наружу отдаётся признак 8192.
     */
    if (!(dc < g_dc_air)) {
        g_lambda = 999.0f;
        out      = 8192;
    } else {
        lam      = g_dc_air / (g_dc_air - dc);
        g_lambda = lam;

        /* Богатая смесь (DC < 0) - "заклон" оси характеристики. */
        if (dc < 0.0f)
            g_lambda = lam * g_rich_slope + g_lean_offset;

        lam = g_lambda;

        if (lam >= 8.19249f) {
            out = 0x1FFF;
        } else {
            out = (int16_t)(lam * 1000.0f);
            if (out < 500)
                out = 500;
        }
    }

    report_value_out(out, AOUT_ACC);

    /*
     * ================= Усреднение для протокола ======================= *
     * Порог 6.0 разделяет два режима вывода: при lambda > 6 прибор отдаёт
     */
    if (g_lambda > 6.0f) {
        int32_t o2;
        uint16_t cnt;

        g_acc_lambda = 0;
        g_cnt_lambda = 0;
        if (g_var_022E != 0) {
            g_acc_o2   = 0;
            g_cnt_o2   = 0;
            g_var_022E = 0;
        }

        o2 = (int32_t)(dc / g_dc_air * 209.0f + 6.0f);
        if (o2 >= 210)
            o2 = 209;

        g_acc_o2 += o2;
        cnt = (uint16_t)(g_cnt_o2 + 1);

        if (g_air_cal_request != 0)
            g_status = STATUS_O2;

        {
            int32_t avg = g_acc_o2 / (int32_t)(int16_t)cnt;
            avg &= 0x1FFF;
            if (avg >= 210)
                avg = 209;
            g_report_value = (uint16_t)avg;

            if (g_test_mode != 0) {
                g_var_022F += (uint16_t)avg;
                g_var_0231++;
            }
        }

        g_cnt_o2   = (uint16_t)(cnt & 0x7FFF);
        g_var_0223 = 0;

    } else {
        uint16_t cnt;

        g_cnt_o2 = 0;
        g_acc_o2 = 0;
        if (g_var_022E != 0) {
            g_cnt_lambda = 0;
            g_acc_lambda = 0;
            g_var_022E   = 0;
        }

        /* Наружу лямбда отдаётся смещённой на 500 - это штатная упаковка */
        g_acc_lambda += (int32_t)(int16_t)(out - 500);
        cnt = (uint16_t)(g_cnt_lambda + 1);

        if (g_air_cal_request != 0)
            g_status = STATUS_LAMBDA;

        g_report_value = (uint16_t)(g_acc_lambda / (int32_t)(int16_t)cnt);
        g_report_value &= 0x1FFF;

        g_cnt_lambda = (uint16_t)(cnt & 0x7FFF);
        g_var_0223   = 1;
    }
}

/*
 * Вызывается из главного цикла. Забирает защёлкнутую ISR компаратора пару
 * времён полуволн, копирует её в рабочие переменные, ведёт суммарную
 * статистику и запускает расчёт.
 */
void lambda_task(void)
{
    if (g_t_pos_valid == 0)
        return;

    /* Без загруженной калибровки по воздуху считать нечего - пара просто */
    if (g_var_0311 != 0) {
        g_t1_meas = (int32_t)g_t_pos;
        g_t2_meas = (int32_t)g_t_neg;

        g_var_0341 += (int32_t)(g_t_neg + g_t_pos);
        g_var_035B++;
        g_var_0248 = 1;

        lambda_update();
    }

    g_t_pos_valid = 0;
}
