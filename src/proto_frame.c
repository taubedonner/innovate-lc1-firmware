/*
 * proto_frame.c: нижний уровень протокола Innovate MTS (ISP2):
 *                 сборка заголовка, упаковка слов, блокирующее чтение.
 *
 * ФОРМАТ КАДРА. Сначала был выведен из кода, затем сверен с официальной
 * спецификацией "Innovate Serial Protocol 2 (ISP2), preliminary, 5/3/2004"
 * и совпал с ней побитово.
 *
 * Линия: 19200 бод, 8N1. USART1 = порт OUT (данные вниз по цепочке, команды
 * приходят оттуда же), USART0 = порт IN (команды уходят вверх). Пакет =
 * заголовок (1 слово) + N слов; слово = 2 байта, старший первым.
 *
 *   Заголовок (спец. sec.2, "Serial Protocol 2 Header"):
 *       байт1 = 1  R  1  D/S  HF  X  1  <len7>
 *       байт2 = 1 <len6..len0>
 *     бит15 - всегда 1, маркер начала;
 *     бит14 R    - устройство ведёт запись во flash;
 *     бит13 - всегда 1  \ по ним ISP2 отличается от ISP1,
 *     бит9  - всегда 1  / где эти биты нули;
 *     бит12 D/S  - 1 данные датчика, 0 ответ на команду программирования;
 *     бит11 HF   - устройство умеет вести внутренний журнал;
 *     бит10 - резерв;
 *     len - длина ПОСЛЕДУЮЩЕЙ части в СЛОВАХ, без заголовка.
 *   Наблюдаемые байт1: 0xB2 (данные датчика), 0xA2 (ответ на команду),
 *   0xBA (то же, что 0xB2, но с битом HF).
 *
 *   Слово данных: у ОБОИХ байтов бит7 = 0, полезны 7+7 = 14 бит:
 *       байт1 = 0 <v13..v7>   байт2 = 0 <v6..v0>
 *   Именно поэтому mts_send_header принимает длину в БАЙТАХ и делит её на 2:
 *   старший бит частного попадает в бит0 первого байта, младшие семь - во
 *   второй.
 *
 * Команды (спец. sec.4), их коды видны в этом файле и в FUN_code_033a:
 *   'H' синхронизация, 'R'/'r' старт/стоп записи, 'e' стирание,
 *   'c' калибровка по воздуху,
 *   0xCC Listen (+8 байт имени), 0xEC Unlisten,
 *   0xCE Namelist, 0xF3 Typelist.
 */
#include "lc1.h"
#include <avr/pgmspace.h>

/*
 * Константы-идентификаторы во флеше (байтовые адреса 0x008c, 0x0094, 0x00ca -
 * сразу за таблицей векторов, которая занимает 0x0000..0x008b).
 * Отправляются "как есть", по 8 байт = 4 слова.
 *
 * Первые две взяты из спецификации дословно: устройство, стоящее сразу за
 * LM-1 и выполняющее преобразование ISP1 -> ISP2, обязано в ответ на Namelist
 * сначала отдать имя "LM-1" (sec.4.6), а в ответ на Typelist - байты
 * 0,0,'L','M','T','R',0,0 (sec.4.7).
 */
static const uint8_t ident_lm1[8] PROGMEM = /* flash:0x008c */
    {'L', 'M', '-', '1', 0x00, 0x00, 0x00, 0x00};

static const uint8_t ident_lmtr[8] PROGMEM = /* flash:0x0094 */
    {0x00, 0x00, 'L', 'M', 'T', 'R', 0x00, 0x00};

/*
 * Первые 8 байт блока Device Info (Serial2 Protocol Supplement, ответ на
 * команду 'S'). Полный блок - 15 байт, лежит в EEPROM 0x01F0..0x01FE и
 * инициализируется отсюда же (см. eeprom_init_and_read_word).
 *   [0,1] версия ПО, big-endian -> 0x1200 = 1.20  (сходится с именем
 *   [2..5] тип устройства, 4 символа = "LC1 "
 *   [6]   версия процессора = 5
 *   [7]   биты возможностей = 0x32 = 0b0011_0010:
 *           бит1 есть ЦАП, бит4 поддержка NTK, бит5 поддержка Bosch LSU 4.2.
 *         Бит6 (LSU 4.9) НЕ выставлен, хотя прошивка датчик обслуживает -
 *         похоже, поле не обновили при добавлении поддержки.
 */
static const uint8_t ident_lc1[8] PROGMEM = /* flash:0x00ca */
    {0x12, 0x00, 'L', 'C', '1', ' ', 0x05, 0x32};

/*
 * ВНИМАНИЕ: `ori r24,0x80` () - не описка декомпилятора, в образе
 * лежит `80 68` = ori r24,0x80. Бит7 второго байта заголовка ДОЛЖЕН быть
 * единицей, в отличие от байтов данных. Проверено на фактическом вызове
 * mts_send_header(0xB2, 4) (..03a8), который даёт классическую
 * шапку LC-1 "B2 82".
 */
