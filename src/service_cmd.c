/*
 * service_cmd.c: сервисный ("out-of-band") режим и заводская проверка портов.
 *
 * Набор команд задан документом "Serial2 Protocol Supplement" (9/21/06).
 * Действует правило
 * "строчная буква - чтение, прописная - запись". LC-1 реализует:
 *
 *   'S' выдать 15 байт Device Info          (стандартная)
 *   'P' уйти в загрузчик, режим прошивки    (стандартная)
 *   'n' / 'N' чтение / запись имени прибора (стандартные)
 *   'o' / 'O' чтение / запись конфигурации  (в спецификации это 'c'/'C')
 *   'l' / 'L' чтение / запись множителя AFR (своя)
 *   'Z' сброс калибровочных данных          (своя)
 *   'K' задать тип датчика по названию      (своя, СЛОМАНА - см. ниже)
 */
#include "lc1.h"

/*
 * Загрузчик в boot-секции ATmega64: 0x7E00 слов = последние 1024 байта
 * flash. Разбираемый образ взят из файла обновления прошивки, а он
 * загрузчик не содержит по определению - обновление как раз через него и
 * идёт. Так что проверить содержимое нельзя, и при заливке собранного
 * .hex с полным стиранием кристалла загрузчик будет потерян.
 */
#define BOOTLOADER_ENTRY ((void (*)(void))0x7E00)

/*
 * Названия датчиков во флеше, сравниваются командой 'K'.
 * Строки короче восьми байт, а сравнение идёт по восьми, поэтому в него
 * попадают и байты соседних строк.
 */
static const char s_ntk[] PROGMEM   = "NTKL1H1"; /* flash:0x0103 */
static const char s_lsu49[] PROGMEM = "LSU49";   /* flash:0x010b */
static const char s_lsu42[] PROGMEM = "LSU42";   /* flash:0x0111 */

#define EE_SENSOR_TYPE 0x01FA
#define EE_AFR_MULTIPLIER 0x0012
#define EE_DEVICE_NAME 0x0001

/*
 * Оснастка заворачивает выход одного порта на вход другого, поэтому:
 *   байты, посланные в USART1, должны прийти на USART0,
 *   байты, посланные в USART0, должны прийти на USART1.
 *
 * ОСОБЕННОСТЬ: три чтения в ..1ce6 идут без проверки, что в
 * кольце есть данные, - гарантирована только первая проверка в .
 * Если оснастка вернёт меньше трёх байт, uart_getchar отдаст мусор из
 * буфера, и тест "пройдёт" или "провалится" случайно.
 */
uint8_t serial_loopback_test(void)
{
    uint8_t fail;
    uint8_t c;
    uint16_t i;

    meas_shutdown();

    while (g_rx0_head != g_rx0_tail)
        (void)uart_getchar(0);
    while (g_rx1_head != g_rx1_tail)
        (void)uart_getchar(1);

    uart_putchar(1, 'A');
    uart_putchar(0, 'A');
    uart_putchar(1, 'B');
    uart_putchar(0, 'B');
    uart_putchar(1, 'C');
    uart_putchar(0, 'C');
    delay_ms(100); /* ~50 мс */

    /* Всё принятое отбрасывается - важен только сам факт связи. */
    while (g_rx0_head != g_rx0_tail)
        (void)uart_getchar(0);
    while (g_rx1_head != g_rx1_tail)
        (void)uart_getchar(1);

    /* --- USART1 -> USART0 : шлём три 'A', ждём их на входном порту --- */
    uart_putchar(1, 'A');
    uart_putchar(1, 'A');
    uart_putchar(1, 'A');
    delay_ms(100);

    if (g_rx0_head == g_rx0_tail) {
        fail = 1;
    } else {
        fail = 0;

        for (i = 0; i < 3; i++) {
            c = uart_getchar(0);
            c = (uint8_t)(c + 4);
            uart_putchar(1, c);

            if (c != 'E') {
                fail = 1;
                break;
            }
        }

        /* --- USART0 -> USART1 : три 'B' обратно --- */
        if (fail == 0) {
            uart_putchar(0, 'B');
            uart_putchar(0, 'B');
            uart_putchar(0, 'B');
            delay_ms(100);

            if (g_rx1_head == g_rx1_tail) {
                fail = 1;
            } else if (uart_getchar(1) != 'B' || uart_getchar(1) != 'B' || uart_getchar(1) != 'B') {
                fail = 1;
            }
        }
    }

    delay_ms(500);

    UCSR0B &= (uint8_t)~(1 << RXCIE0);
    UCSR1B &= (uint8_t)~(1 << RXCIE1);
    PORTE &= (uint8_t)~(1 << P_PROG_PDO_IN_TX);

    return fail;
}

