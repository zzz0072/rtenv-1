qemu: $(OUT_DIR)/$(TARGET).bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-kernel $(OUT_DIR)/$(TARGET).bin -monitor null

qemudbg: $(OUT_DIR)/$(TARGET).bin $(QEMU_STM32)
	$(QEMU_STM32) -M stm32-p103 \
		-gdb tcp::3333 -S \
		-kernel $(OUT_DIR)/$(TARGET).bin -monitor null

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


