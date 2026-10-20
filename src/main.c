#include "app.h"
#include "board.h"

int main(void)
{
    board_init();
    app_init();
    app_create_tasks();
    vTaskStartScheduler();
    app_panic(__FILE__, __LINE__);
}

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack,
                                 configSTACK_DEPTH_TYPE *depth)
{
    static StaticTask_t idle_tcb;
    static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}

void app_panic(const char *file, int line)
{
    /* Inspect these variables with SWD. Do not block on a serial fault report. */
    static const char *volatile panic_file;
    static volatile int panic_line;
    panic_file = file;
    panic_line = line;
    (void)panic_file;
    (void)panic_line;
    __asm volatile ("cpsid i" ::: "memory");
    board_motor_disable();
    for (;;) {
        __asm volatile ("nop"); /* The watchdog resets if it already started. */
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    app_panic("stack overflow", 0);
}

void HardFault_Handler(void) { app_panic("HardFault", 0); }
void MemManage_Handler(void) { app_panic("MemManage", 0); }
void BusFault_Handler(void) { app_panic("BusFault", 0); }
void UsageFault_Handler(void) { app_panic("UsageFault", 0); }
void NMI_Handler(void) { app_panic("NMI", 0); }
