# Toolchain configuration
CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc

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
		$(CMSIS_LIB)/CoreSupport/core_cm3.c \
		$(CMSIS_PLAT_SRC)/system_stm32f10x.c \
		$(CMSIS_PLAT_SRC)/startup/gcc_ride7/startup_stm32f10x_md.s

STM32_SRCS = \
		$(STM32_LIB)/src/stm32f10x_rcc.c \
		$(STM32_LIB)/src/stm32f10x_gpio.c \
		$(STM32_LIB)/src/stm32f10x_usart.c \
		$(STM32_LIB)/src/stm32f10x_exti.c \
		$(STM32_LIB)/src/misc.c

# rtenv sources
RTENV_SRCS = \
		context_switch.s \
		syscall.c \
		stm32_p103.c \
		rt_string.c \
		task.c   \
		serial.c \
		path_server.c \
		shell.c  \
		malloc.c \
		kernel.c \
		unit_test.c

SRCS= \
	$(CMSIS_SRCS) \
	$(STM32_SRCS) \
	$(RTENV_SRCS)

OBJS_1 = $(patsubst %.c, %.o, $(SRCS))
OBJS   = $(patsubst %.s, %.o, $(OBJS_1))
DEPS   = ${OBJS:.o=.d}

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

INCS =  -I . \
		-I$(LIBDIR)/libraries/CMSIS/CM3/CoreSupport \
		-I$(LIBDIR)/libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x \
		-I$(CMSIS_LIB)/CM3/DeviceSupport/ST/STM32F10x \
		-I$(LIBDIR)/libraries/STM32F10x_StdPeriph_Driver/inc \

#----------------------------------------------------------------------------------
all: main.bin

main.bin: $(OBJS)
	$(CROSS_COMPILE)gcc -Wl,-Tmain.ld -nostartfiles \
		$(CFLAGS) $(OBJS) -o main.elf
	$(CROSS_COMPILE)objcopy -Obinary main.elf main.bin
	$(CROSS_COMPILE)objdump -S main.elf > main.list

%.o:%.s
	$(CROSS_COMPILE)gcc -c $(CFLAGS) $^ -o $@

qemu: main.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-kernel main.bin -monitor null

qemudbg: main.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-kernel main.bin


qemu_remote: main.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 -kernel main.bin -vnc :1

qemudbg_remote: main.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-kernel main.bin \
		-vnc :1

qemu_remote_bg: main.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-kernel main.bin \
		-vnc :1 &

qemudbg_remote_bg: main.bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-kernel main.bin \
		-vnc :1 &

emu: main.bin
	bash emulate.sh main.bin

qemuauto: main.bin gdbscript
	bash emulate.sh main.bin &
	sleep 1
	$(CROSS_COMPILE)gdb -x gdbscript&
	sleep 5

qemuauto_remote: main.bin gdbscript
	bash emulate_remote.sh main.bin &
	sleep 1
	$(CROSS_COMPILE)gdb -x gdbscript&
	sleep 5



check: unit_test.c unit_test.h
	$(MAKE) main.bin UNIT_TEST=-DUNIT_TEST
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-serial stdio \
		-kernel main.bin -monitor null >/dev/null &
	# Dirty hack to force running tests every time
	make -C unit_tests clean
	make -C unit_tests CROSS_COMPILE=$(CROSS_COMPILE)
	@pkill -9 $(notdir $(QEMU_STM32))

clean:
	rm -f *.elf *.bin *.list gdb.txt $(OBJS) $(DEPS)
	make -C unit_tests clean

-include $(DEPS)
