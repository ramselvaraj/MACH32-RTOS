#include "motor.h"

#include <string.h>

static void disable(motor_t *motor, uint32_t now_ms)
{
    if (motor->output != 0) {
        motor->off_since_ms = now_ms;
    }
    motor->target = 0;
    motor->output = 0;
}

static void check_timeout(motor_t *motor, uint32_t now_ms)
{
    if (motor->state == MOTOR_RUNNING && motor->mode == MOTOR_SERIAL
        && (uint32_t)(now_ms - motor->last_command_ms) >= MOTOR_COMMAND_TIMEOUT_MS) {
        motor_fault(motor, FAULT_TIMEOUT, now_ms);
    }
}

void motor_init(motor_t *motor)
{
    memset(motor, 0, sizeof(*motor));
    motor->state = MOTOR_DISABLED;
    motor->mode = MOTOR_SERIAL;
}

void motor_stop(motor_t *motor, uint32_t now_ms)
{
    disable(motor, now_ms);
    /* STOP cannot erase a fault. Only an explicit ARM can clear it. */
    if (motor->state != MOTOR_FAULT) {
        motor->state = MOTOR_DISABLED;
    }
}

void motor_fault(motor_t *motor, motor_fault_t fault, uint32_t now_ms)
{
    disable(motor, now_ms);
    motor->state = MOTOR_FAULT;
    motor->fault = fault;
}

bool motor_command(motor_t *motor, const motor_command_t *command, uint32_t now_ms)
{
    check_timeout(motor, now_ms);
    if ((uint32_t)(now_ms - command->received_ms) >= MOTOR_COMMAND_TIMEOUT_MS) {
        return false;
    }
    switch (command->kind) {
        case CMD_ARM_SERIAL:
        case CMD_ARM_POT:
            if (motor->state == MOTOR_RUNNING) {
                return false;
            }
            disable(motor, now_ms);
            motor->mode = command->kind == CMD_ARM_POT ? MOTOR_POT : MOTOR_SERIAL;
            motor->state = MOTOR_RUNNING;
            motor->fault = FAULT_NONE;
            motor->last_command_ms = command->received_ms;
            return true;
        case CMD_SET:
            if (command->value < -100 || command->value > 100) {
                return false;
            }
            if (motor->state != MOTOR_RUNNING || motor->mode != MOTOR_SERIAL) {
                return false;
            }
            motor->target = command->value;
            motor->last_command_ms = command->received_ms;
            return true;
        case CMD_KEEPALIVE:
            if (motor->state != MOTOR_RUNNING || motor->mode != MOTOR_SERIAL) {
                return false;
            }
            motor->last_command_ms = command->received_ms;
            return true;
        default:
            return false;
    }
}

void motor_tick(motor_t *motor, uint16_t potentiometer, uint32_t now_ms)
{
    check_timeout(motor, now_ms);
    if (motor->state != MOTOR_RUNNING) {
        return;
    }
    if (motor->mode == MOTOR_POT) {
        if (potentiometer > 4095U) {
            potentiometer = 4095U;
        }
        motor->target = (int16_t)(((uint32_t)potentiometer * 100U) / 4095U);
    }
    if (motor->target == 0) {
        disable(motor, now_ms);
        return;
    }
    int8_t direction = motor->target > 0 ? 1 : -1;
    if (motor->last_direction != 0 && direction != motor->last_direction) {
        if (motor->output != 0) {
            motor->off_since_ms = now_ms;
            motor->output = 0;
        }
        if ((uint32_t)(now_ms - motor->off_since_ms) < MOTOR_REVERSE_DELAY_MS) {
            return;
        }
    }
    motor->output = motor->target;
    motor->last_direction = direction;
}

const char *motor_state_name(motor_state_t state)
{
    switch (state) {
        case MOTOR_DISABLED: return "DISABLED";
        case MOTOR_RUNNING: return "RUNNING";
        case MOTOR_FAULT: return "FAULT";
        default: return "UNKNOWN";
    }
}

const char *motor_mode_name(motor_mode_t mode)
{
    return mode == MOTOR_POT ? "POT" : "SERIAL";
}

const char *motor_fault_name(motor_fault_t fault)
{
    switch (fault) {
        case FAULT_NONE: return "NONE";
        case FAULT_TIMEOUT: return "TIMEOUT";
        case FAULT_ADC: return "ADC";
        case FAULT_RX_OVERFLOW: return "RX_OVERFLOW";
        case FAULT_COMMAND_QUEUE: return "COMMAND_QUEUE";
        case FAULT_TASK_STALL: return "TASK_STALL";
        default: return "UNKNOWN";
    }
}
