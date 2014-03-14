# source/taget
MKFS_SRC=$(TOOLS_DIR)/mkrootfs.c
MKFS_TARGET=$(OUT_DIR)/$(TOOLS_DIR)/mkrootfs

# flags
MKFS_FLAGS=-g -Wall -Werror

mkrootfs: $(MKFS_TARGET)

$(MKFS_TARGET): $(MKFS_SRC)
	@mkdir -p $(dir $@)
	$(HOST-CC) $(MKFS_FLAGS) -o $@ $^
