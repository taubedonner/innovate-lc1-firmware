/*
 * compat_oldavr.h: слой совместимости с avr-libc эпохи WinAVR-2005.
 *
 * Подключается принудительно, ключом -include, поэтому исходники править
 * не нужно. Подключается ВСЕГДА, обоими компиляторами: если avr-libc уже
 * знает ISR(), весь файл сворачивается в пустое место.
 *
 * Зачем. Макроса ISR() в avr-libc до версии 1.4 не существовало -
 * обработчики объявлялись как SIGNAL(SIG_OVERFLOW1). Без этого слоя
 * `ISR(TIMER1_OVF_vect)` разбирается компилятором как определение обычной
 * функции с именем ISR: файл с одним обработчиком собирается "успешно", но
 * в объектнике оказывается символ `T ISR` вместо `__vector_NN`, а файл с
 * двумя даёт "redefinition of 'ISR'". То есть без слоя старая сборка
 * бессмысленна, даже когда она проходит.
 *
 * Имена SIG_* взяты из iom64.h того же avr-libc.
 */
#ifndef LC1_COMPAT_OLDAVR_H
#define LC1_COMPAT_OLDAVR_H

#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef ISR

#include <avr/signal.h>

/* В старой avr-libc два РАЗНЫХ макроса:
 *   SIGNAL(sig)    - атрибут `signal`,    прерывания на входе закрыты;
 *   INTERRUPT(sig) - атрибут `interrupt`, компилятор ставит `sei` в прологе,
 *                    то есть разрешает вложенные прерывания.
 * Современный ISR(vect) эквивалентен SIGNAL(); ISR(vect, ISR_NOBLOCK) -
 * эквивалент INTERRUPT().
 *
 * По умолчанию берём SIGNAL, потому что так в образе: ни у одного из девяти
 * обработчиков LC-1 в прологе нет `sei` (проверено на code:00ab, 00e5, 0120,
 * 0149, 101b, 104d, 10db, 11a1, 12b0 - везде push r1/r0/SREG, eor r1,r1 и
 * сразу сохранение регистров).
 *
 * Сборка с -DLC1_OLD_ISR_INTERRUPT переключает на INTERRUPT(). ВНИМАНИЕ:
 * это меняет поведение - обработчики начнут прерывать друг друга. Для этой
 * прошивки опасно: ISR аналогового компаратора ведёт отсчёт полупериодов
 * T1/T2 и переключает PORTA/ACSR. */
#ifdef LC1_OLD_ISR_INTERRUPT
#define ISR(vector)                               \
    void vector(void) __attribute__((interrupt)); \
    void vector(void)
#else
#define ISR(vector)                            \
    void vector(void) __attribute__((signal)); \
    void vector(void)
#endif

/* Имена векторов: новые -> старые. Перечислены только те, что нужны LC-1. */
#define TIMER1_OVF_vect SIG_OVERFLOW1
#define TIMER2_OVF_vect SIG_OVERFLOW2
#define ANALOG_COMP_vect SIG_COMPARATOR
#define ADC_vect SIG_ADC
#define SPI_STC_vect SIG_SPI
#define USART0_UDRE_vect SIG_UART0_DATA
#define USART1_UDRE_vect SIG_UART1_DATA
#define USART0_RX_vect SIG_UART0_RECV
#define USART1_RX_vect SIG_UART1_RECV

#endif /* ISR */

#endif /* LC1_COMPAT_OLDAVR_H */
