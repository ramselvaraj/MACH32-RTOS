#include "format.h"
#include "health.h"
#include "motor.h"
#include "protocol.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned cases;

static bool command(motor_t *motor, command_kind_t kind, int16_t value, uint32_t now)
{
    motor_command_t request = { .kind = kind, .value = value, .received_ms = now };
    return motor_command(motor, &request, now);
}

static void test_boot_and_enable(void)
{
    motor_t motor;
    motor_init(&motor);
    assert(motor.state == MOTOR_DISABLED && motor.output == 0);
    assert(!command(&motor, CMD_SET, 50, 0U));
    assert(!command(&motor, CMD_KEEPALIVE, 0, 0U));
    assert(command(&motor, CMD_ARM_SERIAL, 0, 0U));
    assert(motor.state == MOTOR_RUNNING && motor.output == 0);
    assert(!command(&motor, CMD_ARM_POT, 0, 1U));
    assert(command(&motor, CMD_SET, 50, 1U));
    motor_tick(&motor, 0U, 1U);
    assert(motor.output == 50);
    motor_stop(&motor, 2U);
    assert(motor.state == MOTOR_DISABLED && motor.output == 0);
    assert(!command(&motor, CMD_SET, 90, 3U));
    ++cases;
}

static void test_timeout_and_rearm(void)
{
    motor_t motor;
    motor_init(&motor);
    assert(command(&motor, CMD_ARM_SERIAL, 0, 100U));
    assert(command(&motor, CMD_SET, 30, 110U));
    motor_tick(&motor, 0U, 609U);
    assert(motor.output == 30);
    motor_tick(&motor, 0U, 610U);
    assert(motor.state == MOTOR_FAULT && motor.fault == FAULT_TIMEOUT);
    assert(motor.output == 0);
    assert(!command(&motor, CMD_KEEPALIVE, 0, 611U));
    motor_stop(&motor, 612U);
    assert(motor.state == MOTOR_FAULT && motor.fault == FAULT_TIMEOUT);
    assert(command(&motor, CMD_ARM_SERIAL, 0, 620U));
    assert(motor.state == MOTOR_RUNNING && motor.target == 0);
    assert(motor.fault == FAULT_NONE);
    ++cases;
}

static void test_keepalive_and_invalid_commands(void)
{
    motor_t motor;
    motor_init(&motor);
    assert(command(&motor, CMD_ARM_SERIAL, 0, 0U));
    assert(command(&motor, CMD_SET, 100, 10U));
    assert(!command(&motor, CMD_SET, 101, 100U));
    assert(!command(&motor, CMD_SET, -101, 100U));
    assert(!command(&motor, (command_kind_t)99, 0, 100U));
    assert(motor.last_command_ms == 10U);
    assert(command(&motor, CMD_KEEPALIVE, 0, 400U));
    motor_tick(&motor, 0U, 899U);
    assert(motor.state == MOTOR_RUNNING);
    motor_tick(&motor, 0U, 900U);
    assert(motor.state == MOTOR_FAULT);
    ++cases;
}

static void test_receive_age(void)
{
    motor_t motor;
    motor_init(&motor);
    motor_command_t arm = { .kind = CMD_ARM_SERIAL, .received_ms = 0U };
    assert(!motor_command(&motor, &arm, 500U));
    assert(motor.state == MOTOR_DISABLED);
    assert(command(&motor, CMD_ARM_SERIAL, 0, 1000U));
    motor_command_t set = { .kind = CMD_SET, .value = 20, .received_ms = 1010U };
    assert(motor_command(&motor, &set, 1100U));
    assert(motor.last_command_ms == 1010U); /* Queue delay cannot extend TTL. */
    motor_tick(&motor, 0U, 1510U);
    assert(motor.state == MOTOR_FAULT);
    assert(!command(&motor, CMD_SET, 20, 1511U));
    ++cases;
}

