/*
 * eeprom_cfg.c: блочный доступ к EEPROM и хранение калибровок.
 *
 * Карта EEPROM, восстановленная по вызовам:
 *   0x00        признак "настройки испорчены" (0xFF)
 *   0x0A        целевое Rheat прогретого датчика, мОм
 *   0x0E, 0x10  T1 и T2 калибровки по свободному воздуху (по 2 байта)
 *   0x16        targetRpump после калибровки нагревателя
 *   0x18, 0x26  два блока настроек по 14 байт -> ОЗУ 0x038A и 0x0398
 */
#include "lc1.h"

void eeprom_read_block_lc1(void* dst, uint16_t src, uint16_t n)
{
    uint16_t i;
    for (i = 0; i < n; i++)
        ((uint8_t*)dst)[i] = eeprom_read_byte_lc1((uint16_t)(src + i));
}

void eeprom_write_block_lc1(const void* src, uint16_t dst, uint16_t n)
{
    uint16_t i;
    for (i = 0; i < n; i++)
        eeprom_write_byte_lc1((uint16_t)(dst + i), ((const uint8_t*)src)[i]);
}

void eeprom_store_word(uint16_t addr, int16_t val)
{
    eeprom_write_byte_lc1(addr, (uint8_t)((uint16_t)val >> 8));
    eeprom_write_byte_lc1(addr + 1, (uint8_t)val);
}

/*
 * mode != 0 - сохранить, mode == 0 - загрузить два блока по 14 байт.
 * После загрузки проверяются два поля: если хоть одно >= 500, настройки
 * считаются испорченными - в EEPROM[0] пишется 0xFF и делается повторная
 */
void config_block(uint8_t save)
{
    uint8_t* base = (uint8_t*)&g_aout_cfg[0];

    if (save) {
        eeprom_write_block_lc1(base, 0x18, 14);
        eeprom_write_block_lc1(base + 14, 0x26, 14);
        return;
    }

    eeprom_read_block_lc1(base, 0x18, 14);
    eeprom_read_block_lc1(base + 14, 0x26, 14);

    if (g_aout_cfg[0].in_lo >= 500 || g_aout_cfg[1].in_lo >= 500) {
        eeprom_write_byte_lc1(0, 0xFF);
        config_block(0);
    }
}

void save_air_cal(void)
{
    int32_t t1 = g_t1_meas;
    int32_t t2 = g_t2_meas;

    while (((uint32_t)(t1 | t2) & 0xFFFF0000UL) != 0) {
        t1 >>= 1;
        t2 >>= 1;
    }

    eeprom_store_word(14, (int16_t)t1);
    eeprom_store_word(16, (int16_t)t2);
}

/*
 * Помимо чтения слова выполняет ПЕРВИЧНУЮ ИНИЦИАЛИЗАЦИЮ EEPROM: если по
 * адресу 0 не лежит признак 0x5A, вся конфигурация записывается заново
 * значениями по умолчанию из flash. Порядок байт - старший первым, как и
 * в eeprom_store_word.
 *
 * Значения по умолчанию во flash (байтовые адреса):
 *   0x00CA (15 байт) -> EEPROM 0x01F0..0x01FE : "\x12\0LC1 \x05" "2p" ...
 *   0x00DF (8 байт)  -> EEPROM 0x01..0x08     : "LC-1\0\0\0\0" - имя прибора
 *   0x00E7 (14 байт) -> блок настроек 1 -> EEPROM 0x18
 *                      = 0, 958, 1022, 819, 82, 0x8170, 0x8170
 *   0x00F5 (14 байт) -> блок настроек 2 -> EEPROM 0x26
 *                      = 0, 500, 1523, 0, 4095, 0x84B4, 0x84B4
 *   EEPROM 0x0A..0x16 (слова с шагом 2) обнуляются
 *   EEPROM 0x12 = 0x0093
 */
uint16_t eeprom_init_and_read_word(uint16_t addr)
{
    uint8_t hi, lo;

    if (eeprom_read_byte_lc1(0) != 0x5A) {
        uint16_t a;
        uint8_t* base = (uint8_t*)&g_aout_cfg[0];

        eeprom_write_byte_lc1(0, 0x5A);

        for (a = 10; a != 0x18; a += 2)
            eeprom_store_word(a, 0);

        memcpy_P_lc1(base, (const void*)0x00E7, 14);
        memcpy_P_lc1(base + 14, (const void*)0x00F5, 14);

        eeprom_write_block_lc1(base, 0x18, 14);
        eeprom_write_block_lc1(base + 14, 0x26, 14);

        for (a = 0x01F0; a != 0x01FF; a++)
            eeprom_write_byte_lc1(a, pgm_read_byte((const uint8_t*)(0x00CA + (a - 0x01F0))));

        for (a = 1; a <= 8; a++)
            eeprom_write_byte_lc1(a, pgm_read_byte((const uint8_t*)(0x00DF + (a - 1))));

        eeprom_store_word(0x12, 0x0093);
    }

    hi = eeprom_read_byte_lc1(addr);
    lo = eeprom_read_byte_lc1(addr + 1);

    return (uint16_t)(((uint16_t)hi << 8) + lo);
}
