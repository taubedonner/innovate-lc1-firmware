/*
 * lc1.h: общие определения восстановленной прошивки Innovate LC-1
 *
 * .data : 0x0100..0x0209   (инициализируется из flash byte 0x50e6)
 * .bss  : 0x020A..0x03B9
 * стек  : 0x10FF
 */
#ifndef LC1_H
#define LC1_H

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

/* ------------------------------------------------------------------ */
/* Выводы МК - по схеме LC1_Schematics-1.png                           */
/* ------------------------------------------------------------------ */
/* PORTA */
#define P_I_CONTROL 0      /* PA0  вкл. источника тока накачки              */
#define P_DIR_CONTROL 1    /* PA1  знак Ipump / коммутация MAX4544          */
#define P_RN_MEAS_PULSE 2  /* PA2  импульс нагрузки ячейки Нернста          */
#define P_VN_HYST 3        /* PA3  гистерезис компаратора                   */
#define P_AC_CENT_ENABLE 4 /* PA4  импульс "центрирования" AC-связи         */

/* PORTB */
#define P_ADC_SYNC_N 0    /* PB0  /SYNC ЦАП AD5312 (он же SS SPI)          */
#define P_ADC_SCK 1       /* PB1  SCK SPI, выведен на разъём J2            */
#define P_ADC_MOSI 2      /* PB2  MOSI SPI -> DIN ЦАП                       */
#define P_CAL_PU_EN 4     /* PB4  подтяжка входа CAL + светодиод D7        */
#define P_TERM_DETECT_R 5 /* PB5  делитель R31/R4                          */
#define P_HEATER_PWM 7    /* PB7  = OC2, ШИМ нагревателя                   */

/* PORTD - второй USART, порт OUT цепочки */
#define P_USART_OUT_RX 2 /* PD2  RXD1                                     */
#define P_USART_OUT_TX 3 /* PD3  TXD1                                     */

/* PORTE - первый USART (порт IN) и входы аналогового компаратора */
#define P_PROG_PDI_IN_RX 0 /* PE0  RXD0, цепь PROG_PDI/IN_RX                */
#define P_PROG_PDO_IN_TX 1 /* PE1  TXD0, цепь PROG_PDO/IN_TX                */
#define P_VN_COMP_CENTER 2 /* PE2  AIN0, вход компаратора VN_COMP_CENTER    */
#define P_REF_2V95 3       /* PE3  AIN1, опора +2.95 В                      */

/* Каналы АЦП (значение ADMUX целиком: REFS0=1 -> опора AVCC = 5 В) */
#define ADMUX_VPUMP_SENSE 0x40 /* ADC0 PF0 напряжение ячейки насоса     */
#define ADMUX_VBAT_SENSE 0x41  /* ADC1 PF1 бортовое питание B+          */
#define ADMUX_IHEAT_SENSE 0x42 /* ADC2 PF2 ток нагревателя              */
#define ADMUX_RN_SENSE 0x43    /* ADC3 PF3 напряжение ячейки Нернста    */
#define ADMUX_LSU_CALRES 0x44  /* ADC4 PF4 калибровочный резистор ДК    */
#define ADMUX_CPU_CAL 0x45     /* ADC5 PF5 кнопка калибровки            */

/*
 * Типы датчика - переменная g_sensor_type (@0x0237).
 * Подтверждено ДВУМЯ независимыми признаками:
 *     0.64311999 у типа 2 - по внешнему описанию 0.6431 применяется к LSU 4.9;
 *     80 / 400 / 200 - опубликованная таблица LC-1 v1.10 даёт 80 для LSU 4.2
 *     и 200 для LSU 4.9.
 */
#define SENSOR_NTK 0   /* NTK1H1  */
#define SENSOR_LSU42 1 /* BOSCH LSU 4.2 */
#define SENSOR_LSU49 2 /* BOSCH LSU 4.9 */

/* ------------------------------------------------------------------ */
/* Прототипы                                                           */
/* ------------------------------------------------------------------ */

/* util_delay.c */
void delay_loop_10cyc(uint16_t n);
void delay_ms(uint16_t n);