static void test_potentiometer(void)
{
    motor_t motor;
    motor_init(&motor);
    assert(command(&motor, CMD_ARM_POT, 0, 0U));
    motor_tick(&motor, 0U, 1000U);
    assert(motor.output == 0 && motor.state == MOTOR_RUNNING);
    motor_tick(&motor, 2048U, 1010U);
    assert(motor.output == 50);
    motor_tick(&motor, 4095U, 1020U);
    assert(motor.output == 100);
    motor_tick(&motor, UINT16_MAX, 1030U);
    assert(motor.output == 100);
    assert(!command(&motor, CMD_SET, -50, 1040U));
    assert(!command(&motor, CMD_KEEPALIVE, 0, 1040U));
    motor_fault(&motor, FAULT_ADC, 1050U);
    assert(motor.output == 0 && motor.state == MOTOR_FAULT);
    ++cases;
}

static void test_direction_change(void)
{
    motor_t motor;
    motor_init(&motor);
    assert(command(&motor, CMD_ARM_SERIAL, 0, 0U));
    assert(command(&motor, CMD_SET, 50, 1U));
    motor_tick(&motor, 0U, 1U);
    assert(command(&motor, CMD_SET, -40, 10U));
    motor_tick(&motor, 0U, 10U);
    assert(motor.output == 0 && motor.target == -40);
    motor_tick(&motor, 0U, 109U);
    assert(motor.output == 0);
    motor_tick(&motor, 0U, 110U);
    assert(motor.output == -40);
    assert(command(&motor, CMD_SET, 25, 120U));
    motor_tick(&motor, 0U, 120U);
    assert(motor.output == 0);
    motor_stop(&motor, 130U);
    assert(command(&motor, CMD_ARM_SERIAL, 0, 140U));
    assert(command(&motor, CMD_SET, 25, 140U));
    motor_tick(&motor, 0U, 140U);
    assert(motor.output == 0); /* STOP/ARM does not bypass reversal delay. */
    motor_tick(&motor, 0U, 220U);
    assert(motor.output == 25);
    ++cases;
}

static void test_time_wrap(void)
{
    motor_t motor;
    motor_init(&motor);
    uint32_t start = UINT32_MAX - 100U;
    assert(command(&motor, CMD_ARM_SERIAL, 0, start));
    assert(command(&motor, CMD_SET, 20, start));
    motor_tick(&motor, 0U, start + 499U);
    assert(motor.state == MOTOR_RUNNING && motor.output == 20);
    motor_tick(&motor, 0U, start + 500U);
    assert(motor.state == MOTOR_FAULT && motor.output == 0);
    ++cases;
}

static void test_protocol(void)
{
    request_t parsed = protocol_parse("ARM SERIAL", 123U);
    assert(parsed.kind == REQUEST_COMMAND && parsed.command.kind == CMD_ARM_SERIAL);
    assert(parsed.command.received_ms == 123U);
    assert(protocol_parse("ARM POT", 0U).command.kind == CMD_ARM_POT);
    assert(protocol_parse("KEEPALIVE", 0U).command.kind == CMD_KEEPALIVE);
    assert(protocol_parse("STOP", 0U).kind == REQUEST_STOP);
    assert(protocol_parse("STATUS", 0U).kind == REQUEST_STATUS);
    parsed = protocol_parse("SET -100", 1U);
    assert(parsed.kind == REQUEST_COMMAND && parsed.command.value == -100);
    parsed = protocol_parse("SET 100", 1U);
    assert(parsed.kind == REQUEST_COMMAND && parsed.command.value == 100);
    const char *invalid[] = {
        "", "stop", "STOP now", " ARM SERIAL", "ARM SERIAL ", "SET", "SET ",
        "SET -", "SET +1", "SET 101", "SET -101", "SET 1x", "SET  1", "SET 1 ",
        "SET 999999999999999999999999", "SET 0000", "SET 1.5", "ARM", "REBOOT"
    };
    for (size_t index = 0U; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        assert(protocol_parse(invalid[index], 0U).kind == REQUEST_INVALID);
    }
    assert(protocol_parse(NULL, 0U).kind == REQUEST_INVALID);
#if ENABLE_TEST_HOOKS
    assert(protocol_parse("STALL CONTROL", 0U).kind == REQUEST_STALL);
    assert(protocol_parse("STALL INPUT", 0U).stall_task == 1U);
    assert(protocol_parse("STALL SERIAL", 0U).stall_task == 2U);
    assert(protocol_parse("STALL DIAGNOSTICS", 0U).stall_task == 3U);
#else
    assert(protocol_parse("STALL CONTROL", 0U).kind == REQUEST_INVALID);
#endif
    ++cases;
}

