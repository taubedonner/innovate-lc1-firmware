/*
 * protocol_relay.c: ретрансляция входящих пакетов ISP2 с добавлением
 *                    собственных данных.
 *
 * Прибор в середине цепочки принимает пакет от вышестоящего устройства на
 * USART0 (порт IN), пересылает его в USART1 (порт OUT) и дописывает в конец
 * свои два слова, увеличив длину в заголовке. Спецификация ISP2, sec.2:
 * "Each device adds a sub-packet and modifies the header length word by the
 * number of data words contained in its data packet".
 *
 * Дополнительно функция:
 *   - конвертирует пакеты LM-1 старого протокола ISP1 в ISP2, надевая на
 *     них заголовок (spec: "The first intermediate device that sees a LM-1
 *     packet without a header adds the header in front");
 *   - дописывает свои имя/Device Info в ответы на Namelist и Typelist;
 *   - следит за чужими именами в ответе на Namelist и переименовывает себя,
 *     если кто-то выше по цепочке носит такое же имя.
 */
#include "lc1.h"

uint8_t protocol_task(void)
{
    uint8_t c;

    if (g_rx0_head == g_rx0_tail)
        return 0;

    /* Голова цепочки ничего не ретранслирует: выше неё никого нет. */
    if (g_is_chain_head != 0)
        return 0;

    c = uart_getchar(0);

    /*
     * Режим сквозного пропуска. Spec sec.4.4: прибор, ответивший за LM-1,
     * "ignores all input data, but just passes any data on". Разбор
     */
    if (g_listen_role >= 2) {
        uart_putchar(1, c);
        while (g_rx0_head != g_rx0_tail)
            uart_putchar(1, uart_getchar(0));
    }

    /* Выбраны командой Listen (роль 1) - молчим и ничего не разбираем. */
    if (g_listen_role != 0) {
        g_prev_rx_byte = c;
        return 0;
    }

    switch (g_relay_state) {
    /*
     * ==================================================================
     * ==================================================================
     */
    case RELAY_IDLE:
        /*
         * Байт заголовка засчитывается, только если предыдущий байт был
         * байтом ДАННЫХ (бит7 = 0). Оба байта заголовка имеют бит7 = 1,
         * поэтому так отсекается "середина" уже идущего заголовка.
         */
        if ((int8_t)g_prev_rx_byte < 0) {
            uart_putchar(1, c);
            break;
        }

        if ((c & 0xA2) == 0xA2) {
            /* Заголовок ISP2: биты 15, 13 и 9 слова (они же биты 7, 5, 1 */
            g_relay_state = RELAY_HDR2;

            /* Бит0 первого байта - старший (восьмой) бит длины. */
            g_relay_len  = (uint16_t)((c & 1) << 7);
            g_relay_hdr0 = c;

            /* Пришёл нормальный ISP2 - значит LM-1 впереди нет. */
            g_behind_lm1 = 0;

        } else if ((c & 0xA2) == 0x80) {
            /*
             * Пакет LM-1 старого протокола: бит7 есть, а битов 5 и 1 нет.
             * Spec sec.2: "Protocol Version 1 is distinguished from protocol
             * version 2 by bits 13, 9 and 7. In Version 1 these bits are
             * always 0". Пакет LM-1 - 8 слов, то есть 16 байт, один из
             */
            g_relay_len   = 15;
            g_relay_state = RELAY_BODY_ISP1;
            g_relay_hdr0  = c;

        } else {
            uart_putchar(1, c);
        }
        break;

    /*
     * ==================================================================
     * ==================================================================
     */
    case RELAY_HDR2:
        /* Оба байта заголовка обязаны иметь бит7. Иначе кадр битый - */
        if ((int8_t)c >= 0 || (int8_t)g_prev_rx_byte >= 0) {
            g_relay_state = RELAY_IDLE;
            break;
        }

        /* Длина: старшая часть уже лежит в g_relay_len, младшие семь бит */
        g_relay_len = (uint16_t)((g_relay_len + (c & 0x7F)) * 2);
        g_relay_cmd = 0;

        /* Бит4 первого байта = бит12 слова = D/S: данные датчика или */
        if (g_relay_hdr0 & 0x10) {
            /* Данные: сразу выдаём заголовок с длиной, увеличенной на наши */
            mts_send_header(g_relay_hdr0, (uint16_t)(g_relay_len + 4));
            g_relay_state = RELAY_BODY;
        } else {
            /* Ответ на команду - надо ещё разобрать слово команды. */
            g_relay_state = RELAY_CMD_HI;
        }
        break;

    /*
     * ==================================================================
     * ==================================================================
     */
    case RELAY_BODY:
        g_relay_len--;
        uart_putchar(1, c);

        if (g_relay_len != 0)
            break;

        switch (g_relay_cmd) {
        case 0x00: /* обычные данные датчика */
            mts_send_payload();
            break;
        case 0xCE: /* Namelist  */
        case 0xF3: /* Typelist  */
            proto_send_ident(0, g_relay_cmd);
            break;
        case 0xCC: /* Listen перехватил другой */
            g_listen_role = 3;
            break;
        case 0xEC: /* Unlisten - все свободны */
            g_listen_role = 0;
            break;
        default:
            break;
        }

        g_relay_state  = RELAY_IDLE;
        g_prev_rx_byte = c;
        return 1;

    /*
     * ==================================================================
     * ==================================================================
     */
    case RELAY_BODY_ISP1:
        g_relay_len--;

        /* Байт с битом7 внутри тела означает, что кадр оборвался и начался */
        if ((int8_t)c < 0) {
            g_relay_len   = 0;
            g_relay_state = RELAY_IDLE;
            break;
        }

        /*
         * Надеваем на пакет LM-1 заголовок ISP2 длиной 10 слов:
         * 8 слов LM-1 + наши 2. Бит R (запись во flash) переносится из
         */
        proto_send_cmd_hdr(g_relay_hdr0);
        uart_putchar(1, c);

        /*
         * Первые три пакета LM-1 только считаются - прибор убеждается, что
         * впереди действительно LM-1, и лишь потом начинает полноценную
         */
        if (g_behind_lm1 < 3) {
            g_behind_lm1++;
            g_relay_state = RELAY_IDLE;
        } else {
            g_relay_state = RELAY_BODY;
        }
        break;

    /*
     * ==================================================================
     *
     * Spec sec.2: "In a command response packet the next data word contains
     * the bit0..bit6 of the command in Bit0..Bit6 of the word ... Bit 8 to
     * Bit 14 of the word contains bit 7 to 13 of the command".
     * Здесь берётся только бит7 кода команды.
     * ==================================================================
     */
    case RELAY_CMD_HI:
        g_relay_len--;
        g_relay_cmd   = (uint8_t)((c & 1) ? 0x80 : 0x00);
        g_relay_state = RELAY_CMD_LO;
        break;

    /*
     * ==================================================================
     * ==================================================================
     */
    case RELAY_CMD_LO: {
        uint16_t len_before = g_relay_len;

        g_relay_len = (uint16_t)(len_before - 1);
        g_relay_cmd = (uint8_t)(g_relay_cmd | (c & 0x7F));

        /*
         * Заголовок ответа выдаётся заново; mts_send_response_hdr сама
         * добавит к длине 8 или 16 байт для Namelist/Typelist.
         */
        mts_send_response_hdr(g_relay_hdr0, (uint16_t)(len_before + 1), g_relay_cmd);

        /*
         * Для Namelist с заданным именем переходим в режим слежения за
         * чужими именами. Имя считается заданным, если первый его байт в
         */
        if (g_relay_cmd == 0xCE && eeprom_read_byte_lc1(1) != 0) {
            g_relay_state = RELAY_NAMES;
            g_name_dup    = 0;
        } else {
            g_relay_state = RELAY_BODY;
        }
        break;
    }

    /*
     * ==================================================================
     * ==================================================================
     */
    case RELAY_NAMES:
        g_relay_len--;
        uart_putchar(1, c);

        g_name_rx[g_name_idx] = c;
        g_name_idx++;

        if (g_name_idx == 8) {
            /* Здесь используется ИСПРАВНАЯ восьмибайтовая сверка */
            if (str_eq_8((const char*)g_cfg_034F, (const char*)g_name_rx))
                g_name_dup++;
            g_name_idx = 0;
        }

        if (g_relay_len != 0)
            break;

        /* --- пакет кончился --- */

        /*
         * Если кто-то выше по цепочке носит то же имя, прибор сам себя
         * переименовывает: первые четыре символа сохраняются, дальше
         * ставится '.', буква по номеру дубликата и конец строки.
         */
        if (g_name_dup != 0) {
            g_cfg_034F[4] = '.';
            g_cfg_034F[5] = (uint8_t)(g_name_dup + 0x61);
            g_cfg_034F[6] = 0;
        }

        proto_send_ident(0, g_relay_cmd);
        eeprom_write_block_lc1(g_cfg_034F, 1, 8);

        g_relay_state  = RELAY_IDLE;
        g_prev_rx_byte = c;
        return 1;

    default:
        break;
    }

    g_prev_rx_byte = c;
    return 0;
}
