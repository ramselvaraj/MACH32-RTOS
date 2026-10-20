#include "format.h"

void text_init(text_t *text, char *buffer, size_t capacity)
{
    *text = (text_t){ .data = buffer, .capacity = capacity };
    if (capacity != 0U) {
        buffer[0] = '\0';
    }
}

static void character(text_t *text, char value)
{
    if (text->capacity == 0U || text->length + 1U >= text->capacity) {
        text->overflow = true;
        return;
    }
    text->data[text->length++] = value;
    text->data[text->length] = '\0';
}

void text_string(text_t *text, const char *value)
{
    while (*value != '\0') {
        character(text, *value++);
    }
}

void text_unsigned(text_t *text, uint32_t value)
{
    char digits[10];
    size_t count = 0U;
    do {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    while (count != 0U) {
        character(text, digits[--count]);
    }
}

void text_signed(text_t *text, int32_t value)
{
    if (value < 0) {
        character(text, '-');
        /* Unsigned arithmetic also handles INT32_MIN. */
        text_unsigned(text, 0U - (uint32_t)value);
    } else {
        text_unsigned(text, (uint32_t)value);
    }
}

void text_hex(text_t *text, uint32_t value)
{
    static const char digits[] = "0123456789abcdef";
    text_string(text, "0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        character(text, digits[(value >> shift) & 15U]);
    }
}
