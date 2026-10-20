#include "protocol.h"

#include <stddef.h>
#include <string.h>

void protocol_discard(protocol_stream_t *stream)
{
    stream->length = 0U;
    stream->dropping = true;
    stream->carriage_return = false;
}

line_result_t protocol_feed(protocol_stream_t *stream, uint8_t byte)
{
    if (byte == '\n') {
        bool ready = !stream->dropping && stream->length != 0U;
        stream->line[stream->length] = '\0';
        stream->length = 0U;
        stream->dropping = false;
        stream->carriage_return = false;
        return ready ? LINE_READY : LINE_PENDING;
    }
    if (stream->dropping) {
        return LINE_PENDING;
    }
    if (!stream->carriage_return && byte == '\r') {
        stream->carriage_return = true;
        return LINE_PENDING;
    }
    if (stream->carriage_return || byte < 32U || byte > 126U
        || stream->length + 1U >= sizeof(stream->line)) {
        protocol_discard(stream);
        return LINE_INVALID;
    }
    stream->line[stream->length++] = (char)byte;
    return LINE_PENDING;
}

request_t protocol_parse(const char *line, uint32_t received_ms)
{
    request_t request = { .kind = REQUEST_INVALID };
    request.command.received_ms = received_ms;
    if (line == NULL) {
        return request;
    }
    if (strcmp(line, "STOP") == 0) {
        request.kind = REQUEST_STOP;
    } else if (strcmp(line, "STATUS") == 0) {
        request.kind = REQUEST_STATUS;
    } else if (strcmp(line, "ARM SERIAL") == 0) {
        request.kind = REQUEST_COMMAND;
        request.command.kind = CMD_ARM_SERIAL;
    } else if (strcmp(line, "ARM POT") == 0) {
        request.kind = REQUEST_COMMAND;
        request.command.kind = CMD_ARM_POT;
    } else if (strcmp(line, "KEEPALIVE") == 0) {
        request.kind = REQUEST_COMMAND;
        request.command.kind = CMD_KEEPALIVE;
    } else if (strncmp(line, "SET ", 4U) == 0) {
        const char *cursor = line + 4;
        int sign = 1;
        unsigned value = 0U;
        unsigned digits = 0U;
        if (*cursor == '-') {
            sign = -1;
            ++cursor;
        }
        while (*cursor >= '0' && *cursor <= '9') {
            if (++digits > 3U) {
                return request;
            }
            value = value * 10U + (unsigned)(*cursor++ - '0');
        }
        if (digits != 0U && *cursor == '\0' && value <= 100U) {
            request.kind = REQUEST_COMMAND;
            request.command.kind = CMD_SET;
            request.command.value = (int16_t)(sign * (int)value);
        }
    }
#if ENABLE_TEST_HOOKS
    else if (strcmp(line, "STALL CONTROL") == 0) {
        request.kind = REQUEST_STALL;
        request.stall_task = 0U;
    } else if (strcmp(line, "STALL INPUT") == 0) {
        request.kind = REQUEST_STALL;
        request.stall_task = 1U;
    } else if (strcmp(line, "STALL SERIAL") == 0) {
        request.kind = REQUEST_STALL;
        request.stall_task = 2U;
    } else if (strcmp(line, "STALL DIAGNOSTICS") == 0) {
        request.kind = REQUEST_STALL;
        request.stall_task = 3U;
    }
#endif
    return request;
}
