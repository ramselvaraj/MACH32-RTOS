#include "app.h"
#include "board.h"
#include "format.h"
#include "serial.h"

static void field(text_t *text, const char *name, uint32_t value)
{
    text_string(text, name);
    text_unsigned(text, value);
}

void diagnostics_task(void *argument)
{
    (void)argument;
    TickType_t previous = xTaskGetTickCount();
    uint32_t last_report = 0U;
    serial_send("MACH32-RTOS READY\r\n");
    for (;;) {
        app_maybe_stall(TASK_DIAGNOSTICS);
        uint32_t now = (uint32_t)xTaskGetTickCount();
        if (serial_take_status_request() || (uint32_t)(now - last_report) >= 1000U) {
            app_status_t snapshot;
            serial_stats_t serial;
            UBaseType_t margins[5];
            char line[640];
            app_get_status(&snapshot);
            serial_get_stats(&serial);
            app_stack_margins(margins);
            text_t text;
            text_init(&text, line, sizeof(line));
            text_string(&text, "STATUS state=");
            text_string(&text, motor_state_name(snapshot.motor.state));
            text_string(&text, " mode=");
            text_string(&text, motor_mode_name(snapshot.motor.mode));
            text_string(&text, " fault=");
            text_string(&text, motor_fault_name(snapshot.motor.fault));
            text_string(&text, " target=");
            text_signed(&text, snapshot.motor.target);
            text_string(&text, " output=");
            text_signed(&text, snapshot.motor.output);
            text_string(&text, " applied=");
            text_signed(&text, snapshot.applied);
            field(&text, " outputs=", MOTOR_OUTPUT_ENABLED);
            field(&text, " limit=", MOTOR_MAX_DUTY_PERCENT);
            field(&text, " hooks=", ENABLE_TEST_HOOKS);
            field(&text, " pot=", snapshot.potentiometer);
            field(&text, " uptime_ms=", snapshot.uptime_ms);
            text_string(&text, " reset=");
            text_hex(&text, snapshot.reset_flags);
            field(&text, " control_runs=", snapshot.control_runs);
            field(&text, " max_control_us=", snapshot.max_control_us);
            field(&text, " max_late_ms=", snapshot.max_lateness_ms);
            field(&text, " missed=", snapshot.missed_periods);
            field(&text, " rejected=", snapshot.rejected_commands);
            field(&text, " queue_peak=", snapshot.queue_peak);
            field(&text, " invalid=", serial.invalid_lines);
            field(&text, " rx_overflows=", serial.rx_overflows);
            field(&text, " uart_errors=", serial.uart_errors);
            field(&text, " tx_drops=", serial.tx_drops);
            text_string(&text, " stacks=");
            for (unsigned index = 0U; index < 5U; ++index) {
                if (index != 0U) {
                    text_string(&text, ",");
                }
                text_unsigned(&text, (uint32_t)margins[index]);
            }
            text_string(&text, "\r\n");
            if (!text.overflow) {
                serial_send(line);
            }
            last_report = now;
        }
        app_progress(TASK_DIAGNOSTICS);
        (void)xTaskDelayUntil(&previous, pdMS_TO_TICKS(100U));
    }
}
