/*
 * lib.c: служебные функции работы с памятью.
 *
 * Имена `memcpy`, `memcmp`, `strncmp`, `eeprom_read_byte/write_byte`,
 * `_delay_loop_2`, `delay_ms_approx`, `adc_read_sync`, `adc_start`,
 * `uart_*` и все `__addsf3`/`__mulsf3`/... FID-меток НЕ имеют - это ручные
 * предположения, и принимать их на веру нельзя. Ниже - результат
 * перепроверки по байтам образа.
 */
#include "lc1.h"

void memcpy_P_lc1(void* dst, const void* src_flash, uint16_t n)
{
    memcpy_P(dst, src_flash, n);
}

/*
 * Стандартная avr-libc eeprom_read_byte: ожидание EEWE, запись EEAR,
 * взвод EERE, чтение EEDR.
 */
uint8_t eeprom_read_byte_lc1(uint16_t addr)
{
    return eeprom_read_byte((const uint8_t*)addr);
}

/* Стандартная avr-libc eeprom_write_byte. Обратите внимание: секция */
void eeprom_write_byte_lc1(uint16_t addr, uint8_t val)
{
    eeprom_write_byte((uint8_t*)addr, val);
}

/*
 * с дефектом: байты читаются по одному разу ДО цикла, а внутри цикла ни
 * указатели, ни счётчик не меняются. Поэтому при совпадении первых байт и
 * b[0] != 0 функция не возвращается никогда.
 * Единственные вызовы - команда 'K' в service_cmd.c, и все три попадают
 * ровно в этот случай.
 */
int8_t memcmp_lc1(const char* a_flash, const volatile uint8_t* b_ram, uint16_t n)
{
    uint8_t b0 = *b_ram;
    uint8_t a0 = pgm_read_byte((const uint8_t*)a_flash);

    for (;;) {
        if (a0 != b0)
            return ((int8_t)b0 < (int8_t)a0) ? 1 : -1;

        if (b0 == 0)
            return 0;

        if (n == 0)
            return 0;
        /* n не уменьшается, указатели не сдвигаются - вечный цикл */
    }
}

void dbg_putc(uint8_t c)
{
    (void)c;
}
void dbg_puts_P(const char* s)
{
    (void)s;
}
/*
 * Вызывается с двумя аргументами (
 * то есть это форматный вывод процента прогрева. Тело пустое.
 */
void dbg_printf_P(const char* fmt, int16_t v)
{
    (void)fmt;
    (void)v;
}
