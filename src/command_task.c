/*
 * command_task.c: приём команд протокола ISP2 и выдача штатного пакета.
 *
 * Всё поведение сверено со спецификацией "Innovate Serial Protocol 2"
 * (sec.4) и её дополнением "Serial2 Protocol Supplement".
 *
 * Правило спецификации: команда, адресованная не нам, пересылается ВВЕРХ по
 * цепочке, то есть в USART0 (порт IN). Отвечаем мы вниз, в USART1.
 */
#include "lc1.h"

/*
 * Отладочные строки во флеше, сразу за таблицей векторов.
 * Все три уходят в dbg_puts_P, тело которой в этой сборке пустое.
 */
static const char s_rcvd_cmd[] PROGMEM = "rcvd cmd ";        /* flash:0x009c */
static const char s_namelist[] PROGMEM = "namelstcommand\r"; /* flash:0x00a6 */
static const char s_typelist[] PROGMEM = "typelstcommand\r"; /* flash:0x00b6 */

/*
 * Буфер имени, принимаемого после Listen. Адреса 0x025B и 0x02A6 попадают
 * разбор команд и кольцевые буферы, скорее всего, лежали в одном .c.
 */
volatile uint8_t g_name_idx;   /* @0x025B */
volatile uint8_t g_name_rx[8]; /* @0x02A6 */

/*
 * name_matches - сравнение принятого имени с образцом.
 * исходнике один операнд был `char`, другой `int`. На результат это не
 * влияет: оба сравниваемых массива содержат ASCII и нули.
 */
static uint8_t name_matches_ram(const volatile uint8_t* ref)
{
    uint16_t i;

    for (i = 0; i < 8; i++)
        if (g_name_rx[i] != ref[i])
            return 0;
    return 1;
}

static uint8_t name_matches_flash(const char* ref)
{
    uint16_t i;

    for (i = 0; i < 8; i++)
        if ((int16_t)(int8_t)g_name_rx[i] != (int16_t)pgm_read_byte(&ref[i]))
            return 0;
    return 1;
}

