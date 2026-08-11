/*
 * uart.c: низкий уровень двух USART: кольцевые буферы и обработчики.
 *
 * Аргумент port во всех функциях: 0 - USART0 (порт IN цепочки, цепи
 * PROG_PDI/IN_RX на PE0 и PROG_PDO/IN_TX на PE1),
 * иначе USART1 (линия на ПК, PD2/PD3).
 */
#include "lc1.h"

#define UART_BUF_SZ 32

volatile uint8_t g_rx1_tail;             /* @0x0257 */
volatile uint8_t g_rx0_tail;             /* @0x0258 */
volatile uint8_t g_tx0_tail;             /* @0x025A */
volatile uint8_t g_tx1_head;             /* @0x025C */
volatile uint8_t g_tx0_buf[UART_BUF_SZ]; /* @0x025D */
volatile uint8_t g_tx0_head;             /* @0x0282 */
volatile uint8_t g_rx0_buf[UART_BUF_SZ]; /* @0x0283 */
volatile uint8_t g_tx1_tail;             /* @0x02A3 */
volatile uint8_t g_rx1_head;             /* @0x02A4 */
volatile uint8_t g_tx1_buf[UART_BUF_SZ]; /* @0x02AE */
volatile uint8_t g_rx0_head;             /* @0x02CE */
volatile uint8_t g_rx1_buf[UART_BUF_SZ]; /* @0x02CF */

/*
 * ISR(USART0_UDRE_vect)
 * Прерывание передатчика запрещается, когда буфер опустел. Обратите
 */
ISR(USART0_UDRE_vect)
{
    if (g_tx0_tail == g_tx0_head) {
        UCSR0B &= 0xDF;
        return;
    }

    UDR0 = g_tx0_buf[g_tx0_tail];
    g_tx0_tail++;
    if (g_tx0_tail == UART_BUF_SZ)
        g_tx0_tail = 0;

    if (g_tx0_tail == g_tx0_head)
        UCSR0B &= 0xDF;
}

ISR(USART1_UDRE_vect)
{
    if (g_tx1_tail == g_tx1_head) {
        UCSR1B &= 0xDF;
        return;
    }

    UDR1 = g_tx1_buf[g_tx1_tail];
    g_tx1_tail++;
    if (g_tx1_tail == UART_BUF_SZ)
        g_tx1_tail = 0;

    if (g_tx1_tail == g_tx1_head)
        UCSR1B &= 0xDF;
}

/*
 * ISR(USART0_RX_vect)
 * При переполнении байт молча теряется (запись пропускается).
 */
ISR(USART0_RX_vect)
{
    uint8_t d    = UDR0;
    uint8_t next = (uint8_t)(g_rx0_head + 1);

    if (next == UART_BUF_SZ)
        next = 0;

    if (next != g_rx0_tail) {
        g_rx0_buf[g_rx0_head] = d;
        g_rx0_head            = next;
    }
}

ISR(USART1_RX_vect)
{
    uint8_t d    = UDR1;
    uint8_t next = (uint8_t)(g_rx1_head + 1);

    if (next == UART_BUF_SZ)
        next = 0;

    if (next != g_rx1_tail) {
        g_rx1_buf[g_rx1_head] = d;
        g_rx1_head            = next;
    }
}

uint8_t str_eq_8(const char* a, const volatile char* b)
{
    uint16_t i;

    for (i = 0; i < 8; i++) {
        uint8_t ca = (uint8_t)a[i];
        uint8_t cb = (uint8_t)b[i];

        if (ca != cb)
            return 0;
        if (ca == 0)
            break;
    }
    return 1;
}

/* (биты UDRE = 5 и TXC = 6 в UCSRnA). Буферы не используются. */
void uart_send_byte_blocking(uint8_t port, uint8_t byte)
{
    if (port != 0) {
        while (!(UCSR1A & (1 << 5)))
            ;
        UDR1 = byte;
        while (!(UCSR1A & (1 << 6)))
            ;
    } else {
        while (!(UCSR0A & (1 << 5)))
            ;
        UDR0 = byte;
        while (!(UCSR0A & (1 << 6)))
            ;
    }
}

