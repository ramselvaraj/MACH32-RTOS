# MACH32-RTOS: no generated code, heap, or vendor source modifications.
PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE = $(PREFIX)size
HOST_CC ?= cc
PYTHON ?= python3

MOTOR_OUTPUT_ENABLED ?= 0
MOTOR_MAX_DUTY_PERCENT ?= 25
ENABLE_TEST_HOOKS ?= 0
BUILD := build/output$(MOTOR_OUTPUT_ENABLED)-limit$(MOTOR_MAX_DUTY_PERCENT)-hooks$(ENABLE_TEST_HOOKS)
TARGET := $(BUILD)/firmware
MACH := vendor/MACH32
KERNEL := vendor/FreeRTOS-Kernel
PORT := $(KERNEL)/portable/GCC/ARM_CM4F

CPU := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS := -DSTM32F446xx -DMOTOR_OUTPUT_ENABLED=$(MOTOR_OUTPUT_ENABLED) \
        -DMOTOR_MAX_DUTY_PERCENT=$(MOTOR_MAX_DUTY_PERCENT) \
        -DENABLE_TEST_HOOKS=$(ENABLE_TEST_HOOKS)
INCLUDES := -Iinclude -Iconfig -I$(MACH)/Drivers/inc -I$(MACH)/Drivers/cmsis \
            -I$(KERNEL)/include -I$(PORT)
COMMON := -std=c11 -Wall -Wextra -Werror -g3 -Og -ffunction-sections -fdata-sections
CFLAGS := $(CPU) $(COMMON) $(DEFS) $(INCLUDES) -MMD -MP
ASFLAGS := $(CPU) -g3 -DSTM32F446xx
LDFLAGS := $(CPU) -T$(MACH)/linker/stm32f446re.ld -nostartfiles \
           --specs=nano.specs --specs=nosys.specs \
           -Wl,--gc-sections,-Map=$(TARGET).map,--cref \
           -Wl,--defsym=MACH32_FLASH_ORIGIN=0x08000000 \
           -Wl,--defsym=MACH32_FLASH_LENGTH=0x00080000

APP_SRC := $(wildcard src/*.c)
# Explicit list: never link MACH32's competing SysTick_Handler or busy-wait UART API usage.
DRIVER_SRC := $(addprefix $(MACH)/Drivers/src/,rcc.c gpio.c timer.c adc.c usart.c iwdg.c)
KERNEL_SRC := $(addprefix $(KERNEL)/,tasks.c queue.c list.c) $(PORT)/port.c
SOURCES := $(APP_SRC) $(DRIVER_SRC) $(MACH)/Drivers/cmsis/system_stm32f4xx.c $(KERNEL_SRC)
STARTUP := $(MACH)/startup/startup_stm32f446xx.s
OBJECTS := $(addprefix $(BUILD)/,$(SOURCES:.c=.o)) $(BUILD)/$(STARTUP:.s=.o)
HOST_SOURCES := src/motor.c src/protocol.c src/health.c src/format.c tests/test_motor.c
HOST_FLAGS := -std=c11 -Wall -Wextra -Werror -Wpedantic -g -Iinclude

.PHONY: all deps check-deps test test-sanitize check-python verify flash size clean
.DEFAULT_GOAL := all

all: check-deps $(TARGET).elf $(TARGET).bin $(TARGET).hex
	$(SIZE) $(TARGET).elf

deps:
	git submodule update --init --recursive

check-deps:
	@test -f $(KERNEL)/include/FreeRTOS.h -a -f $(MACH)/Drivers/inc/mach32.h || \
	    { echo 'Dependencies missing. Run make deps.'; exit 1; }

$(BUILD)/%.o: %.c Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s Makefile
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJECTS) $(MACH)/linker/stm32f446re.ld
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

size: $(TARGET).elf
	$(SIZE) $<

test:
	@mkdir -p build/tests
	$(HOST_CC) $(HOST_FLAGS) -DENABLE_TEST_HOOKS=0 $(HOST_SOURCES) -o build/tests/test_motor
	./build/tests/test_motor
	$(HOST_CC) $(HOST_FLAGS) -DENABLE_TEST_HOOKS=1 $(HOST_SOURCES) -o build/tests/test_motor_hooks
	./build/tests/test_motor_hooks
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py' -v

test-sanitize:
	@mkdir -p build/tests
	$(HOST_CC) $(HOST_FLAGS) -DENABLE_TEST_HOOKS=1 -fsanitize=address,undefined \
	    -fno-omit-frame-pointer $(HOST_SOURCES) -o build/tests/test_motor_sanitize
	./build/tests/test_motor_sanitize

check-python:
	$(PYTHON) -m py_compile tools/test_motor.py tools/check_firmware.py tests/test_bench_tool.py

verify: all
	$(PYTHON) tools/check_firmware.py --nm $(PREFIX)nm $(TARGET).elf $(TARGET).bin

# Flashing is always an explicit user action. This erases any existing bootloader.
flash: all
	st-flash --connect-under-reset --reset write $(TARGET).bin 0x08000000

clean:
	$(PYTHON) -c "import shutil; shutil.rmtree('build', ignore_errors=True)"

-include $(OBJECTS:.o=.d)
