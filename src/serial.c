#include "serial.h"

#include "app.h"
#include "board.h"
#include "protocol.h"
#include "queue.h"
#include "stm32f4xx.h"

#include <string.h>

#define RX_CAPACITY 256U
#define TX_CAPACITY 2048U
#define SERIAL_BATCH 64U

typedef struct {
    uint32_t received_ms;
    uint32_t generation;
    uint8_t byte;
} received_byte_t;
static StaticQueue_t rx_queue_storage;
static uint8_t rx_buffer[RX_CAPACITY * sizeof(received_byte_t)];
static QueueHandle_t rx_queue;
static uint8_t tx_buffer[TX_CAPACITY];
static volatile uint32_t tx_head, tx_tail;
static volatile bool rx_corrupt;
static bool status_requested;
static serial_stats_t stats;

void serial_init(void)
{
    rx_queue = xQueueCreateStatic(RX_CAPACITY, sizeof(received_byte_t),
                                  rx_buffer, &rx_queue_storage);
    configASSERT(rx_queue != NULL);
    NVIC_SetPriority(USART2_IRQn, 6U);
}

void serial_enable_interrupt(void)
{
    /* Called by the task, after the scheduler and all task handles exist. */
    NVIC_ClearPendingIRQ(USART2_IRQn);
    USART2->CR1 |= USART_CR1_RXNEIE | USART_CR1_PEIE;
    USART2->CR3 |= USART_CR3_EIE;
    NVIC_EnableIRQ(USART2_IRQn);
}

void USART2_IRQHandler(void)
{
    BaseType_t wake = pdFALSE;
    uint32_t flags = USART2->SR;
    const uint32_t errors = USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE;
    if ((flags & (USART_SR_RXNE | errors)) != 0U) {
        uint8_t byte = (uint8_t)USART2->DR; /* SR then DR clears error flags. */
        if ((flags & errors) != 0U) {
            ++stats.uart_errors;
            rx_corrupt = true;
            app_fault_from_isr(FAULT_RX_OVERFLOW, &wake);
        } else {
            const received_byte_t received = {
                .received_ms = (uint32_t)xTaskGetTickCountFromISR(),
                .generation = app_generation_from_isr(), .byte = byte
            };
            if (xQueueSendFromISR(rx_queue, &received, &wake) != pdPASS) {
                ++stats.rx_overflows;
                rx_corrupt = true;
                app_fault_from_isr(FAULT_RX_OVERFLOW, &wake);
            }
        }
    }
    if ((flags & USART_SR_TXE) != 0U && (USART2->CR1 & USART_CR1_TXEIE) != 0U) {
        if (tx_tail != tx_head) {
            USART2->DR = tx_buffer[tx_tail];
            tx_tail = (tx_tail + 1U) % TX_CAPACITY;
        } else {
            USART2->CR1 &= ~USART_CR1_TXEIE;
        }
    }
    portYIELD_FROM_ISR(wake);
}

void serial_send(const char *text)
{
    size_t length = strlen(text);
    taskENTER_CRITICAL();
    uint32_t free_bytes = (tx_tail + TX_CAPACITY - tx_head - 1U) % TX_CAPACITY;
    if (length > free_bytes) {
        ++stats.tx_drops; /* Drop the complete record, never a partial line. */
    } else {
        for (size_t index = 0U; index < length; ++index) {
            tx_buffer[tx_head] = (uint8_t)text[index];
            tx_head = (tx_head + 1U) % TX_CAPACITY;
        }
        USART2->CR1 |= USART_CR1_TXEIE;
    }
    taskEXIT_CRITICAL();
}

void serial_get_stats(serial_stats_t *snapshot)
{
    taskENTER_CRITICAL();
    *snapshot = stats;
    taskEXIT_CRITICAL();
}

void serial_request_status(void)
{
    taskENTER_CRITICAL();
    status_requested = true;
    taskEXIT_CRITICAL();
}

bool serial_take_status_request(void)
{
    taskENTER_CRITICAL();
    bool requested = status_requested;
    status_requested = false;
    taskEXIT_CRITICAL();
    return requested;
}

static void dispatch(const char *line, uint32_t received_ms, uint32_t generation)
{
    request_t request = protocol_parse(line, received_ms);
    request.command.generation = generation;
    switch (request.kind) {
        case REQUEST_STOP:
            app_stop();
            serial_send("OK STOP\r\n");
            break;
        case REQUEST_STATUS:
            serial_request_status();
            break;
        case REQUEST_COMMAND:
            serial_send(app_submit(&request.command)
                ? "QUEUED\r\n" : "ERROR BUSY\r\n");
            break;
        case REQUEST_STALL:
            app_request_stall(request.stall_task);
            serial_send("OK STALL\r\n");
            break;
        default:
            taskENTER_CRITICAL();
            ++stats.invalid_lines;
            taskEXIT_CRITICAL();
            serial_send("ERROR COMMAND\r\n");
            break;
    }
}

void serial_task(void *argument)
{
    (void)argument;
    protocol_stream_t stream = { 0 };
    serial_enable_interrupt();
    for (;;) {
        app_maybe_stall(TASK_SERIAL);
        for (unsigned count = 0U; count < SERIAL_BATCH; ++count) {
            taskENTER_CRITICAL();
            bool corrupt = rx_corrupt;
            if (corrupt) {
                /* Old delimiters must not end resynchronization before the
                 * byte that was lost. Flush everything before this point. */
                (void)xQueueReset(rx_queue);
            }
            rx_corrupt = false;
            taskEXIT_CRITICAL();
            if (corrupt) {
                protocol_discard(&stream);
            }
            received_byte_t received;
            if (xQueueReceive(rx_queue, &received, 0U) != pdPASS) {
                break;
            }
            line_result_t result = protocol_feed(&stream, received.byte);
            if (result == LINE_READY) {
                dispatch(stream.line, received.received_ms, received.generation);
            } else if (result == LINE_INVALID) {
                taskENTER_CRITICAL();
                ++stats.invalid_lines;
                taskEXIT_CRITICAL();
            }
        }
        app_progress(TASK_SERIAL);
        vTaskDelay(pdMS_TO_TICKS(2U));
    }
}
