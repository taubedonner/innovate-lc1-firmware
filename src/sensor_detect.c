/* sensor_detect.c: определение типа датчика и опрос кнопки калибровки. */
#include "lc1.h"

/*
 * Тип определяется по калибровочному резистору датчика (цепь LSU_CALRES,
 * ADC4). Резистор опрашивается дифференциально: два замера при разных
 * уровнях DIR_CONTROL, берётся модуль разности. У LSU резистор есть,
 * у NTK - нет, поэтому порог 51 отсчёта разделяет типы.
 *
 * В обычном режиме результат может быть переопределён байтом EEPROM 0x01FA -
 * это и есть штатный механизм принудительного выбора типа (в том числе
 * LSU 4.9). В режиме диагностики (g_test_mode) переопределение НЕ работает.
 */
void detect_sensor_type(void)
{
    int16_t v1, v2, d;
    uint8_t cfg;

    PORTA |= (1 << P_I_CONTROL);
    DDRA |= (1 << P_DIR_CONTROL);
    delay_loop_10cyc(10);
    v1 = adc_read_sync(ADMUX_LSU_CALRES);

    PORTA |= (1 << P_DIR_CONTROL);
    delay_loop_10cyc(10);
    v2 = adc_read_sync(ADMUX_LSU_CALRES);

    DDRA &= ~(1 << P_DIR_CONTROL);
    PORTA &= ~(1 << P_DIR_CONTROL);

    d   = (int16_t)(v1 - v2);
    cfg = eeprom_read_byte_lc1(0x01FA);

    if (d < 0)
        d = (int16_t)(-d);

    if (d >= 51) {
        g_sensor_type = SENSOR_LSU42;

        /* Переопределение из EEPROM - только вне режима диагностики */
        if (g_test_mode == 0 && cfg != 0)
            g_sensor_type = eeprom_read_byte_lc1(0x01FA);
    } else {
        if (g_test_mode != 0) {
            g_sensor_type = SENSOR_LSU42;
        } else {
            g_sensor_type = SENSOR_NTK;
            PORTA &= ~(1 << P_I_CONTROL);
        }
    }
}

void cal_button_poll(void)
{
    uint16_t i;

    g_var_0317 = 0;

    for (i = 0; i < 3; i++) {
        int16_t v;

        PORTB |= (1 << P_CAL_PU_EN);
        delay_loop_10cyc(10);
        v = adc_read_sync(ADMUX_CPU_CAL);
        PORTB &= ~(1 << P_CAL_PU_EN);

        if (v < 0xA4) {
            g_var_0317 = 1;
            return;
        }
        delay_ms(1);
    }
}
