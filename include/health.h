#ifndef HEALTH_H
#define HEALTH_H

#include <stdbool.h>
#include <stdint.h>

#define HEALTH_TASK_COUNT 4U
#define HEALTH_TIMEOUT_MS 300U

typedef struct {
    uint32_t counters[HEALTH_TASK_COUNT];
    uint32_t changed_ms[HEALTH_TASK_COUNT];
} health_t;

void health_init(health_t *health, uint32_t now_ms);
bool health_check(health_t *health, const uint32_t *counters, uint32_t now_ms);

#endif