/* adc.c */
int16_t adc_read_sync(uint8_t admux);
void adc_start(uint8_t admux);

/* cell_measure.c */
int16_t cell_measure(uint8_t mode);
#define CELL_MEAS_RNERNST 0
#define CELL_MEAS_ACO 1
#define CELL_MEAS_RPUMP 2

/* nernst_ac.c */
void nernst_ac_start(void);
void meas_shutdown(void);
void fatal_halt(uint8_t led_on);

/* eeprom_cfg.c */
void eeprom_read_block_lc1(void* dst, uint16_t src, uint16_t n);
void eeprom_write_block_lc1(const void* src, uint16_t dst, uint16_t n);
void config_block(uint8_t save);
void save_air_cal(void);
uint16_t eeprom_init_and_read_word(uint16_t addr);

/* air_cal.c */
uint8_t load_air_cal(void);
void air_cal_step(void);

/* state_machine.c */
uint8_t state_machine_step(void);

void eeprom_store_word(uint16_t addr, int16_t val);

/* Ещё не восстановлены - прототипы выведены по вызовам */
/*
 * Отладочный вывод. В этой сборке все три - пустые `ret` (
 * 1048), но вызовы с осмысленными аргументами сохранились, поэтому в
 * исходнике функции были. Строки лежат во флеше сразу за таблицей векторов.
 */
void dbg_putc(uint8_t c);
void dbg_puts_P(const char* s);
void dbg_printf_P(const char* fmt, int16_t v);

/* command_task.c */
extern volatile uint8_t g_name_idx;   /* @0x025B - принято байт имени, 0..8 */
extern volatile uint8_t g_name_rx[8]; /* @0x02A6..0x02AD - буфер имени */

/* lambda_calc.c */
float duty_cycle(int32_t t1, int32_t t2);
void lambda_update(void);
void lambda_task(void);

/* analog_out.c - два аналоговых выхода через AD5312 */

/*
 * Блок настроек одного аналогового выхода, 14 байт.
 * ОЗУ: канал 0 @0x038A, канал 1 @0x0398. В EEPROM - по 0x18 и 0x26.
 */
typedef struct {
    uint8_t period;     /* +0  период выдачи, тиков по 20.48 мс (читается */
    uint8_t reserved;   /* +1  ни разу не читается */
    int16_t in_lo;      /* +2  нижняя точка входного диапазона */
    int16_t in_hi;      /* +4  верхняя точка */
    int16_t out_lo;     /* +6  код ЦАП для in_lo */
    int16_t out_hi;     /* +8  код ЦАП для in_hi */
    int16_t out_warmup; /* +10 код ЦАП при старте/прогреве (mode 2, */
    int16_t out_error;  /* +12 код ЦАП при ошибке (mode 3, */
} aout_cfg_t;

extern volatile aout_cfg_t g_aout_cfg[2]; /* @0x038A, @0x0398 */

extern volatile int32_t g_aout0_acc;  /* @0x0347..0x034A */
extern volatile uint16_t g_aout0_cnt; /* @0x0318/0x0319 */
extern volatile int32_t g_aout1_acc;  /* @0x0381..0x0384 */
extern volatile uint16_t g_aout1_cnt; /* @0x032C/0x032D */

int16_t aout_scale(uint8_t ch);
void report_value_out(int16_t value, int16_t mode);

/* uart.c - низкий уровень USART */
uint8_t str_eq_8(const char* a, const volatile char* b);
void uart_send_byte_blocking(uint8_t port, uint8_t byte);
uint8_t uart_putchar(uint8_t port, uint8_t byte);
void uart_putchar_word(uint8_t port, uint16_t w);
uint8_t uart_getchar(uint8_t port);

/* uart.c - кольцевые буферы (нужны блокирующему чтению в proto_frame.c) */
extern volatile uint8_t g_rx0_head; /* @0x02CE */
extern volatile uint8_t g_rx0_tail; /* @0x0258 */
extern volatile uint8_t g_rx1_head; /* @0x02A4 */
extern volatile uint8_t g_rx1_tail; /* @0x0257 */

