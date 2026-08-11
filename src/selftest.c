/*
 * selftest.c: заводской самотест аналогового выхода.
 *
 * Запускается из main только при g_test_mode != 0 (внешняя подтяжка на
 * ADC_SCK, разъём J2). Прогоняет ЦАП по четырём точкам и проверяет отклик
 * на входе LSU_CALRES (ADC4) - то есть предполагает испытательную оснастку,
 * заворачивающую выходы OUT1/OUT2 на этот вход.
 */
#include "lc1.h"

/* Ожидание освобождения SPI после посылки в ЦАП - повторяется четырежды */
#define WAIT_SPI()               \
    do {                         \
        while (g_spi_state != 0) \
            ;                    \
    } while (0)

uint8_t selftest_dac_outputs(void)
{
    int16_t v;
    uint8_t fail;

    /* --- 1: оба канала в 0, выход должен лечь ниже 41 отсчёта --------- */
    dac_write(0, 0);
    WAIT_SPI();
    dac_write(0, 1);
    WAIT_SPI();
    delay_ms(100);

    v    = adc_read_sync(ADMUX_LSU_CALRES);
    fail = (v >= 0x29) ? 1 : 0;
    if (fail)
        return fail;

    /* --- 2: канал A на максимум, ожидаем окно 300...700 ---------------- */
    dac_write(0x0FFF, 0);
    WAIT_SPI();
    v = adc_read_sync(ADMUX_LSU_CALRES);

    /*
     * Следующий adc_read_sync всё равно пишет 0xC7, поэтому эффекта нет;
     * похоже на остаток отладки. Воспроизведено как есть.
     */
    ADCSRA = 0x07;

    /* Проверка окна собрана компилятором в два вычитания с переносом: */
    fail = ((uint16_t)(v - 300) >= 401) ? 1 : 0;
    if (fail)
        return fail;

    /* --- 3: канал A в 0, канал B на максимум ------------------------- */
    dac_write(0, 0);
    WAIT_SPI();
    dac_write(0x0FFF, 1);
    WAIT_SPI();
    delay_ms(100);

    v    = adc_read_sync(ADMUX_LSU_CALRES);
    fail = ((uint16_t)(v - 300) >= 401) ? 1 : 0;
    if (fail)
        return fail;

    /* --- 4: оба канала на максимум, ожидаем не меньше 700 ------------ */
    dac_write(0x0FFF, 0);
    WAIT_SPI();
    delay_ms(100);

    v = adc_read_sync(ADMUX_LSU_CALRES);
    return (v < 700) ? 0 : 1;
}
