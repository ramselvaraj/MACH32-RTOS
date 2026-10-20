#include "app.h"

#include "board.h"
#include "queue.h"
#include "serial.h"

#include <string.h>

#define COMMAND_CAPACITY 8U
#define CONTROL_PERIOD_MS 10U

static StaticQueue_t command_queue_storage;
static uint8_t command_buffer[COMMAND_CAPACITY * sizeof(motor_command_t)];
static QueueHandle_t commands;
static StaticTask_t task_storage[5];
static StackType_t control_stack[512], input_stack[256], serial_stack[512];
static StackType_t diagnostics_stack[1024], supervisor_stack[256];
static TaskHandle_t tasks[5];
static app_status_t status;
static uint32_t progress[HEALTH_TASK_COUNT];
static volatile bool stop_pending;
static volatile motor_fault_t fault_pending;
static volatile bool terminal_fault;
static volatile uint32_t emergency_generation;
static bool input_valid;
static uint32_t input_ms;
static uint32_t stall_mask;

static void notify_control(void)
{
    if (tasks[TASK_CONTROL] != NULL) {
        xTaskNotifyGive(tasks[TASK_CONTROL]);
    }
}

void app_init(void)
{
    motor_init(&status.motor);
    status.reset_flags = board_reset_flags();
    commands = xQueueCreateStatic(COMMAND_CAPACITY, sizeof(motor_command_t),
                                  command_buffer, &command_queue_storage);
    configASSERT(commands != NULL);
    serial_init();
}

bool app_submit(const motor_command_t *command)
{
    bool accepted = false;
    taskENTER_CRITICAL();
    if (!stop_pending && fault_pending == FAULT_NONE && !terminal_fault
        && command->generation == emergency_generation) {
        accepted = xQueueSend(commands, command, 0U) == pdPASS;
        UBaseType_t depth = uxQueueMessagesWaiting(commands);
        if (depth > status.queue_peak) {
            status.queue_peak = depth;
        }
        if (!accepted) {
            board_motor_disable();
            fault_pending = FAULT_COMMAND_QUEUE;
        }
    }
    taskEXIT_CRITICAL();
    notify_control();
    return accepted;
}

uint32_t app_generation_from_isr(void)
{
    /* Aligned 32-bit access is atomic on this MCU. Writers mask this ISR. */
    return emergency_generation;
}

void app_stop(void)
{
    taskENTER_CRITICAL();
    board_motor_disable();
    stop_pending = true;
    ++emergency_generation;
    taskEXIT_CRITICAL();
    notify_control();
}

void app_fault(motor_fault_t fault)
{
    taskENTER_CRITICAL();
    board_motor_disable();
    fault_pending = fault;
    ++emergency_generation;
    if (fault == FAULT_TASK_STALL) {
        terminal_fault = true;
    }
    taskEXIT_CRITICAL();
    notify_control();
}

void app_fault_from_isr(motor_fault_t fault, BaseType_t *wake)
{
    UBaseType_t mask = taskENTER_CRITICAL_FROM_ISR();
    board_motor_disable();
    fault_pending = fault;
    ++emergency_generation;
    taskEXIT_CRITICAL_FROM_ISR(mask);
    if (tasks[TASK_CONTROL] != NULL) {
        vTaskNotifyGiveFromISR(tasks[TASK_CONTROL], wake);
    }
}

void app_set_input(uint16_t sample, bool valid)
{
    taskENTER_CRITICAL();
    status.potentiometer = sample;
    input_valid = valid;
    input_ms = (uint32_t)xTaskGetTickCount();
    taskEXIT_CRITICAL();
}

void app_get_status(app_status_t *snapshot)
{
    taskENTER_CRITICAL();
    *snapshot = status;
    snapshot->uptime_ms = (uint32_t)xTaskGetTickCount();
    snapshot->applied = board_motor_applied();
    /* A stalled control task cannot publish its own fault state. */
    if (terminal_fault || fault_pending != FAULT_NONE) {
        snapshot->motor.state = MOTOR_FAULT;
        snapshot->motor.fault = terminal_fault ? FAULT_TASK_STALL : fault_pending;
        snapshot->motor.target = 0;
        snapshot->motor.output = 0;
    } else if (stop_pending) {
        if (snapshot->motor.state == MOTOR_RUNNING) {
            snapshot->motor.state = MOTOR_DISABLED;
        }
        snapshot->motor.target = 0;
        snapshot->motor.output = 0;
    }
    taskEXIT_CRITICAL();
}

void app_progress(app_task_t task)
{
    taskENTER_CRITICAL();
    ++progress[task];
    taskEXIT_CRITICAL();
}

void app_get_progress(uint32_t counters[HEALTH_TASK_COUNT])
{
    taskENTER_CRITICAL();
    memcpy(counters, progress, sizeof(progress));
    taskEXIT_CRITICAL();
}