/* proto_frame.c - кадр протокола Innovate MTS */
void mts_send_header(uint8_t hdr_hi, uint16_t nbytes);
void mts_send_payload(void);
void proto_send_cmd_hdr(uint8_t code);
void proto_send_ident(uint8_t with_prefix, uint8_t msg_id);
void mts_send_response_hdr(uint8_t hdr_hi, uint16_t nbytes, uint8_t msg_id);
void proto_flush_ack(void);
uint16_t uart1_get_word_blocking(void);
uint8_t uart1_get_byte_blocking(void);
void uart1_write_buf(const uint8_t* buf, int16_t n);
void uart1_read_buf(uint8_t* buf, int16_t n);

/* Прочие функции протокола и служебных команд, каждая в своём файле */
void uart_init(void);
uint8_t protocol_task(void);
void command_task(void);
void service_command(uint8_t c);
int8_t memcmp_lc1(const char* a_flash, const volatile uint8_t* b_ram, uint16_t n);
void error_task(void);
uint8_t serial_loopback_test(void);

/* heater_ff.c */
void heater_rheat_regulate(void);

/* heater_pid.c */
void heater_pid_step(void);

/* heater_meas.c */
uint8_t heater_resistance_step(void);

/* heater_cal.c */
uint8_t heater_cal_step(void);
uint8_t ntk_heater_cal_step(void);

/* heater.c */
void heater_pwm_set(uint16_t on);
void heater_set_nominal(void);
void dir_toggle_forever(void);
void heater_duty_update(int16_t vheat_eff, uint8_t mode);
uint8_t warmup_percent(void);

/* dac.c */
void dac_write(int16_t value, uint8_t channel_b);

/* led.c */
void led_task(void);

/* sensor_detect.c */
void detect_sensor_type(void);
void cal_button_poll(void);

/* selftest.c */
uint8_t selftest_dac_outputs(void);

/* term_detect.c */
uint8_t sck_pulldown_detect(void);

/* lib.c - библиотечные функции (FID, уверенность 100 %) */
void memcpy_P_lc1(void* dst, const void* src_flash, uint16_t n);
uint8_t eeprom_read_byte_lc1(uint16_t addr);
void eeprom_write_byte_lc1(uint16_t addr, uint8_t val);

/* ------------------------------------------------------------------ */
/* Глобальные переменные (определены в globals.c)                      */
/* Порядок - по возрастанию адреса в оригинале.                        */
/* ------------------------------------------------------------------ */

/* --- .bss --- */
extern volatile uint8_t g_var_0100; /* @0x0100 - .data, нач. 0xFF; теневая копия g_state */
/*
 * @0x020A - прибор стоит ПЕРВЫМ в цепочке (он же "head"). Определяется
 * тем, что служебный порт USART0 вернул собственную посылку - в разъём IN
 * вставлена заглушка, замыкающая Rx и Tx (ISP2 spec sec.3, "a special
 */
extern volatile uint8_t g_is_chain_head;
/*
 * @0x020B - СЧЁТЧИК подряд принятых пакетов LM-1 (ISP1), 0..3; обнуляется,
 * значение означает "выше по цепочке стоит LM-1, и мы конвертируем
 * ISP1 -> ISP2". Тогда на Namelist/Typelist прибор обязан отвечать и за
 * LM-1 (spec sec.4.6, sec.4.7), из-за чего ответ длиннее на 8 байт, и он же
 * откликается на имя "LM-1" в команде Listen.
 */
extern volatile uint8_t g_behind_lm1;
/*
 * @0x020C - режим Listen:
 *   0 обычная работа,
 *   1 выбраны мы (по собственному имени) - сервисный режим,
 *   2 отозвались за LM-1,
 * как требует spec sec.4.4.
 */
extern volatile uint8_t g_listen_role;
/*
 * Состояние ретранслятора пакетов (protocol_relay.c). Пока != 0, идёт
 * разбор входящего пакета и command_task откладывает свои ответы.
 *   0 ждём заголовок, 1 второй байт заголовка ISP2,
 *   2 пересылка тела, 3 тело пакета LM-1 (ISP1),
 *   4 старший байт слова команды, 5 младший,
 *   6 проход имён в ответе на Namelist.
 */
