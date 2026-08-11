/*
 * adc.c: низкоуровневый доступ к АЦП.
 *
 * ADCSRA в прошивке принимает три значения:
 *   0x87 = ADEN                     | ADPS=111 (/128)  - исходная настройка в main
 *   0xC7 = ADEN | ADSC              | ADPS=111         - синхронный запуск
 *   0x97 = ADEN |        ADIF       | ADPS=111         - сброс флага ADIF
 *   0xCF = ADEN | ADSC | ADIE       | ADPS=111         - запуск с прерыванием
 * Тактовая АЦП = 16 МГц / 128 = 125 кГц, преобразование 13 тактов ~ 104 мкс.
 */
#include "lc1.h"

/*
 * Синхронное (блокирующее) преобразование. Прерывание АЦП НЕ используется:
 * ожидание идёт по флагу ADIF, который затем сбрасывается записью 1.
 */
int16_t adc_read_sync(uint8_t admux)
{
    uint8_t lo, hi;

    ADMUX  = admux;
    ADCSRA = 0xC7;

    while (!(ADCSRA & (1 << ADIF)))
        ;

    lo     = ADCL;
    hi     = ADCH;
    ADCSRA = 0x97;

    return (int16_t)(((uint16_t)hi << 8) | lo);
}

void adc_start(uint8_t admux)
{
    g_adc_busy = 1;
    ADMUX      = admux;
    ADCSRA     = 0xCF;
}
