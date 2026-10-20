#include "health.h"

#include <stddef.h>

void health_init(health_t *health, uint32_t now_ms)
{
    for (size_t index = 0U; index < HEALTH_TASK_COUNT; ++index) {
        health->counters[index] = 0U;
        health->changed_ms[index] = now_ms;
    }
}

bool health_check(health_t *health, const uint32_t *counters, uint32_t now_ms)
{
    bool healthy = true;
    for (size_t index = 0U; index < HEALTH_TASK_COUNT; ++index) {
        if (counters[index] != health->counters[index]) {
            health->counters[index] = counters[index];
            health->changed_ms[index] = now_ms;
        } else if ((uint32_t)(now_ms - health->changed_ms[index]) >= HEALTH_TIMEOUT_MS) {
            healthy = false;
        }
    }
    return healthy;
}