void mts_send_header(uint8_t hdr_hi, uint16_t nbytes)
{
    uint16_t nwords = nbytes >> 1;

    if ((uint8_t)nwords & 0x80)
        hdr_hi |= 0x01;

    uart_putchar(1, hdr_hi);
    uart_putchar(1, (uint8_t)nwords | 0x80);
}

/*
 * Тело штатного пакета LC-1: ровно два слова.
 *
 * Совпадает с таблицей "LC-1 Sub-Packet format" из спецификации (sec. стр. 5)
 * бит в бит.
 */
void mts_send_payload(void)
{
    uint8_t b1;
    uint16_t v;

    b1 = (uint8_t)(((g_status & 7) << 2) | 0x42);

    if ((int8_t)(uint8_t)g_afr_multiplier < 0)
        b1 |= 0x01;

    uart_putchar(1, b1);
    uart_putchar(1, (uint8_t)g_afr_multiplier & 0x7F);

    v = g_report_value;
    uart_putchar(1, (uint8_t)(v >> 7) & 0x3F);
    uart_putchar(1, (uint8_t)v & 0x7F);

    g_var_022E = 1;
}

void proto_send_cmd_hdr(uint8_t code)
{
    uart_putchar(1, (uint8_t)((code & 0x40) | 0xBA));
    uart_putchar(1, 0x8A);
    uart_putchar(1, code);
}

/*
 * Выдаёт один или два блока по 8 байт (= по 4 слова).
 *
 * msg_id - код команды, на которую отвечаем: 0xCE Namelist или 0xF3 Typelist.
 * with_prefix != 0 означает "мы стоим сразу за LM-1 и конвертируем ISP1 в
 * ISP2", тогда спецификация требует сначала отдать данные за LM-1:
 */
void proto_send_ident(uint8_t with_prefix, uint8_t msg_id)
{
    uint8_t i;

    if (with_prefix != 0) {
        for (i = 0; i < 8; i++) {
            if (msg_id == 0xCE)
                uart_putchar(1, pgm_read_byte(&ident_lm1[i]));
            else
                uart_putchar(1, pgm_read_byte(&ident_lmtr[i]));
        }
    }

    for (i = 0; i < 8; i++) {
        if (msg_id == 0xCE)
            uart_putchar(1, g_cfg_034F[i]);
        else
            uart_putchar(1, pgm_read_byte(&ident_lc1[i]));
    }
}

void mts_send_response_hdr(uint8_t hdr_hi, uint16_t nbytes, uint8_t msg_id)
{
    if (msg_id == 0xCE || msg_id == 0xF3) {
        if (g_behind_lm1 != 0)
            nbytes += 16;
        else
            nbytes += 8;
    }

    mts_send_header(hdr_hi, nbytes);

    uart_putchar(1, (uint8_t)(msg_id >> 7));
    uart_putchar(1, (uint8_t)(msg_id & 0x7F));
}

void proto_flush_ack(void)
{
    uint8_t m = g_listen_role;

    if (m == 1 || (m == 2 && g_behind_lm1 != 0))
        mts_send_response_hdr(0xA2, 2, 0xEC);
    else
        uart_putchar(0, 0xEC);

    g_listen_role = 0;
}

/*
 * Ждёт два байта в приёмном кольце USART1 и склеивает их старшим вперёд.
 * Ожидание - "пока голова == хвоста", без сброса сторожевого таймера:
 * если поток оборвётся посреди слова, прибор уйдёт в WDT-сброс.
 */
uint16_t uart1_get_word_blocking(void)
{
    uint8_t hi, lo;

    while (g_rx1_head == g_rx1_tail)
        ;
    hi = uart_getchar(1);

    while (g_rx1_head == g_rx1_tail)
        ;
    lo = uart_getchar(1);

    return (uint16_t)(((uint16_t)hi << 8) + lo);
}

uint8_t uart1_get_byte_blocking(void)
{
    while (g_rx1_head == g_rx1_tail)
        ;
    return uart_getchar(1);
}

/*
 * Две функции ниже лежат в ДРУГИХ участках флеша (
 * их к конкретному файлу по адресам ОЗУ нельзя: собственных переменных у них
 * нет. Держу здесь, потому что по смыслу это тот же обмен по USART1.
 */

void uart1_write_buf(const uint8_t* buf, int16_t n)
{
    int16_t i;

    for (i = 0; i < n; i++)
        uart_putchar(1, buf[i]);
}

void uart1_read_buf(uint8_t* buf, int16_t n)
{
    int16_t i;

    for (i = 0; i < n; i++)
        buf[i] = uart1_get_byte_blocking();
}
