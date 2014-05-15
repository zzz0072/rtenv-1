# Toolchain configuration
CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
HOST-CC := gcc

# TARGET settings
TARGET=rtenv

OUT_DIR=build
TOOLS_DIR=tools
ROOTFS_DIR=rootfs

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
RTENV_SRCS = $(shell ls src/*.c src/*.s)

SRCS= \
	$(CMSIS_SRCS) \
	$(STM32_SRCS) \
	$(RTENV_SRCS)

ifeq ($(USE_ASM_OPTI_FUNC),YES)
	SRCS+=src/asm-opti/memcpy.s

	CFLAGS+=-DUSE_ASM_OPTI_FUNC
endif

# Binary generation
C_OBJS = $(patsubst %.c, %.o, $(SRCS))   # translate *.c to *.o
OBJS   = $(patsubst %.s, %.o, $(C_OBJS)) # also *.s to *.o files

OUT_OBJS = $(addprefix $(OUT_DIR)/, $(OBJS))

ROOTFS_OBJ = $(OUT_DIR)/rootfs.o

DEPS   = $(OUT_OBJS:.o=.d)

# Compilation flags
DEBUG_FLAGS = -g3 $(UNIT_TEST)

CFLAGS += $(DEBUG_FLAGS)
CFLAGS += -Wall -Werror -MMD $(INCS) -std=c99
CFLAGS += -DUSER_NAME=\"$(USER)\"
CFLAGS += -fno-common -ffreestanding -O0

# Include PATH
LIBDIR = .
CMSIS_LIB=$(LIBDIR)/libraries/CMSIS/$(ARCH)
STM32_LIB=$(LIBDIR)/libraries/STM32F10x_StdPeriph_Driver

INCS =  -Iinclude \
		-I$(LIBDIR)/libraries/CMSIS/CM3/CoreSupport \
		-I$(LIBDIR)/libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x \
		-I$(CMSIS_LIB)/CM3/DeviceSupport/ST/STM32F10x \
		-I$(LIBDIR)/libraries/STM32F10x_StdPeriph_Driver/inc

# Include build rules
MK_RULES=$(shell ls mk/*.mk)

#----------------------------------------------------------------------------------
$(OUT_DIR)/$(TARGET).bin: $(OUT_OBJS) $(ROOTFS_OBJ)
	$(CROSS_COMPILE)gcc -Wl,-Map=$(OUT_DIR)/$(TARGET).map,-Tsrc/$(TARGET).ld -nostartfiles \
		$(CFLAGS) $(OUT_OBJS) $(ROOTFS_OBJ) -o $(OUT_DIR)/$(TARGET).elf
	$(CROSS_COMPILE)objcopy -Obinary $(OUT_DIR)/$(TARGET).elf $@
	$(CROSS_COMPILE)objdump -S $(OUT_DIR)/$(TARGET).elf > $(OUT_DIR)/$(TARGET).list

$(OUT_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CROSS_COMPILE)gcc -c $(CFLAGS) $< -o $@

$(OUT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CROSS_COMPILE)gcc -c $(CFLAGS) $< -o $@

check: src/unit_test.c include/unit_test.h
	$(MAKE) $(OUT_DIR)/$(TARGET).bin UNIT_TEST=-DUNIT_TEST
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-serial stdio \
		-kernel $(OUT_DIR)/$(TARGET).bin -monitor null >/dev/null &
	- make unit_test
	@pkill -9 $(notdir $(QEMU_STM32))

clean:
	rm -fr $(OUT_DIR) gdb.txt

distclean: clean
	rm -fr $(ROOTFS_DIR)

-include $(DEPS)
include $(MK_RULES)
