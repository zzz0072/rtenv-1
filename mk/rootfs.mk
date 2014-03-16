# source/taget
MKFS_SRC=$(TOOLS_DIR)/mkrootfs.c
MKFS_HOST=$(OUT_DIR)/$(TOOLS_DIR)/mkrootfs
ROOTFS_IMAGE=$(OUT_DIR)/rootfs.img

# flags
MKFS_FLAGS=-g -Wall -Werror -I./include -DBUILD_HOST

$(ROOTFS_OBJ): $(ROOTFS_IMAGE)
	@$(CROSS_COMPILE)objcopy -I binary -O elf32-littlearm -B arm \
		--prefix-sections '.rom' $< $@

$(ROOTFS_IMAGE): $(MKFS_HOST) rootfs_dir
	$(MKFS_HOST) -d $(ROOTFS_DIR) -o $@

rootfs_dir:
	@test -d $(ROOTFS_DIR) || \
		(echo "mkdir $(ROOTFS_DIR). You can store to $(ROOTFS_DIR) here." ;  mkdir $(ROOTFS_DIR))

# host tool to pack files/directory to a binary file
$(MKFS_HOST): $(MKFS_SRC)
	@mkdir -p $(dir $@)
	$(HOST-CC) $(MKFS_FLAGS) -o $@ $^