extern volatile uint8_t g_relay_state;  /* @0x027E */
extern volatile uint8_t g_relay_cmd;    /* @0x027D - собранный код команды */
extern volatile uint16_t g_relay_len;   /* @0x027F/0x0280 - сколько байт осталось */
extern volatile uint8_t g_relay_hdr0;   /* @0x0281 - первый байт заголовка */
extern volatile uint8_t g_name_dup;     /*
                                                 * @0x0259 - сколько приборов выше по
                                                 *   цепочке носят то же имя
                                                 */
extern volatile uint8_t g_prev_rx_byte; /*
                                                 * @0x02A5 - предыдущий байт из USART0,
                                                 *   нужен для поиска начала кадра
                                                 */
/*
 * @0x020D - состояние разбора команд (command_task.c):
 *   0 обычный режим, 1 приём 8 байт имени после Listen,
 *   2 имя совпало с "LM-1", 3 имя совпало с собственным,
 *   4 надо ответить на Typelist, 5 на Namelist,
 *   6 сервисный режим (измерения остановлены).
 */
extern volatile uint8_t g_cmd_state;
extern volatile uint8_t g_tick_flag_proto; /* @0x020E - взводится раз в 20.48 мс */
extern volatile uint8_t g_state;           /* @0x020F - фаза работы, 8 = измерение */
extern volatile uint8_t g_var_0223;        /* @0x0223 */
extern volatile uint8_t g_air_cal_request; /* @0x0224 */
extern volatile uint8_t g_ac_settled;      /* @0x0227 - сбрасывается в ANA_COMP */
extern volatile uint8_t g_error_latched;   /*
                                                   * @0x0229 - код ошибки, для которого
                                                   *   уже выполнена остановка измерений
                                                   */
extern volatile uint8_t g_retry_lo_r;      /* @0x0233 - повторы по ошибке 1, до 4 */
extern volatile uint8_t g_retry_misc;      /* @0x0234 - повторы по ошибкам 4/6, до 4 */
extern volatile uint16_t g_err2_cycles;    /* @0x0255 - циклы без нагревателя, порог 60 */
extern volatile uint16_t g_var_022F;       /* @0x022F/0x0230 */
extern volatile uint16_t g_var_0231;       /* @0x0231/0x0232 */
extern volatile uint8_t g_adc_busy;        /* @0x0235 - 1 пока идёт преобразование по прерыванию */
/*
 * @0x0236 - идёт калибровка нагревателя. Взводится первым заходом в
 * 0bdb) и сбрасывается машиной состояний перед входом в состояние 6
 */
extern volatile uint8_t g_heater_cal_active;
extern volatile uint8_t g_sensor_type;       /* @0x0237 */
extern volatile uint8_t g_tick_sub;          /* @0x0238 - 0..19, шаг 1.024 мс */
extern volatile uint8_t g_error_code;        /* @0x0239 */
extern volatile uint8_t g_spi_state;         /* @0x023A - 0 idle, 1 старший байт ушёл, 2 младший */
extern volatile uint16_t g_status;           /*
                                                   * @0x023E/0x023F - код состояния для протокола:
                                                   *   0 = лямбда, 1 = %O2, 4 = прогрев,
                                                   *   5 = обратный отсчёт калибровки
                                                   */
extern volatile uint8_t g_adc_data_ready;    /* @0x0240 - результаты VBAT/IHEAT готовы */
extern volatile uint8_t g_var_0241;          /* @0x0241 */
extern volatile uint8_t g_ticks_since_ac;    /* @0x0242 - насыщающийся счётчик тиков без переключения компаратора */
extern volatile uint8_t g_var_0243;          /* @0x0243 */
extern volatile uint8_t g_var_0248;          /* @0x0248 */
extern volatile uint8_t g_cal_btn_count;     /* @0x0249 - счётчик удержания кнопки, насыщение 100 */
extern volatile uint8_t g_tick_flag_led;     /* @0x024A */
extern volatile uint8_t g_cal_btn_latched;   /* @0x024B */
extern volatile uint8_t g_nernst_ac_running; /* @0x024C */
extern volatile uint8_t g_test_mode;         /* @0x024D - внешняя подтяжка на SCK */
extern volatile uint16_t g_halfcycles_pos;   /* @0x024E/0x024F */
extern volatile uint16_t g_halfcycles_neg;   /* @0x0250/0x0251 */
extern volatile uint8_t g_var_0252;          /* @0x0252 */
extern volatile uint8_t g_led_prev_state;    /* @0x0253 */
extern volatile uint8_t g_led_prev_error;    /* @0x0254 */

