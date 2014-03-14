# source/taget
MKFS_SRC=$(TOOLS_DIR)/mkrootfs.c
MKFS_HOST=$(OUT_DIR)/$(TOOLS_DIR)/mkrootfs

# flags
MKFS_FLAGS=-g -Wall -Werror

mkrootfs: $(MKFS_HOST)

# host tool to pack files/directory to a binary file
$(MKFS_HOST): $(MKFS_SRC)
	@mkdir -p $(dir $@)
	$(HOST-CC) $(MKFS_FLAGS) -o $@ $^