void command_task(void)
{
    uint8_t c;
    /* Состояние читается ОДИН раз и дальше используется из регистра: */
    uint8_t st = g_cmd_state;

    /*
     * ==================================================================
     * ==================================================================
     */
    switch (st) {
    case CMD_ANSWER_LISTEN_LM1:  /* имя совпало с "LM-1"   */
    case CMD_ANSWER_LISTEN_SELF: /* имя совпало с нашим    */
        if (g_relay_state != RELAY_IDLE)
            return;

        g_listen_role = (st == CMD_ANSWER_LISTEN_SELF) ? 1 : 2;

        /* Ответ - эхо самой команды Listen, spec sec.4.4. code:0358..035d */
        mts_send_response_hdr(0xA2, 2, 0xCC);

        /* Отозвались собственным именем - переходим в сервисный режим. */
        if (g_listen_role == 1)
            g_cmd_state = CMD_SERVICE;
        break;

    case CMD_ANSWER_TYPELIST: /* надо ответить на Typelist */
        if (g_relay_state != RELAY_IDLE)
            return;

        dbg_puts_P(s_typelist);
        mts_send_response_hdr(0xA2, 2, 0xF3);
        proto_send_ident(g_behind_lm1, 0xF3);
        g_cmd_state = CMD_IDLE;
        break;

    case CMD_ANSWER_NAMELIST: /* надо ответить на Namelist */
        if (g_relay_state != RELAY_IDLE)
            return;

        dbg_puts_P(s_namelist);
        mts_send_response_hdr(0xA2, 2, 0xCE);
        proto_send_ident(g_behind_lm1, 0xCE);
        g_cmd_state = CMD_IDLE;
        break;

    default:
        break;
    }

    /*
     * ==================================================================
     *
     * Шлём только если: разбор команд не в работе, прибор - голова цепочки
     * (spec sec.3: "The device at the beginning of the chain also acts as the
     * timing source"), подошёл тик и мы не в сервисном режиме.
     *
     * ЗАМЕЧАНИЕ: спецификация задаёт период 81.92 мс, а g_tick_flag_proto
     * взводится каждые 20.48 мс (tick_isr.c). То есть LC-1 шлёт пакеты в
     * четыре раза чаще эталонного LM-1. Проверить осциллографом.
     * ==================================================================
     */
    if (g_cmd_state == CMD_IDLE) {
        if (g_is_chain_head != 0 && g_tick_flag_proto != 0) {
            if (g_test_mode == 0) {
                mts_send_header(0xB2, 4);
                mts_send_payload();
            }
            g_tick_flag_proto = 0;
        }
    }

    /*
     * ==================================================================
     * ==================================================================
     */
    if (g_rx1_head == g_rx1_tail)
        return;

    c = uart_getchar(1);

    if (g_test_mode != 0)
        return;

    dbg_puts_P(s_rcvd_cmd);
    dbg_putc(c);
    dbg_putc('\r');

    switch (g_cmd_state) {
    case CMD_IDLE:
        switch (c) {
        case 0xCC: /* Listen, spec sec.4.4 */

            g_cmd_state = CMD_RX_NAME;
            g_name_idx  = 0;
            break;

        case 0xEC: /* Unlisten, spec sec.4.5 */
            proto_flush_ack();
            break;

        case 'S': /* вход в сервисный режим */
            /*
             * Supplement: устройство отвечает на out-of-band команды,
             * если оно голова цепочки ЛИБО было выбрано командой Listen.
             */
            if (g_is_chain_head != 0 || g_listen_role == 1) {
                service_command(c);
                g_cmd_state = CMD_SERVICE;
            } else {
                uart_putchar(0, 'S');
            }
            break;

        case 'c': /* калибровка по воздуху */
            /* spec sec.4.3: команду выполняют все приборы с ШДК и всё равно */
            g_air_cal_request = 1;
            uart_putchar(0, 'c');
            break;

        case 0xCE: /* Namelist, spec sec.4.6 */
        case 0xF3: /* Typelist, spec sec.4.7 */
            /* Отвечает голова цепочки; прибор за LM-1 отвечает и за него. */
            if (g_is_chain_head != 0 || g_behind_lm1 != 0)
                g_cmd_state = (c == 0xF3) ? CMD_ANSWER_TYPELIST : CMD_ANSWER_NAMELIST;
            else
                uart_putchar(0, c);
            break;

        default: /* чужая команда - вверх */
            uart_putchar(0, c);
            break;
        }
        break;

    case CMD_RX_NAME:
        g_name_rx[g_name_idx] = c;
        g_name_idx++;

        if (g_name_idx != 8)
            break;

        if (name_matches_ram(g_cfg_034F)) {
            g_cmd_state = CMD_ANSWER_LISTEN_SELF;
        } else if (g_behind_lm1 != 0) {
            /*
             * spec sec.4.4: на имя "LM-1" отвечает прибор, стоящий сразу за
             * ним. Образец - та же константа, что уходит в ответе.
             */
            if (name_matches_flash((const char*)0x008C))
                g_cmd_state = CMD_ANSWER_LISTEN_LM1;
            /*
             * иначе имя чужое и НЕ наше: оригинал просто выходит, вверх
             * по цепочке команду не передаёт. Похоже на недосмотр -
             */
        } else {
            /* Не нам - пересылаем Listen вместе с именем вверх. */
            uart_putchar(0, 0xCC);
            {
                uint8_t i;
                for (i = 0; i < 8; i++)
                    uart_putchar(0, g_name_rx[i]);
            }
            g_cmd_state = CMD_IDLE;
        }
        break;

    case CMD_SERVICE:
        if (c == 0xEC) { /* Unlisten - выходим */
            proto_flush_ack();
            g_cmd_state = CMD_IDLE;
        } else {
            service_command(c);
        }
        break;

    default:
        break;
    }
}