/* Возврат: 0 - байт положен в буфер, 1 - буфер полон. */
uint8_t uart_putchar(uint8_t port, uint8_t byte)
{
    uint8_t next;

    if (port != 0) {
        next = (uint8_t)(g_tx1_head + 1);
        if (next == UART_BUF_SZ)
            next = 0;

        if (next == g_tx1_tail)
            return 1;

        cli();
        g_tx1_buf[g_tx1_head] = byte;
        g_tx1_head            = next;
        UCSR1B |= 0x20;
        sei();
        return 0;
    }

    if (g_is_chain_head != 0 && g_test_mode == 0)
        return 0;

    next = (uint8_t)(g_tx0_head + 1);
    if (next == UART_BUF_SZ)
        next = 0;

    if (next == g_tx0_tail)
        return 1;

    cli();
    g_tx0_buf[g_tx0_head] = byte;
    g_tx0_head            = next;
    UCSR0B |= 0x20;
    sei();
    return 0;
}

/* Старший байт первым - тот же порядок, что и при записи слов в EEPROM. */
void uart_putchar_word(uint8_t port, uint16_t w)
{
    uart_putchar(port, (uint8_t)(w >> 8));
    uart_putchar(port, (uint8_t)w);
}

/* Проверки "буфер пуст" НЕТ - её обязан делать вызывающий код. */
uint8_t uart_getchar(uint8_t port)
{
    uint8_t c;

    if (port != 0) {
        c = g_rx1_buf[g_rx1_tail];
        g_rx1_tail++;
        if (g_rx1_tail == UART_BUF_SZ)
            g_rx1_tail = 0;
    } else {
        c = g_rx0_buf[g_rx0_tail];
        g_rx0_tail++;
        if (g_rx0_tail == UART_BUF_SZ)
            g_rx0_tail = 0;
    }
    return c;
}

static uint8_t chain_probe(void)
{
    uint8_t saved = UCSR0B;
    uint8_t got   = 0;

    delay_ms(1);
    UCSR0B &= 0x7F;

    uart_send_byte_blocking(0, 'H');
    delay_ms(10);

    if (UCSR0A & 0x80) {
        got = UDR0;
        if (got == 'H')
            goto restore;
    }

    uart_send_byte_blocking(0, 'H');
    delay_ms(10);

    if (UCSR0A & 0x80)
        got = UDR0;
    else
        got = 0;

restore:
    UCSR0B |= (uint8_t)(saved & 0x80);
    return got;
}

/*
 * UBRR = 51 при 16 МГц -> 16e6 / (16 * 52) = 19231 бод ~ 19200 -
 * штатная скорость приборов Innovate.
 * UCSRnB = 0x18 = RXEN | TXEN, приёмное прерывание включается в конце.
 */
void uart_init(void)
{
    g_tx0_head = 0;
    g_tx0_tail = 0;
    g_tx1_head = 0;
    g_tx1_tail = 0;
    g_rx0_head = 0;
    g_rx0_tail = 0;
    g_rx1_head = 0;
    g_rx1_tail = 0;

    UBRR0H = 0;
    UBRR0L = 51;
    UCSR0B = 0x18;

    UBRR1H = 0;
    UBRR1L = 51;
    UCSR1B = 0x18;

    g_relay_state = 0;

    if (chain_probe() == 'H') {
        g_behind_lm1    = 0;
        g_is_chain_head = 1;
    } else {
        g_is_chain_head = 0;
    }

    UCSR0B |= 0x80;
    UCSR1B |= 0x80;
    g_prev_rx_byte = 0xFF;
}

volatile uint8_t g_name_dup;     /* @0x0259 */
volatile uint8_t g_relay_cmd;    /* @0x027D */
volatile uint8_t g_relay_state;  /* @0x027E */
volatile uint16_t g_relay_len;   /* @0x027F */
volatile uint8_t g_relay_hdr0;   /* @0x0281 */
volatile uint8_t g_prev_rx_byte; /* @0x02A5 */
