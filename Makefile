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
		syscall.s \
		stm32_p103.c \
		string.c \
		task.c   \
		kernel.c

SRCS= \
	$(CMSIS_SRCS) \
	$(STM32_SRCS) \
	$(RTENV_SRCS)

# Header files
HEADERS = \
		RTOSConfig.h     \
		stm32f10x_conf.h \
		stm32_p103.h     \
		string.h         \
		task.h           \
		syscall_def.h    \
		syscall.h

# Basic configurations
CFLAGS += -g3
CFLAGS += -Wall
CFLAGS += -DUSER_NAME=\"$(USER)\"
CFLAGS += -fno-common -ffreestanding -O0

ifeq ($(USE_ASM_OPTI_FUNC),YES)
	SRCS+=memcpy.s

	CFLAGS=-DUSE_ASM_OPTI_FUNC
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

main.bin: $(SRCS) $(HEADERS)
	$(CROSS_COMPILE)gcc -Wl,-Tmain.ld -nostartfiles \
		$(INCS) $(CFLAGS) $(SRCS) -o main.elf
	$(CROSS_COMPILE)objcopy -Obinary main.elf main.bin
	$(CROSS_COMPILE)objdump -S main.elf > main.list

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

clean:
	rm -f *.elf *.bin *.list gdb.txt
