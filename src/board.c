#include "board.h"

#include "mach32_adc.h"
#include "mach32_gpio.h"
#include "mach32_iwdg.h"
#include "mach32_rcc.h"
#include "mach32_timer.h"
#include "mach32_usart.h"

static uint32_t reset_flags;
static volatile int16_t applied_output;

static void pin(mach32_gpio_port_t port, uint8_t number,
                mach32_gpio_mode_t mode, uint8_t alternate)
{
    const mach32_gpio_init_t config = {
        .port = port, .pin = number, .mode = mode,
        .otype = MACH32_GPIO_OTYPE_PUSHPULL,
        .speed = MACH32_GPIO_SPEED_MEDIUM,
        .pupd = MACH32_GPIO_PUPD_NONE, .altfn = alternate
    };
    mach32_gpio_init(&config);
}

void board_motor_disable(void)
{
    /* Also works before board_init and from fault/interrupt context.
     * PA6 is an ordinary low GPIO while disabled, not a pending PWM compare. */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;
    GPIOA->BSRR = 1UL << (6U + 16U);
    GPIOA->MODER = (GPIOA->MODER & ~(3UL << 12U)) | (1UL << 12U);
    GPIOB->BSRR = (1UL << 16U) | (1UL << 17U);
    applied_output = 0;
}

void board_init(void)
{
    board_motor_disable();
    pin(MACH32_GPIOB, 0U, MACH32_GPIO_MODE_OUTPUT, 0U);
    pin(MACH32_GPIOB, 1U, MACH32_GPIO_MODE_OUTPUT, 0U);
    reset_flags = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;
    mach32_rcc_system_clock_config();

    /* All four implemented NVIC bits are preemption bits. */
    NVIC_SetPriorityGrouping(0U);

    const mach32_timer_init_t clock = {
        .instance = TIM2, .prescaler = 89U,
        .auto_reload = UINT32_MAX, .auto_reload_preload = 0U
    };
    mach32_timer_init(&clock);
    mach32_timer_start(TIM2); /* APB1 timer clock 90 MHz -> 1 MHz. */

    const mach32_timer_init_t pwm = {
        .instance = TIM3, .prescaler = 44U,
        .auto_reload = 999U, .auto_reload_preload = 1U
    };
    mach32_timer_init(&pwm);
    mach32_timer_pwm_init(TIM3, MACH32_TIMER_CHANNEL_1, 0U);
    mach32_timer_start(TIM3); /* 2 kHz. PA6 remains a low GPIO. */
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(15UL << 24U)) | (2UL << 24U);

    pin(MACH32_GPIOA, 0U, MACH32_GPIO_MODE_ANALOG, 0U);
    const mach32_adc_init_t adc = {
        .instance = ADC1, .channel = 0U,
        .resolution = MACH32_ADC_RESOLUTION_12_BIT,
        .sample_time = MACH32_ADC_SAMPLE_84_CYCLES, .continuous = 0U
    };
    mach32_adc_init(&adc);
    ADC1->CR2 |= ADC_CR2_ADON;
    uint32_t start = board_micros();
    while ((uint32_t)(board_micros() - start) < 10U) {
        __NOP();
    }

    pin(MACH32_GPIOA, 2U, MACH32_GPIO_MODE_ALTFN, 7U);
    pin(MACH32_GPIOA, 3U, MACH32_GPIO_MODE_ALTFN, 7U);
    const mach32_usart_init_t uart = {
        .instance = USART2, .peripheral_clock_hz = 45000000U,
        .baud_rate = 115200U, .mode = MACH32_USART_MODE_TXRX,
        .parity = MACH32_USART_PARITY_NONE,
        .stop_bits = MACH32_USART_STOP_BITS_1
    };
    mach32_usart_init(&uart);
    pin(MACH32_GPIOC, 7U, MACH32_GPIO_MODE_OUTPUT, 0U);
}

void board_motor_apply(int16_t demand)
{
#if MOTOR_OUTPUT_ENABLED
    if (demand < -100 || demand > 100) {
        board_motor_disable();
        return;
    }
    int16_t scaled = (int16_t)((demand * MOTOR_MAX_DUTY_PERCENT) / 100);
    if (scaled == 0) {
        board_motor_disable();
        return;
    }
    /* The state machine supplies the off interval before a reversal. */
    GPIOB->BSRR = scaled > 0
        ? (1UL << 0U) | (1UL << 17U)
        : (1UL << 1U) | (1UL << 16U);
    uint32_t magnitude = (uint32_t)(scaled < 0 ? -scaled : scaled);
    mach32_timer_pwm_set_pulse(TIM3, MACH32_TIMER_CHANNEL_1, magnitude * 10U);
    if (applied_output == 0) {
        TIM3->EGR = TIM_EGR_UG;
        GPIOA->MODER = (GPIOA->MODER & ~(3UL << 12U)) | (2UL << 12U);
    }
    applied_output = scaled;
#else
    (void)demand;
    board_motor_disable();
#endif
}

int16_t board_motor_applied(void)
{
    return applied_output;
}

bool board_adc_read(uint16_t *sample)
{
    ADC1->SR = 0U;
    mach32_adc_start(ADC1);
    uint32_t start = board_micros();
    while ((ADC1->SR & ADC_SR_EOC) == 0U) {
        if ((uint32_t)(board_micros() - start) >= 200U) {
            return false;
        }
    }
    *sample = (uint16_t)(ADC1->DR & 0xFFFU);
    return true;
}

uint32_t board_micros(void) { return TIM2->CNT; }
uint32_t board_reset_flags(void) { return reset_flags; }

void board_probe(bool high)
{
    GPIOC->BSRR = high ? (1UL << 7U) : (1UL << 23U);
}

void board_watchdog_start(void)
{
    /* Nominal timeout only. The internal LSI frequency has tolerance. */
    if (mach32_iwdg_start(1000U, 32000U) == 0U) {
        board_motor_disable();
        NVIC_SystemReset();
    }
}

void board_watchdog_refresh(void) { mach32_iwdg_refresh(); }
