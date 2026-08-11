/* main.c: инициализация и главный цикл. */
#include "lc1.h"

int main(void)
{
    /*
     * ---------------- Порты,
     * PA0 = I_CONTROL (вых, 1), PA4 = AC_CENT_ENABLE (вых)
     * PB0 = ADC_SYNC# (вых, 1), PB1/PB2 = SPI, PB4 = CAL_PU_EN, PB7 = OC2
     * PD3 = USART_OUT_TX, PE1 = PROG_PDO/IN_TX
     */
    PORTA = 0x01;
    DDRA  = 0x11;
    PORTB = 0x01;
    DDRB  = 0x97;
    PORTC = 0x00;
    DDRC  = 0x00;
    PORTD = 0x08;
    DDRD  = 0x08;
    PORTE = 0x00;
    DDRE  = 0x02;
    PORTF = 0x00;
    DDRF  = 0x00;

    TCCR0  = 0;
    TCNT0  = 0;
    TCCR2  = 0;
    ASSR   = 0;
    TCNT2  = 0;
    OCR2   = 0;
    MCUCR  = 0;
    TIMSK  = 0;
    UBRR0H = 0;

    ACSR   = 0x80; /* ACD = 1 - компаратор выключен до nernst_ac_start() */
    SFIOR  = 0x10; /* бит 4 у ATmega64 зарезервирован - см. неопределённость 1 */
    ADMUX  = ADMUX_VPUMP_SENSE;
    ADCSRA = 0x87; /* ADEN, предделитель /128 -> 125 кГц */

    g_afr_multiplier = eeprom_init_and_read_word(0x12);

    eeprom_read_block_lc1(g_cfg_034F, 1, 8);
    if (g_cfg_034F[0] == 0)
        memcpy_P_lc1(g_cfg_034F, (const void*)0x00DA, 8);

    uart_init();

    g_test_mode = sck_pulldown_detect();

    if (g_is_chain_head == 0)
        UCSR0B |= 0x80;
    if (g_test_mode != 0)
        g_is_chain_head = 1;

    detect_sensor_type();
    config_block(0);

    sei();

    if (g_test_mode != 0) {
        if (selftest_dac_outputs() != 0)
            fatal_halt(1);
        if (serial_loopback_test() != 0)
            fatal_halt(1);
    }

    g_var_037F       = 5;
    g_status         = 4; /* "прогрев" */
    g_report_value   = 0;
    g_uptime_20ms    = 0;
    g_var_0243       = 100;
    g_lambda         = 999.0f;
    g_var_0241       = 0;
    g_ticks_since_ac = 0;
    g_state          = 0;
    g_var_0252       = 0;
    g_error_latched  = 0;

    /* ---------------- Константы по типу датчика, */
    if (g_sensor_type == SENSOR_NTK) {
        g_rheat_min_mohm = 2000;        /* 2.0 Ohm  */
        g_vbat_min_raw   = 2271;        /* 11.09 В */
        g_rich_slope     = 0.71428573f; /* 0x3F36DB0E */
    }
    if (g_sensor_type == SENSOR_LSU42) {
        g_rheat_min_mohm = 1800; /* 1.8 Ohm  */
        g_vbat_min_raw   = 1558; /* 7.61 В */
        g_rich_slope     = 0.71428573f;
    }
    if (g_sensor_type == SENSOR_LSU49) {
        g_rheat_min_mohm = 1600;        /* 1.6 Ohm  */
        g_vbat_min_raw   = 1030;        /* 5.03 В */
        g_rich_slope     = 0.64311999f; /* 0x3F24A234 */
    }

    g_lean_offset = 1.0f - g_rich_slope;

    /*
     * ================================================================= *
     *                        Главный цикл                                *
     * =================================================================
     */
    for (;;) {
        led_task();
        protocol_task();
        command_task();

        if (g_error_code == 0)
            g_error_code = state_machine_step();

        if (g_error_code == 0 && g_state == 8) {
            /* Первый вход в рабочий режим - подтянуть калибровку воздуха */
            if (g_var_0252 == 0) {
                g_air_cal_request = load_air_cal();
                g_var_0252        = 1;
            }

            if (g_nernst_ac_running == 0)
                nernst_ac_start();

            /* Кнопка калибровки: удержание больше 11 опросов - взвести, */
            if (g_cal_btn_count > 11 && g_cal_btn_latched == 0)
                g_cal_btn_latched = 1;

            if (g_cal_btn_count == 0 && g_cal_btn_latched != 0) {
                g_cal_btn_latched = 0;
                if (g_status == 0 || g_status == 1) {
                    if (g_var_0223 == 0 && g_var_0317 == 0)
                        g_air_cal_request = 1;
                }
            }

            if (g_air_cal_request != 0)
                air_cal_step();
            else
                lambda_task();

            /*
             * ---- Автоколебания пропали: попытка перезапуска ---------- *
             * g_ticks_since_ac растёт в тике 20.48 мс и обнуляется ISR
             */
            if (g_ticks_since_ac >= 0x29) {
                if (ACSR & (1 << ACO)) {
                    /* Компаратор "залип" в 1 - сбросить накопители и */
                    PORTA &= 0xF5;
                    ACSR             = 0x1A;
                    g_ticks_since_ac = 0;
                    g_acc_lambda     = 0;
                    g_var_03AE       = 0;
                    g_cnt_lambda     = 1;
                    g_cnt_o2         = 0;
                    report_value_out(500, 0);
                } else if (g_ac_settled == 0) {
                    if (ACSR & (1 << ACO)) {
                        PORTA &= 0xF5;
                        ACSR = 0x1A;
                    } else {
                        PORTA |= (1 << P_DIR_CONTROL) | (1 << P_VN_HYST);
                        ACSR = 0x1B;
                    }
                    g_ticks_since_ac = 0;
                    g_ac_settled     = 1;
                    delay_loop_10cyc(5);
                    PORTA |= (1 << P_AC_CENT_ENABLE);
                    delay_loop_10cyc(2);
                    PORTA &= ~(1 << P_AC_CENT_ENABLE);
                } else {
                    /* Попытка уже была и не помогла - "sensor timing". */
                    g_error_code = 8;
                }
            }
        }

        error_task();

        if (g_state == 8 && g_test_mode != 0 && g_var_0231 >= 25) {
            g_var_022F = (uint16_t)(g_var_022F / g_var_0231);
            eeprom_write_byte_lc1(0, 0xFF);
            fatal_halt((g_var_022F >= 200) ? 1 : 0);
        }
    }
}
