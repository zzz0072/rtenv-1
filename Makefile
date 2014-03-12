# Toolchain configuration
CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc

# TARGET settings
OUT_DIR=build
TARGET=rtenv

# QEMU PATH
QEMU_STM32 ?= ../qemu_stm32/arm-softmmu/qemu-system-arm

# Platform architecture
ARCH = CM3
VENDOR = ST
PLAT = STM32F10x

CFLAGS += -mcpu=cortex-m3 -mthumb

# Start up and BSP files
CMSIS_PLAT_SRC = $(CMSIS_LIB)/DeviceSupport/$(VENDOR)/$(PLAT)

CMSIS_SRCS = \
		$(CMSIS_LIB)/CoreSupport/core_cm3.c  \
		$(CMSIS_PLAT_SRC)/system_stm32f10x.c \
		$(CMSIS_PLAT_SRC)/startup/gcc_ride7/startup_stm32f10x_md.s

STM32_SRCS = \
		$(STM32_LIB)/src/stm32f10x_rcc.c   \
		$(STM32_LIB)/src/stm32f10x_gpio.c  \
		$(STM32_LIB)/src/stm32f10x_usart.c \
		$(STM32_LIB)/src/stm32f10x_exti.c  \
		$(STM32_LIB)/src/misc.c

# rtenv sources
RTENV_SRC = \
		context_switch.s \
		syscall.c        \
		stm32_p103.c     \
		rt_string.c      \
		task.c           \
		serial.c         \
		path_server.c    \
		shell.c          \
		malloc.c         \
		kernel.c         \
		unit_test.c

RTENV_SRCS = $(addprefix src/, $(RTENV_SRC))

SRCS= \
	$(CMSIS_SRCS) \
	$(STM32_SRCS) \
	$(RTENV_SRCS)

# Binary generation
C_OBJS = $(patsubst %.c, %.o, $(SRCS))   # translate *.c to *.o
OBJS   = $(patsubst %.s, %.o, $(C_OBJS)) # also *.s to *.o files

OUT_OBJS = $(addprefix $(OUT_DIR)/, $(OBJS))

DEPS   = ${OUT_OBJS:.o=.d}

# Basic configurations
DEBUG_FLAGS = -g3 $(UNIT_TEST)

CFLAGS += $(DEBUG_FLAGS)
CFLAGS += -Wall -std=c99 -MMD $(INCS)
CFLAGS += -DUSER_NAME=\"$(USER)\"
CFLAGS += -fno-common -ffreestanding -O0

ifeq ($(USE_ASM_OPTI_FUNC),YES)
	SRCS+=memcpy.s

	CFLAGS+=-DUSE_ASM_OPTI_FUNC
endif

# Include PATH
LIBDIR = .
CMSIS_LIB=$(LIBDIR)/libraries/CMSIS/$(ARCH)
STM32_LIB=$(LIBDIR)/libraries/STM32F10x_StdPeriph_Driver

INCS =  -Iinclude \
		-I$(LIBDIR)/libraries/CMSIS/CM3/CoreSupport \
		-I$(LIBDIR)/libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x \
		-I$(CMSIS_LIB)/CM3/DeviceSupport/ST/STM32F10x \
		-I$(LIBDIR)/libraries/STM32F10x_StdPeriph_Driver/inc \

#----------------------------------------------------------------------------------
$(OUT_DIR)/$(TARGET).bin: $(OUT_OBJS)
	$(CROSS_COMPILE)gcc -Wl,-Tsrc/$(TARGET).ld -nostartfiles \
		$(CFLAGS) $(OUT_OBJS) -o $(OUT_DIR)/$(TARGET).elf
	$(CROSS_COMPILE)objcopy -Obinary $(OUT_DIR)/$(TARGET).elf $@
	$(CROSS_COMPILE)objdump -S $(OUT_DIR)/$(TARGET).elf > $(OUT_DIR)/$(TARGET).list

$(OUT_DIR)/%.o: %.s
	mkdir -p $(dir $@)
	$(CROSS_COMPILE)gcc -c $(CFLAGS) $^ -o $@

$(OUT_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CROSS_COMPILE)gcc -c $(CFLAGS) $^ -o $@

qemu: $(OUT_DIR)/$(TARGET).bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-kernel $(OUT_DIR)/$(TARGET).bin -monitor null

qemudbg: $(OUT_DIR)/$(TARGET).bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-kernel $(OUT_DIR)/$(TARGET).bin

qemu_remote: $(OUT_DIR)/$(TARGET).bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 -kernel $(OUT_DIR)/$(TARGET).bin -vnc :1

qemudbg_remote: $(OUT_DIR)/$(TARGET).bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-kernel $(OUT_DIR)/$(TARGET).bin \
		-vnc :1

qemu_remote_bg: $(OUT_DIR)/$(TARGET).bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-kernel $(OUT_DIR)/$(TARGET).bin \
		-vnc :1 &

qemudbg_remote_bg: $(OUT_DIR)/$(TARGET).bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-kernel $(OUT_DIR)/$(TARGET).bin \
		-vnc :1 &

emu: $(OUT_DIR)/$(TARGET).bin
	bash emulate.sh $(OUT_DIR)/$(TARGET).bin

qemuauto: $(OUT_DIR)/$(TARGET).bin gdbscript
	bash emulate.sh $(OUT_DIR)/$(TARGET).bin &
	sleep 1
	$(CROSS_COMPILE)gdb -x gdbscript&
	sleep 5

qemuauto_remote: $(OUT_DIR)/$(TARGET).bin gdbscript
	bash emulate_remote.sh $(OUT_DIR)/$(TARGET).bin &
	sleep 1
	$(CROSS_COMPILE)gdb -x gdbscript&
	sleep 5

check: unit_test.c unit_test.h
	$(MAKE) $(OUT_DIR)/$(TARGET).bin UNIT_TEST=-DUNIT_TEST
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-serial stdio \
		-kernel $(OUT_DIR)/$(TARGET).bin -monitor null >/dev/null &
	# Dirty hack to force running tests every time
	make -C unit_tests clean
	make -C unit_tests CROSS_COMPILE=$(CROSS_COMPILE)
	@pkill -9 $(notdir $(QEMU_STM32))

clean:
	rm -fr gdb.txt $(OUT_DIR)
	make -C unit_tests clean

-include $(DEPS)