void app_request_stall(unsigned task)
{
#if ENABLE_TEST_HOOKS
    if (task < HEALTH_TASK_COUNT) {
        taskENTER_CRITICAL();
        stall_mask |= 1UL << task;
        taskEXIT_CRITICAL();
    }
#else
    (void)task;
#endif
}

void app_maybe_stall(app_task_t task)
{
    taskENTER_CRITICAL();
    bool stall = (stall_mask & (1UL << task)) != 0U;
    taskEXIT_CRITICAL();
    if (stall) {
        vTaskSuspend(NULL);
    }
}

void app_stack_margins(UBaseType_t margins[5])
{
    for (unsigned index = 0U; index < 5U; ++index) {
        margins[index] = uxTaskGetStackHighWaterMark(tasks[index]);
    }
}

/* Call with the kernel interrupt mask held. STOP/fault discards old commands. */
static void consume_emergency(motor_t *motor, uint32_t now_ms)
{
    if (stop_pending || fault_pending != FAULT_NONE || terminal_fault) {
        motor_fault_t fault = terminal_fault ? FAULT_TASK_STALL : fault_pending;
        if (fault != FAULT_NONE) {
            motor_fault(motor, fault, now_ms);
        } else {
            motor_stop(motor, now_ms);
        }
        (void)xQueueReset(commands);
        stop_pending = false;
        fault_pending = FAULT_NONE;
    }
}

static void control_task(void *argument)
{
    (void)argument;
    motor_t motor;
    motor_init(&motor);
    TickType_t next = xTaskGetTickCount() + pdMS_TO_TICKS(CONTROL_PERIOD_MS);
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        TickType_t wait = (int32_t)(next - now) > 0 ? next - now : 0U;
        (void)ulTaskNotifyTake(pdTRUE, wait);
        app_maybe_stall(TASK_CONTROL);
        uint32_t start = board_micros();
        board_probe(true);
        now = xTaskGetTickCount();
        uint32_t late = 0U, missed = 0U;
        if ((int32_t)(now - next) >= 0) {
            late = (uint32_t)(now - next);
            missed = late / CONTROL_PERIOD_MS;
            next += pdMS_TO_TICKS((missed + 1U) * CONTROL_PERIOD_MS);
        }

        taskENTER_CRITICAL();
        consume_emergency(&motor, (uint32_t)now);
        uint16_t pot = status.potentiometer;
        bool valid = input_valid && (uint32_t)(now - input_ms) < 100U;
        taskEXIT_CRITICAL();

        motor_tick(&motor, pot, (uint32_t)now);
        /* Drain a bounded batch. An emergency can interrupt any iteration. */
        for (unsigned index = 0U; index < COMMAND_CAPACITY; ++index) {
            motor_command_t command;
            if (xQueueReceive(commands, &command, 0U) != pdPASS) {
                break;
            }
            if (!motor_command(&motor, &command, (uint32_t)now)) {
                taskENTER_CRITICAL();
                ++status.rejected_commands;
                taskEXIT_CRITICAL();
            }
        }
        if (motor.state == MOTOR_RUNNING && motor.mode == MOTOR_POT && !valid) {
            motor_fault(&motor, FAULT_ADC, (uint32_t)now);
        }
        motor_tick(&motor, pot, (uint32_t)now);

        /* A STOP/fault cannot race the final output write. USART priority 6 is
         * masked here. No other context may enable the output. */
        taskENTER_CRITICAL();
        consume_emergency(&motor, (uint32_t)now);
        board_motor_apply(motor.output);
        status.motor = motor;
        ++status.control_runs;
        status.missed_periods += missed;
        if (late > status.max_lateness_ms) {
            status.max_lateness_ms = late;
        }
        uint32_t duration = (uint32_t)(board_micros() - start);
        if (duration > status.max_control_us) {
            status.max_control_us = duration;
        }
        taskEXIT_CRITICAL();
        board_probe(false);
        app_progress(TASK_CONTROL);
    }
}

void app_create_tasks(void)
{
    /* Static allocation: depths are in StackType_t words, not bytes. */
    tasks[TASK_CONTROL] = xTaskCreateStatic(control_task, "control", 512U, NULL, 4U,
                                           control_stack, &task_storage[0]);
    tasks[TASK_INPUT] = xTaskCreateStatic(inputs_task, "input", 256U, NULL, 2U,
                                         input_stack, &task_storage[1]);
    tasks[TASK_SERIAL] = xTaskCreateStatic(serial_task, "serial", 512U, NULL, 3U,
                                          serial_stack, &task_storage[2]);
    tasks[TASK_DIAGNOSTICS] = xTaskCreateStatic(diagnostics_task, "diagnostics", 1024U,
                                               NULL, 1U, diagnostics_stack,
                                               &task_storage[3]);
    tasks[4] = xTaskCreateStatic(supervisor_task, "supervisor", 256U, NULL, 5U,
                                 supervisor_stack, &task_storage[4]);
    for (unsigned index = 0U; index < 5U; ++index) {
        configASSERT(tasks[index] != NULL);
    }
}
