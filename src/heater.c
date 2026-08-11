/*
 * heater.c: управление ШИМ нагревателя датчика.
 *
 * Нагреватель включён ключом Q1 (AON7418) с затвора PB7 = OC2.
 * Timer2 в режиме Fast PWM, предделитель /64 -> 976.56 Гц.
 * Он же служит источником системного тика (см. tick_isr.c).
 */
#include "lc1.h"

void heater_pwm_set(uint16_t on)
{
    if (on == 0) {
        TCCR2 &= 0xCF;
        PORTB &= 0x7F;
        return;
    }

    OCR2 = 10;

    /* Полная инициализация выполняется только если ШИМ ещё не запущен */
    if ((TCCR2 & 0x30) == 0) {
        TIMSK &= 0x3F;
        TIFR &= 0x3F;
        SFIOR |= 0x02;
        TCNT2 = 0;
        TCCR2 = 0x6C; /*
                                             *                   COM2=10 (неинверт.),
                                             *                   CS2=100 (/64)
                                             */
        TIMSK |= 0x40;
    }
}

/*
 * Пересчёт "эффективного напряжения нагревателя" в скважность ШИМ.
 * Внешнее описание алгоритма LC-1 даёт формулу
 *      HtrDC% = Vheat(eff) * 100% / Ubatt
 * здесь она в целочисленном виде с масштабом 256 (полная шкала OCR2):
 *      duty = (vheat_eff * 256) / делитель,  затем ограничение [4 ... 254]
 */
void heater_duty_update(int16_t vheat_eff, uint8_t mode)
{
    int32_t num;
    int32_t den;
    int32_t duty;

    num = (int32_t)vheat_eff * 256;

    if (mode == 0) {
        if (vheat_eff == 0) {
            TCCR2 &= 0xCF; /* COM2 = 00, выход OC2 отключён */
            PORTB &= 0x7F; /* PB7 = 0, ключ закрыт          */
        }
        den = (int32_t)(int16_t)g_iheat_raw;
    } else if (mode == 1) {
        den = (int32_t)(int16_t)g_vbat_scaled;
    } else {
        return;
    }

    duty = num / den;

    if (duty >= 254)
        duty = 254;
    else if (duty < 4)
        duty = 4;

    OCR2        = (uint8_t)duty;
    g_heat_duty = (uint16_t)duty;
}

/*
 * Линейная интерполяция "процента прогрева":
 *
 *      p1000 = (current - cold) * 1000 / (target - cold)      [0 ... 999]
 *      return p1000 / 10                                      [0 ... 99]
 *
 * где cold    = g_rheat_cold_mohm  (@0x02FF, int16)  - значение у холодного датчика,
 *     current = g_rheat_mohm  (@0x02EF, int32)  - текущее,
 *     target  = g_rheat_target_mohm  (@0x02F9, int16)  - целевое для прогретого.
 *
 * Совпадает с описанием прибора: "показания warming up % - это оставшийся
 * процент до достижения сенсором прогретого состояния, сопротивление
 * холодного сенсора принято за 0 %, целевое - за 100 %".
 */
uint8_t warmup_percent(void)
{
    int16_t cold;
    int32_t p1000;

    cold = (int16_t)g_rheat_cold_mohm;

    p1000 = ((int32_t)g_rheat_mohm - (int32_t)cold) * 1000;

    p1000 /= (int32_t)((int16_t)g_rheat_target_mohm - cold);

    if (p1000 >= 1000)
        p1000 = 999;

    g_status       = 4;
    g_report_value = (uint16_t)p1000;

    return (uint8_t)(p1000 / 10);
}

/*
 * Включает ШИМ и выставляет номинальное эффективное напряжение по типу
 * датчика: LSU 4.2 -> 1000, LSU 4.9 -> 833, NTK -> 2150 (последний с mode 0).
 */
void heater_set_nominal(void)
{
    heater_pwm_set(1);

    if (g_sensor_type == SENSOR_LSU42)
        heater_duty_update(1000, 1);
    if (g_sensor_type == SENSOR_LSU49)
        heater_duty_update(833, 1);
    if (g_sensor_type == SENSOR_NTK)
        heater_duty_update(2150, 0);
}

/*
 * Бесконечное переключение DIR_CONTROL / VN_HYST с полупериодом ~5 мс.
 * режим генерации меандра на PA1/PA3.
 */
void dir_toggle_forever(void)
{
    DDRA |= (1 << P_DIR_CONTROL) | (1 << P_VN_HYST);

    for (;;) {
        PORTA &= 0xF5;
        delay_ms(10);
        PORTA |= (1 << P_DIR_CONTROL) | (1 << P_VN_HYST);
        delay_ms(10);
    }
}