extern volatile int32_t g_rheat_mohm;        /* @0x02EF..0x02F2 - сопротивление нагревателя, мОм */
extern volatile int16_t g_vbat_scaled;       /* @0x02F3/0x02F4 - питание, 1 ед. = 5/1024 В ~ 4.883 мВ */
extern volatile uint16_t g_heat_target;      /* @0x02FB/0x02FC - уставка ПИД: Rnernst при калибровке, Rpump в работе */
extern volatile uint16_t g_heat_duty;        /* @0x02F7/0x02F8 - рассчитанная скважность, 4..254 */
extern volatile int16_t g_rheat_target_mohm; /* @0x02F9/0x02FA - целевое Rheat: 9100 / 6000 / 7500 мОм */
extern volatile int16_t g_rheat_cold_mohm;   /* @0x02FF/0x0300 - Rheat холодного датчика (0 % прогрева) */
extern volatile int16_t g_cal_err_integ;     /* @0x0210/0x0211 - интегратор невязки калибровки */
extern volatile uint8_t g_var_0212;          /* @0x0212 */
extern volatile uint8_t g_var_0217;          /* @0x0217 - счётчик шагов внутри состояния */
extern volatile uint8_t g_var_0218;          /* @0x0218 */
extern volatile uint16_t g_var_0221;         /* @0x0221/0x0222 - число накопленных пар T1/T2 */
extern volatile uint8_t g_var_0226;          /* @0x0226 */
extern volatile int32_t g_rheat_acc;         /* @0x0213..0x0216 - накопитель Rheat (4 выборки) */
extern volatile int16_t g_pid_integ;         /* @0x0219/0x021A - интегратор ПИД, +/-75 */
extern volatile int16_t g_pid_prev_err;      /* @0x021B/0x021C - невязка предыдущей итерации */
extern volatile int16_t g_pid_out;           /* @0x021D/0x021E - Vheat(eff), выход ПИД */
extern volatile uint16_t g_rheat_cnt;        /* @0x021F/0x0220 - счётчик выборок Rheat */
extern volatile uint16_t g_cal_sample_cnt;   /* @0x0301/0x0302 - выборки в текущем окне (до 25) */
extern volatile int16_t g_var_02F5;          /* @0x02F5/0x02F6 - % прогрева (см. state 3) */
extern volatile int16_t g_var_0303;          /* @0x0303/0x0304 - предыдущее Rheat при росте */
/*
 * @0x02FD/0x02FE - переменная с двойным использованием:
 *   в состоянии 2 (прогрев) - эффективное напряжение нагревателя (ramp),
 *   в heater_cal_step()     - счётчик итераций калибровки (до 2000).
 */
extern volatile uint16_t g_vheat_ramp;
extern volatile int16_t g_nominal_veff;    /* @0x0305/0x0306 - NominalVeff: 1000 (LSU4.2) / 833 (LSU4.9) */
extern volatile int32_t g_rheat_avg_mohm;  /* @0x0307..0x030A - усреднённое Rheat, мОм */
extern volatile int16_t g_rnernst_acc;     /* @0x030B/0x030C - накопитель Rnernst */
extern volatile uint8_t g_var_0311;        /* @0x0311 - калибровка по воздуху загружена */
extern volatile uint32_t g_uptime_20ms;    /* @0x030D..0x0310 */
extern volatile uint8_t g_dac_lsb;         /* @0x0312 - младший байт посылки в ЦАП */
extern volatile uint16_t g_led_blink_done; /* @0x0313/0x0314 */
/*
 * @0x0315/0x0316 - измеренное сопротивление ячейки, вход ПИД нагревателя:
 * в рабочем режиме туда кладётся Rpump = Upump(+) - Upump(-) (ISR АЦП),
 */
