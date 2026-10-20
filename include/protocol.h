#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "motor.h"
#include <stddef.h>

#ifndef ENABLE_TEST_HOOKS
#define ENABLE_TEST_HOOKS 0
#endif
#if ENABLE_TEST_HOOKS != 0 && ENABLE_TEST_HOOKS != 1
#error ENABLE_TEST_HOOKS must be zero or one
#endif

#define PROTOCOL_LINE_SIZE 48U
typedef enum {
    REQUEST_INVALID, REQUEST_COMMAND, REQUEST_STOP, REQUEST_STATUS,
    REQUEST_STALL
} request_kind_t;

typedef struct {
    request_kind_t kind;
    motor_command_t command;
    unsigned stall_task;
} request_t;

typedef enum { LINE_PENDING, LINE_READY, LINE_INVALID } line_result_t;
typedef struct {
    char line[PROTOCOL_LINE_SIZE];
    size_t length;
    bool dropping;
    bool carriage_return;
} protocol_stream_t;

/* Exact uppercase ASCII commands. No allocation and no partial numeric parsing. */
request_t protocol_parse(const char *line, uint32_t received_ms);
line_result_t protocol_feed(protocol_stream_t *stream, uint8_t byte);
void protocol_discard(protocol_stream_t *stream);

#endif
