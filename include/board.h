#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

#ifndef MOTOR_OUTPUT_ENABLED
#define MOTOR_OUTPUT_ENABLED 0
#endif
#ifndef MOTOR_MAX_DUTY_PERCENT
#define MOTOR_MAX_DUTY_PERCENT 25
#endif

#if MOTOR_OUTPUT_ENABLED != 0 && MOTOR_OUTPUT_ENABLED != 1
#error MOTOR_OUTPUT_ENABLED must be zero or one
#endif
#if MOTOR_MAX_DUTY_PERCENT < 1 || MOTOR_MAX_DUTY_PERCENT > 100
#error MOTOR_MAX_DUTY_PERCENT must be between 1 and 100
#endif

void board_init(void);
void board_motor_disable(void);
void board_motor_apply(int16_t demand);
int16_t board_motor_applied(void);
bool board_adc_read(uint16_t *sample);
uint32_t board_micros(void);
uint32_t board_reset_flags(void);
void board_probe(bool high);
void board_watchdog_start(void);
void board_watchdog_refresh(void);

#endif
