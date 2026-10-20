#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t invalid_lines;
    uint32_t rx_overflows;
    uint32_t uart_errors;
    uint32_t tx_drops;
} serial_stats_t;

void serial_init(void);
void serial_enable_interrupt(void);
void serial_send(const char *text);
void serial_get_stats(serial_stats_t *stats);
void serial_request_status(void);
bool serial_take_status_request(void);

#endif
