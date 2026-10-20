#ifndef FORMAT_H
#define FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    bool overflow;
} text_t;

void text_init(text_t *text, char *buffer, size_t capacity);
void text_string(text_t *text, const char *value);
void text_unsigned(text_t *text, uint32_t value);
void text_signed(text_t *text, int32_t value);
void text_hex(text_t *text, uint32_t value);

#endif
