/*
 * state_machine.c: главная машина состояний прибора.
 *
 * Возврат - код ошибки (0 = нет).
 */
#include "lc1.h"

uint8_t state_machine_step(void)
{
    uint8_t err = 0;

    if (g_var_0100 != g_state)
        g_var_0100 = g_state;

    if (g_cmd_state == 6) {
        if (g_state != 0)
            meas_shutdown();
        g_state = 0;
        return 0;
    }

    switch (g_state) {
    case 0:
        g_var_0218 = 0;
        report_value_out(0, 2);
        while (g_spi_state != 0)
            ;
        g_var_0217 = 0;

        g_rheat_target_mohm = (int16_t)eeprom_init_and_read_word(10);

        /*
         * Значение из EEPROM используется только если оно ненулевое;
         * иначе подставляется константа по типу датчика.
         */
        if (g_rheat_target_mohm == 0 && g_sensor_type == SENSOR_NTK)
            g_rheat_target_mohm = 9100;
        if (g_sensor_type != SENSOR_NTK)
            g_rheat_target_mohm = (g_sensor_type == SENSOR_LSU49) ? 7500 : 6000;

        if (g_sensor_type == SENSOR_NTK) {
            g_vheat_ramp = 54;
            heater_pwm_set(54);
        } else if (g_sensor_type == SENSOR_LSU42) {
            g_vheat_ramp = 433;
            heater_pwm_set(433);
        } else if (g_sensor_type == SENSOR_LSU49) {
            g_vheat_ramp = 433;
            heater_pwm_set(433);
        }

        g_adc_data_ready = 0;
        g_state          = 1;
        /* провал в состояние 1 - так в ASM (нет rjmp) */

    case 1:
        if (g_adc_data_ready == 0)
            return 0;
        g_adc_data_ready  = 0;
        g_rheat_cold_mohm = 0;
        g_cal_sample_cnt  = 0;
        g_state           = 2;
        /* провал в состояние 2 */

    /*
     * ================= 2: подъём напряжения =====
     * Две скорости нарастания: быстрая до порога, дальше медленная -
     * это "участок с высокой ramprate" и "участок 0.4 В/с" из описания.
     */
    case 2:
        if (g_sensor_type == SENSOR_NTK) {
            heater_duty_update((int16_t)g_vheat_ramp, 0);
            if (g_vheat_ramp < 144) {
                g_vheat_ramp++;
            } else if (g_var_02F5 < 75) {
                if (g_vheat_ramp < 210)
                    g_vheat_ramp++;
            }
        }
        if (g_sensor_type == SENSOR_LSU42) {
            heater_duty_update((int16_t)g_vheat_ramp, 1);
            if (g_vheat_ramp < 1433)
                g_vheat_ramp += 100;
            else if (g_vheat_ramp < 2660)
                g_vheat_ramp += 6;
        }
        if (g_sensor_type == SENSOR_LSU49) {
            heater_duty_update((int16_t)g_vheat_ramp, 1);
            if (g_vheat_ramp < 1433)
                g_vheat_ramp += 100;
            else if (g_vheat_ramp < 2660)
                g_vheat_ramp += 6;
        }
        g_state = 3;
        /* провал в состояние 3 */

    case 3:
        if (g_adc_data_ready == 0)
            return 0;
        g_adc_data_ready = 0;

        if (g_var_0218 != 0) {
            g_var_0218 = 1;
            return 0;
        }

        err = heater_resistance_step();
        /* 0x00C6 - "ht%", то есть вывод процента прогрева. */
        dbg_printf_P((const char*)0x00C6, g_var_02F5);
        delay_ms(2);

        if (err != 0) {
            heater_duty_update(0, 0);
            g_state = 0;
            return err;
        }

        /* Проверка Rnernst: как только ячейка достаточно прогрелась, */
        if (g_sensor_type != SENSOR_NTK && g_var_02F5 >= 65) {
            int16_t rn    = cell_measure(CELL_MEAS_RNERNST);
            g_rnernst_acc = rn;

            if (g_sensor_type == SENSOR_LSU42) {
                if (rn < 81) {
                    /*
                     * ОШИБКА ОРИГИНАЛА (
                     * только что измеренного rn, поэтому номер режима
                     * получается случайным (0...80). Симметричная ветка для
                     * Воспроизведено как есть.
                     */
                    if (cell_measure((uint8_t)rn) != 0) {
                        heater_duty_update(1000, 1);
                        g_state = 4;
                        return 0;
                    }
                }
            } else {
                if (rn < 201) {
                    /*
                     * ОШИБКА ОРИГИНАЛА: cell_measure(1) всегда возвращает
                     * ненулевое значение (0x20 при ACO=1, иначе 1 -
                     * см. cell_measure.c), поэтому условие вырождается
                     * в "rn < 201".
                     */
                    if (cell_measure(CELL_MEAS_ACO) != 0) {
                        heater_duty_update(833, 1);
                        g_state = 4;
                        return 0;
                    }
                }
            }
        }

        /* Фиксация Rheat холодного датчика (точка 0 % прогрева) - */
        if (g_rheat_cold_mohm == 0) {
            if (g_sensor_type == SENSOR_NTK) {
                g_rheat_cold_mohm = (g_rheat_mohm < 3700) ? (int16_t)g_rheat_mohm : 3700;
            } else {
                g_rheat_cold_mohm = 3200;
            }
            g_var_0303 = (g_rheat_mohm > 0) ? (int16_t)(g_rheat_mohm - 1) : 0;
        }

        /*
         * Раз в 10 (NTK) или 5 (LSU) выборок - пересчёт % прогрева.
         * Пересчёт делается только при РОСТЕ Rheat: сопротивление
         */
        g_cal_sample_cnt++;
        if ((g_cal_sample_cnt == 10 && g_sensor_type == SENSOR_NTK) ||
            (g_cal_sample_cnt == 5 && g_sensor_type != SENSOR_NTK)) {
            if ((int32_t)g_var_0303 < g_rheat_mohm) {
                g_var_02F5 = (int16_t)(int8_t)warmup_percent();
                g_var_0303 = (int16_t)g_rheat_mohm;
            }
            g_cal_sample_cnt = 0;
        }
        g_state = 2;
        return 0;

    case 4: {
        int16_t limit;
        int16_t rp, rn;

        if (g_tick_sub != 6)
            return 0;

        limit = (g_sensor_type != SENSOR_NTK) ? 600 : 300;

        while (g_tick_sub != 6)
            ;

        rp            = cell_measure(CELL_MEAS_RPUMP);
        g_cell_r_meas = rp;

        err = (rp < 18) ? 3 : 0;
        if (rp >= limit)
            err = 4;

        rn            = cell_measure(CELL_MEAS_RNERNST);
        g_rnernst_acc = rn;

        if (rn < 2)
            err = 5;
        if (rn >= 501)
            err = 6;

        if (err != 0) {
            heater_duty_update(0, 0);
            g_state = 0;
            return err;
        }

        if (g_sensor_type == SENSOR_NTK) {
            g_state    = 10;
            g_var_0217 = 0;
        } else {
            g_nominal_veff = (g_sensor_type == SENSOR_LSU49) ? 833 : 1000;
            g_state        = 5;
        }
        return 0;
    }

    case 5:
        g_adc_data_ready = 0;

        if (g_sensor_type == SENSOR_NTK) {
            if (eeprom_init_and_read_word(10) == 0) {
                g_var_0212 = 0;
                g_state    = 7;
            } else {
                heater_duty_update(2150, 1);
                g_state = 4;
            }
        } else {
            if (eeprom_init_and_read_word(0x16) == 0) {
                g_heater_cal_active = 0;
                g_state             = 6; /* нет targetRpump -> калибровать */
            } else {
                /* Повторное чтение той же ячейки EEPROM - так в ASM */
                g_heat_target = eeprom_init_and_read_word(0x16);
                g_rpump_phase = 3;
                g_state       = 10;
                g_var_0217    = 0;
            }
        }
        return 0;

    case 6:
        err = heater_cal_step();
        if (err != 0) {
            g_state = 0;
            return err;
        }
        if (g_heater_cal_active != 0)
            return 0;
        g_state = 5;
        return 0;

    case 7:
        err = ntk_heater_cal_step();
        if (err != 0) {
            g_state = 0;
            return err;
        }
        if (g_var_0212 != 0)
            return 0;
        heater_duty_update(2150, 1);
        g_state = 4;
        return 0;

    case 8:
        if (g_rpump_phase == 3) {
            g_rpump_phase++;
            if (g_sensor_type != SENSOR_NTK && g_cell_r_meas >= 11)
                heater_pid_step();
        }

        if (g_adc_data_ready == 0)
            return 0;

        if (g_var_0226 != 0 && g_var_0226 < 0x20)
            g_var_0226++;

        err = heater_resistance_step();

        /* Для NTK температура держится по сопротивлению нагревателя - */
        if (err == 0 && g_sensor_type == SENSOR_NTK) {
            if ((int32_t)g_rheat_target_mohm != g_rheat_mohm)
                heater_rheat_regulate();
        }

        g_adc_data_ready = 0;
        g_var_0217++;

        if (g_var_0217 >= 3 && g_rpump_phase == 0) {
            g_rpump_phase = 1;
            g_var_0217    = 0;
        }

        if (err == 0)
            return 0;

        heater_duty_update(0, 0);
        g_state = 0;
        return err;

    case 9:
        g_var_0217 = 0;
        eeprom_init_and_read_word(10);

        if (g_sensor_type == SENSOR_NTK)
            g_rheat_target_mohm = 9100;
        else
            g_rheat_target_mohm = (g_sensor_type == SENSOR_LSU49) ? 7500 : 6000;

        if (g_sensor_type == SENSOR_NTK) {
            eeprom_store_word(10, 0);
            g_state = 5;
        } else {
            eeprom_store_word(0x16, 0);
            g_state = 4;
        }

        heater_duty_update(g_pid_out, 1);
        return 0;

    /*
     * ================= 10: раскачка ячейки ======
     * Серия симметричных импульсов тока накачки с "центрированием"
     * AC-связи. Компаратор на это время выключен (ACSR = 0).
     */
    case 10:
        ACSR = 0;
        DDRA |= (1 << P_DIR_CONTROL) | (1 << P_VN_HYST);

        PORTA &= 0xF5;
        delay_loop_10cyc(5);
        PORTA |= (1 << P_AC_CENT_ENABLE);
        delay_loop_10cyc(10);
        PORTA &= ~(1 << P_AC_CENT_ENABLE);
        delay_loop_10cyc(20);

        PORTA |= (1 << P_DIR_CONTROL) | (1 << P_VN_HYST);
        delay_loop_10cyc(5);
        PORTA |= (1 << P_AC_CENT_ENABLE);
        delay_loop_10cyc(10);
        PORTA &= ~(1 << P_AC_CENT_ENABLE);
        delay_loop_10cyc(20);

        DDRA &= 0xF5;

        g_var_0217++;
        if (g_var_0217 >= 50) {
            g_state    = 11;
            g_var_0217 = 0;
        }

        if (g_adc_data_ready == 0)
            return 0;
        g_adc_data_ready = 0;
        return 0;

    /*
     * ================= 11: синхронизация ========
     * Направление тока приводится в противофазу с состоянием компаратора,
     * чтобы автоколебания запустились с правильной фазы.
     */
    case 11:
        DDRA |= (1 << P_DIR_CONTROL);

        if ((ACSR & (1 << ACO)) && (PORTA & (1 << P_DIR_CONTROL))) {
            PORTA &= ~(1 << P_DIR_CONTROL);
            delay_loop_10cyc(5);
            PORTA |= (1 << P_AC_CENT_ENABLE);
            delay_loop_10cyc(10);
            PORTA &= ~(1 << P_AC_CENT_ENABLE);
            delay_ms(1);
        }
        if (!(ACSR & (1 << ACO)) && !(PORTA & (1 << P_DIR_CONTROL))) {
            PORTA |= (1 << P_DIR_CONTROL);
            delay_loop_10cyc(5);
            PORTA |= (1 << P_AC_CENT_ENABLE);
            delay_loop_10cyc(10);
            PORTA &= ~(1 << P_AC_CENT_ENABLE);
            delay_ms(1);
        }

        if (g_adc_data_ready == 0)
            return 0;
        g_adc_data_ready = 0;

        g_var_0217++;
        if (g_var_0217 != 20)
            return 0;

        DDRA &= 0xF5;
        g_ticks_since_ac = 0;
        g_var_0243       = 100;
        g_state          = 8;
        return 0;

    default:
        return 0;
    }
}
