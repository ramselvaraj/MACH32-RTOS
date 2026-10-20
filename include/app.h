#ifndef APP_H
#define APP_H

#include "FreeRTOS.h"
#include "task.h"
#include "motor.h"
#include "health.h"

typedef enum { TASK_CONTROL, TASK_INPUT, TASK_SERIAL, TASK_DIAGNOSTICS } app_task_t;
typedef struct {
    motor_t motor;
    uint32_t uptime_ms;
    uint32_t control_runs;
    uint32_t max_control_us;
    uint32_t max_lateness_ms;
    uint32_t missed_periods;
    uint32_t rejected_commands;
    uint32_t reset_flags;
    uint16_t potentiometer;
    int16_t applied;
    UBaseType_t queue_peak;
} app_status_t;

void app_init(void);
void app_create_tasks(void);
bool app_submit(const motor_command_t *command);
uint32_t app_generation_from_isr(void);
void app_stop(void);
void app_fault(motor_fault_t fault);
void app_fault_from_isr(motor_fault_t fault, BaseType_t *wake);
void app_set_input(uint16_t sample, bool valid);
void app_get_status(app_status_t *status);
void app_progress(app_task_t task);
void app_get_progress(uint32_t counters[HEALTH_TASK_COUNT]);
void app_maybe_stall(app_task_t task);
void app_request_stall(unsigned task);
void app_stack_margins(UBaseType_t margins[5]);

void inputs_task(void *argument);
void serial_task(void *argument);
void supervisor_task(void *argument);
void diagnostics_task(void *argument);

#endif