extern volatile int16_t g_cell_r_meas;
extern volatile uint8_t g_var_0317;        /* @0x0317 */
extern volatile float g_rich_slope;        /* @0x031A..0x031D - 0.71428573 / 0.64311999 */
extern volatile uint16_t g_vbat_raw;       /* @0x031E/0x031F */
extern volatile uint32_t g_t_neg;          /* @0x0320..0x0323 - T2, тики 0.5 мкс */
extern volatile float g_dc_air;            /* @0x0324..0x0327 - DCair, калибровка по воздуху */
extern volatile int32_t g_acc_o2;          /* @0x0328..0x032B - накопитель %O2 */
extern volatile uint16_t g_cnt_o2;         /* @0x022C/0x022D - счётчик выборок %O2 */
extern volatile uint8_t g_var_022E;        /* @0x022E - признак смены режима вывода */
extern volatile uint16_t g_cnt_lambda;     /* @0x022A/0x022B - счётчик выборок лямбды */
extern volatile int32_t g_acc_lambda;      /* @0x03A8..0x03AB - накопитель лямбды */
extern volatile int32_t g_t1_meas;         /* @0x034B..0x034E - T1 текущее */
extern volatile int32_t g_t2_meas;         /* @0x03B6..0x03B9 - T2 текущее */
extern volatile uint32_t g_t_pos;          /* @0x032E..0x0331 - T1, тики 0.5 мкс */
extern volatile uint16_t g_report_value;   /* @0x0334/0x0335 - усреднённое значение, 13 бит */
extern volatile uint16_t g_iheat_raw;      /* @0x0337/0x0338 */
extern volatile float g_lambda;            /* @0x0339..0x033C - текущая лямбда, нач. 999.0f */
extern volatile uint8_t g_t_pos_valid;     /* @0x033D */
extern volatile uint8_t g_aout1_timer;     /* @0x033E - до выдачи канала 1, тик 20.48 мс */
extern volatile uint16_t g_var_03AE;       /* @0x03AE/0x03AF */
extern uint8_t g_cfg_034F[8];              /* @0x034F..0x0356 - блок настроек из EEPROM */
extern volatile uint16_t g_var_035B;       /* @0x035B/0x035C - число завершённых измерений */
extern volatile uint16_t g_led_timer;      /* @0x035D/0x035E */
extern volatile uint16_t g_rheat_min_mohm; /* @0x035F/0x0360 - минимум Rheat: 2000 / 1800 / 1600 мОм */
/*
 * @0x0361/0x0362 - множитель AFR: стехиометрическое отношение выбранного
 * топлива x 10 (147 для бензина). Грузится из EEPROM 0x12, умолчание там
 * 0x0093 = 147. В пакет уходит только младший байт (см. mts_send_payload).
 * ISP2 spec sec.2.1: "AFR multiplier is stoichiometric AFR value of current
 */
extern volatile uint16_t g_afr_multiplier;
extern volatile int32_t g_var_0341;      /* @0x0341..0x0344 - сумма периодов T1+T2 */
extern volatile uint16_t g_vbat_min_raw; /*
                                                   * @0x0345/0x0346 - минимум питания: 2271 / 1558 / 1030
                                                   *   = 11.09 / 7.61 / 5.03 В
                                                   */
extern volatile uint16_t g_var_037F;     /* @0x037F/0x0380 - 5 */
extern volatile float g_lean_offset;     /* @0x037B..0x037E = 1.0f - g_rich_slope */
extern volatile uint16_t g_var_0386;     /* @0x0386/0x0387 */
extern volatile uint8_t g_aout0_timer;   /* @0x03A6 - до выдачи канала 0, тик 20.48 мс */
extern volatile uint16_t g_timer1_ovf;   /* @0x03AC/0x03AD */
extern volatile uint8_t g_rpump_phase;   /* @0x03B3 - автомат замера Rpump: 0..3 */

#endif /* LC1_H */
