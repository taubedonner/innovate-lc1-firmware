/*
 * cell_measure.c: прямые (синхронные) замеры ячеек датчика.
 *
 * Соответствие внешнему описанию (rotorman.dtt-motorsport.ru, статья 5):
 *   Rnernst:  Rin = ( V(adc3)|фаза+ - V(adc3)|фаза- ) * 8/7   <- режим 0
 *   Rpump  :  Rpump = ((Upump(+) - Upump(-)) / 2) / Ipump,
 *             Ipump = 6.25 мА для LC-1                        <- режим >= 2
 * Деление на 2 и на Ipump выполняется НЕ здесь, а в вызывающем коде;
 * эта функция возвращает "сырую" разность отсчётов АЦП.
 */
#include "lc1.h"

int16_t cell_measure(uint8_t mode)
{
    int16_t result;

    if (mode == 0) {
        /*
         * ---------------- Режим 0: сопротивление ячейки Нернста ----------
         * после подачи импульса нагрузки на RN_MEAS_PULSE (PA2).
         *
         * По схеме PA2 сидит на R21 10 кОм прямо в цепи LSU_VN, а общий
         * электрод LSU_COM удерживается усилителем U1A на 2.5 В. Сама цепь
         * стоит около 2.95 В, поэтому при PA2 = 5 В течёт 205 мкА, при
         * PA2 = 0 течёт -295 мкА, то есть размах 500 мкА и амплитуда ровно
         * +/-250 мкА. Это в точности паспортный предел Bosch на ток измерения
         * Ri ("max. current load <= 250 ?A for RI,N measurement"), так что
         * номинал R21 явно считался по этой строке. Разность отсчётов равна
         * этому току на сопротивлении ячейки, усиленному трактом U3A (x1.5)
         * и U1D (x5.7), то есть суммарно примерно x8.55.
         *
         * Отсчёт снимается через 6.25 мкс после подачи импульса, то есть на
         * эквивалентной частоте порядка 25 кГц - выше паспортной полосы
         * 1...4 кГц. Чем быстрее замер, тем сильнее ёмкость двойного слоя
         * шунтирует электродную часть импеданса и тем МЕНЬШЕ выходит
         * измеренное сопротивление. Отсюда и уставка 200 Ом в прошивке
         * против паспортных 300 Ом: это не противоречие, а следствие более
         * быстрого измерения.
         */
        int16_t v_low, v_high;

        DDRA &= 0xF1;
        PORTA &= 0xF1;
        delay_loop_10cyc(10);

        DDRA |= (1 << P_RN_MEAS_PULSE);
        delay_loop_10cyc(10);

        v_low = adc_read_sync(ADMUX_RN_SENSE);

        PORTA |= (1 << P_RN_MEAS_PULSE);
        delay_loop_10cyc(10);

        v_high = adc_read_sync(ADMUX_RN_SENSE);

        result = (int16_t)(((v_high - v_low) * 8) / 7);

    } else if (mode == 1) {
        /*
         * ---------------- Режим 1: чтение состояния компаратора ----------
         * ветви. Если ACO = 1 -> возвращается 0x20 (сам бит ACO),
         * если ACO = 0 -> возвращается 1 ( = !((ACSR >> 5) & 1) ).
         */
        DDRA |= (1 << P_DIR_CONTROL) | (1 << P_VN_HYST);

        if (ACSR & (1 << ACO)) {
            PORTA &= 0xF5;
            result = (int16_t)(ACSR & (1 << ACO));
        } else {
            PORTA |= (1 << P_DIR_CONTROL) | (1 << P_VN_HYST);
            /* ACO уже проверен выше и равен нулю, так что тут всегда 1. */
            result = (int16_t)((uint8_t)(~(ACSR >> 5)) & 1);
        }

    } else {
        /*
         * ---------------- Режим >=2: сопротивление ячейки насоса ----------
         * поэтому Rpump propto (Upump(+) - Upump(-)).
         * Две зеркальные ветви - по текущему состоянию DIR_CONTROL, чтобы
         * измерение всегда начиналось с текущей полуволны и линия
         * возвращалась в исходное состояние.
         */
        int16_t v1, v2;

        DDRA |= (1 << P_DIR_CONTROL) | (1 << P_VN_HYST);

        if (PORTA & (1 << P_DIR_CONTROL)) {
            delay_loop_10cyc(10);
            PORTA |= (1 << P_AC_CENT_ENABLE);
            v1 = adc_read_sync(ADMUX_VPUMP_SENSE);
            PORTA &= ~(1 << P_AC_CENT_ENABLE);
            PORTA &= ~(1 << P_DIR_CONTROL);
            delay_loop_10cyc(10);
            v2 = adc_read_sync(ADMUX_VPUMP_SENSE);
            PORTA |= (1 << P_DIR_CONTROL);
            result = (int16_t)(v1 - v2);
        } else {
            delay_loop_10cyc(10);
            PORTA |= (1 << P_AC_CENT_ENABLE);
            v1 = adc_read_sync(ADMUX_VPUMP_SENSE);
            PORTA &= ~(1 << P_AC_CENT_ENABLE);
            PORTA |= (1 << P_DIR_CONTROL);
            delay_loop_10cyc(10);
            PORTA |= (1 << P_AC_CENT_ENABLE);
            v2 = adc_read_sync(ADMUX_VPUMP_SENSE);
            PORTA &= ~(1 << P_AC_CENT_ENABLE);
            PORTA &= ~(1 << P_DIR_CONTROL);
            result = (int16_t)(v2 - v1);
        }
    }

    DDRA &= 0xF1;
    PORTA &= 0xF1;

    return result;
}