void service_command(uint8_t c)
{
    uint8_t* cfg  = (uint8_t*)&g_aout_cfg[0]; /* ОЗУ 0x038A, 2 x 14 байт */
    uint8_t* name = (uint8_t*)g_cfg_034F;     /* ОЗУ 0x034F, 8 байт      */
    uint16_t i;

    switch (c) {
    case 'S':
        /*
         * Тот же блок, что прошивка кладёт в EEPROM 0x01F0..0x01FE при
         * первой инициализации (eeprom_init_and_read_word).
         */
        for (i = 0x00CA; i != 0x00D9; i++)
            uart_putchar(1, pgm_read_byte((const uint8_t*)i));
        break;

    case 'P':
        meas_shutdown();
        PORTB &= (uint8_t)~(1 << P_CAL_PU_EN);
        BOOTLOADER_ENTRY();
        break;

    case 'l':
        uart_putchar_word(1, g_afr_multiplier);
        break;

    case 'L':
        g_afr_multiplier = uart1_get_word_blocking();
        eeprom_store_word(EE_AFR_MULTIPLIER, (int16_t)g_afr_multiplier);
        uart_putchar(1, '\r');
        break;

    /* ---- 'o' / 'O' - конфигурация аналоговых выходов, 2 x 14 байт ---- */
    case 'o':
        uart1_write_buf(cfg, 14);
        uart1_write_buf(cfg + 14, 14);
        break;

    case 'O':
        uart1_read_buf(cfg, 14);
        uart1_read_buf(cfg + 14, 14);
        config_block(1);
        uart_putchar(1, '\r');
        break;

    /* ---- 'n' / 'N' - имя прибора, 8 символов ---- */
    case 'n':
        /*
         * Читается ИЗ EEPROM, а не из ОЗУ. Адреса 1..8
         * Счётчик инкрементируется ДО чтения, поэтому байт 0 пропущен.
         */
        for (i = 0; i != 8;) {
            i++;
            uart_putchar(1, eeprom_read_byte_lc1(EE_DEVICE_NAME - 1 + i));
        }
        break;

    case 'N':
        uart1_read_buf(name, 8);
        eeprom_write_block_lc1(name, EE_DEVICE_NAME, 8);
        uart_putchar(1, '\r');
        break;

    case 'Z':
        eeprom_store_word(0x0A, 0);
        eeprom_store_word(0x0C, 0);
        /*
         * ОШИБКА ОРИГИНАЛА: адрес 0x0E обнуляется ТРИЖДЫ подряд
         * save_air_cal() пишет калибровку по воздуху в пару 0x0E/0x10,
         * поэтому после "сброса калибровки" половина её (T2) остаётся в
         * EEPROM. Похоже на копипаст: строки различаются только адресом.
         */
        eeprom_store_word(0x0E, 0);
        eeprom_store_word(0x0E, 0);
        eeprom_store_word(0x0E, 0);
        eeprom_store_word(0x14, 0);
        eeprom_store_word(0x16, 0);
        uart_putchar(1, '\r');
        break;

    case 'K':
        uart1_read_buf(name, 8);

        /*
         * ДЕФЕКТ ОРИГИНАЛА, критический. Сравнение идёт функцией по адресу
         * счётчик (см. memcmp_lc1 в lib.c):
         *
         * То есть при совпадении первых байт и b[0] != 0 функция зависает.
         * Все три сравнения ниже именно такие: любое принятое название,
         * начинающееся с 'L' (для первых двух) или с 'N' (для третьего),
         * подвесит прибор до срабатывания сторожевого таймера, а название,
         * не совпадающее ни с одним, просто ничего не сделает.
         * Команда 'K' в этой сборке нерабочая.
         */
        if (memcmp_lc1(s_lsu42, name, 8) == 0)
            eeprom_write_byte_lc1(EE_SENSOR_TYPE, SENSOR_LSU42);

        if (memcmp_lc1(s_lsu49, name, 8) == 0)
            eeprom_write_byte_lc1(EE_SENSOR_TYPE, SENSOR_LSU49);

        if (memcmp_lc1(s_ntk, name, 8) == 0)
            eeprom_write_byte_lc1(EE_SENSOR_TYPE, SENSOR_NTK);

        uart_putchar(1, '\r');
        break;

    default:
        break;
    }
}
