#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_COMMAND_TIMEOUT_MS 500U
#define MOTOR_REVERSE_DELAY_MS 100U

typedef enum { MOTOR_DISABLED, MOTOR_RUNNING, MOTOR_FAULT } motor_state_t;
typedef enum { MOTOR_SERIAL, MOTOR_POT } motor_mode_t;
typedef enum {
    FAULT_NONE, FAULT_TIMEOUT, FAULT_ADC, FAULT_RX_OVERFLOW,
    FAULT_COMMAND_QUEUE, FAULT_TASK_STALL
} motor_fault_t;
typedef enum { CMD_ARM_SERIAL, CMD_ARM_POT, CMD_SET, CMD_KEEPALIVE } command_kind_t;

typedef struct {
    command_kind_t kind;
    int16_t value;
    uint32_t received_ms;
    uint32_t generation; /* RTOS layer rejects commands received before an emergency. */
} motor_command_t;

typedef struct {
    motor_state_t state;
    motor_mode_t mode;
    motor_fault_t fault;
    int16_t target;
    int16_t output;
    int8_t last_direction;
    uint32_t off_since_ms;
    uint32_t last_command_ms;
} motor_t;

void motor_init(motor_t *motor);
bool motor_command(motor_t *motor, const motor_command_t *command, uint32_t now_ms);
void motor_tick(motor_t *motor, uint16_t potentiometer, uint32_t now_ms);
void motor_stop(motor_t *motor, uint32_t now_ms);
void motor_fault(motor_t *motor, motor_fault_t fault, uint32_t now_ms);
const char *motor_state_name(motor_state_t state);
const char *motor_mode_name(motor_mode_t mode);
const char *motor_fault_name(motor_fault_t fault);

#endif
