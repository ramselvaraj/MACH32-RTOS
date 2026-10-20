#include "app.h"
#include "board.h"

void inputs_task(void *argument)
{
    (void)argument;
    TickType_t previous = xTaskGetTickCount();
    for (;;) {
        app_maybe_stall(TASK_INPUT);
        uint16_t sample = 0U;
        bool valid = board_adc_read(&sample);
        app_set_input(sample, valid);
        app_progress(TASK_INPUT);
        (void)xTaskDelayUntil(&previous, pdMS_TO_TICKS(10U));
    }
}