static line_result_t feed(protocol_stream_t *stream, const char *text)
{
    line_result_t result = LINE_PENDING;
    while (*text != '\0') {
        result = protocol_feed(stream, (uint8_t)*text++);
    }
    return result;
}

static void test_stream(void)
{
    protocol_stream_t stream = { 0 };
    assert(feed(&stream, "ST") == LINE_PENDING);
    assert(feed(&stream, "OP\r\n") == LINE_READY);
    assert(strcmp(stream.line, "STOP") == 0);
    assert(feed(&stream, "\n") == LINE_PENDING);
    assert(feed(&stream, "ST\rOP\n") == LINE_PENDING);
    assert(feed(&stream, "STATUS\n") == LINE_READY);
    assert(strcmp(stream.line, "STATUS") == 0);
    for (unsigned index = 0U; index < PROTOCOL_LINE_SIZE - 1U; ++index) {
        assert(protocol_feed(&stream, 'X') == LINE_PENDING);
    }
    assert(protocol_feed(&stream, 'X') == LINE_INVALID);
    assert(feed(&stream, "STOP\n") == LINE_PENDING); /* Never use an overlong suffix. */
    assert(feed(&stream, "STOP\n") == LINE_READY);
    assert(protocol_feed(&stream, 0U) == LINE_INVALID);
    assert(feed(&stream, "ARM SERIAL\n") == LINE_PENDING);
    assert(feed(&stream, "ARM SERIAL\n") == LINE_READY);
    protocol_discard(&stream);
    assert(feed(&stream, "SET 100\n") == LINE_PENDING);
    assert(feed(&stream, "STOP\n") == LINE_READY);
    ++cases;
}

static void test_health(void)
{
    health_t health;
    uint32_t counters[HEALTH_TASK_COUNT] = { 0U };
    health_init(&health, 0U);
    assert(health_check(&health, counters, 299U));
    assert(!health_check(&health, counters, 300U));
    for (unsigned index = 0U; index < HEALTH_TASK_COUNT; ++index) {
        ++counters[index];
    }
    assert(health_check(&health, counters, 301U));
    ++counters[0];
    assert(!health_check(&health, counters, 601U));
    health_init(&health, UINT32_MAX - 100U);
    assert(health_check(&health, counters, UINT32_MAX - 100U));
    assert(health_check(&health, counters, 198U));
    assert(!health_check(&health, counters, 199U));
    ++cases;
}

static void test_formatter(void)
{
    char buffer[80];
    text_t text;
    text_init(&text, buffer, sizeof(buffer));
    text_unsigned(&text, UINT32_MAX);
    text_string(&text, " ");
    text_signed(&text, INT32_MIN);
    text_string(&text, " ");
    text_hex(&text, 0xA123U);
    assert(strcmp(buffer, "4294967295 -2147483648 0x0000a123") == 0);
    assert(!text.overflow);
    text_init(&text, buffer, 4U);
    text_string(&text, "ABCD");
    assert(text.overflow && strcmp(buffer, "ABC") == 0);
    text_init(&text, NULL, 0U);
    text_signed(&text, 0);
    assert(text.overflow);
    ++cases;
}

static void test_random_stream(void)
{
    protocol_stream_t stream = { 0 };
    uint32_t random = 42U;
    for (unsigned index = 0U; index < 100000U; ++index) {
        random = random * 1664525U + 1013904223U;
        if (protocol_feed(&stream, (uint8_t)(random >> 24U)) == LINE_READY) {
            (void)protocol_parse(stream.line, index);
        }
        assert(stream.length < PROTOCOL_LINE_SIZE);
    }
    ++cases;
}

int main(void)
{
    test_boot_and_enable();
    test_timeout_and_rearm();
    test_keepalive_and_invalid_commands();
    test_receive_age();
    test_potentiometer();
    test_direction_change();
    test_time_wrap();
    test_protocol();
    test_stream();
    test_health();
    test_formatter();
    test_random_stream();
    printf("PASS: %u C test groups (test hooks=%d)\n", cases, ENABLE_TEST_HOOKS);
    return 0;
}
