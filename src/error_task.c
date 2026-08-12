/*
 * error_task.c: обработка аварий: остановка измерений, выдача кода ошибки
 *                в протокол и в ЦАП, повторные попытки, сброс калибровки.
 *
 * Логика повторов:
 *   ошибка 1     -> до 4 повторов, счётчик g_retry_lo_r  (@0x0233);
 *   ошибки 4 и 6 -> до 4 повторов, счётчик g_retry_misc  (@0x0234);
 *   ошибки 2 и 9 -> повторяются бесконечно, но у ошибки 2 после 60 циклов
 *                  стирается калибровка по воздуху в EEPROM.
 */
#include "lc1.h"

void error_task(void)
{
    uint8_t err;
    uint8_t n;

    if (g_error_code == ERR_NONE) {
        g_error_latched = 0;
        return;
    }

    /* В сервисном режиме любая ошибка, кроме "нет нагревателя", фатальна. */
    if (g_test_mode != 0 && g_error_code != ERR_NO_HEAT_CURR)
        fatal_halt(1);

    if (g_error_latched == 0) {
        /*
         * Первое срабатывание для этой ошибки - глушим измерения.
         * затирает; аргументом это быть не может - остальные четыре
         */
        (void)g_error_code;
        meas_shutdown();
        report_value_out(0, AOUT_ERROR);

        while (g_spi_state != 0)
            ;

        g_error_latched = g_error_code;
        g_err2_cycles   = 0;
        g_status        = STATUS_ERROR;

        /* Код ошибки уходит в поле значения пакета со знаковым */
        g_report_value = (uint16_t)(int16_t)(int8_t)g_error_code;

        err = g_error_code;
        if (err == ERR_VBAT_LOW || err < ERR_NO_HEAT_CURR) {
            heater_set_nominal();

            if (g_error_code < ERR_NO_HEAT_CURR) {
                n = g_retry_lo_r;
                if (n < 4) {
                    g_error_code = ERR_NONE;
                    g_state      = ST_RESET;
                    g_retry_lo_r = (uint8_t)(n + 1);
                }
            }
        } else if (g_error_code == ERR_NERNST_OPEN || g_error_code == ERR_PUMP_OPEN) {
            heater_set_nominal();

            n = g_retry_misc;
            if (n < 4) {
                g_error_code = ERR_NONE;
                g_state      = ST_RESET;
                g_retry_misc = (uint8_t)(n + 1);
            }
        }
    }

    /* Периодическая перепроверка причины для "повторяемых" ошибок. */
    err = g_error_code;

    if (err == ERR_VBAT_LOW) {
        if (g_adc_data_ready == 0)
            return;

        g_adc_data_ready = 0;
        g_error_code     = heater_resistance_step();

        if (g_error_code == ERR_NONE)
            g_error_latched = 0;

        g_state = ST_RESET;
        return;
    }

    if (err != ERR_NO_HEAT_CURR)
        return;

    /* ---------------- ошибка 2: нагреватель не отвечает ---------------- */
    if (g_adc_data_ready == 0)
        return;

    g_adc_data_ready = 0;
    g_error_code     = heater_resistance_step();

    if (g_error_code != ERR_NO_HEAT_CURR) {
        g_state = ST_RESET;
        return;
    }

    if (g_cmd_state == CMD_SERVICE) {
        g_state = ST_RESET;
        return;
    }

    /*
     * Через 60 циклов без нагревателя калибровка по воздуху считается
     * EEPROM 0x0E и 0x10 - T1/T2 из save_air_cal(); 0x16 - третье слово
     */
    if (g_err2_cycles == 60) {
        eeprom_store_word(0x16, 0);
        eeprom_store_word(0x0E, 0);
        eeprom_store_word(0x10, 0);
    }

    if ((int16_t)g_err2_cycles < 61)
        g_err2_cycles++;
}
