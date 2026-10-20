#include "app.h"
#include "board.h"

void supervisor_task(void *argument)
{
    (void)argument;
    health_t health;
    health_init(&health, (uint32_t)xTaskGetTickCount());
    board_watchdog_start();
    bool failed = false;
    TickType_t previous = xTaskGetTickCount();
    for (;;) {
        uint32_t counters[HEALTH_TASK_COUNT];
        app_get_progress(counters);
        if (!health_check(&health, counters, (uint32_t)xTaskGetTickCount())) {
            if (!failed) {
                app_fault(FAULT_TASK_STALL);
            }
            failed = true; /* No rearm or watchdog feed until reset. */
        }
        if (!failed) {
            board_watchdog_refresh();
        }
        (void)xTaskDelayUntil(&previous, pdMS_TO_TICKS(20U));
    }
}
