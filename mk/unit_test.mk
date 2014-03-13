# Set tests
TEST_FILES=$(shell ls tests/test*.in)
LOG_FILES=$(patsubst %.in, %.log, $(TEST_FILES))
OUT_LOG_FILES=$(addprefix $(OUT_DIR)/, $(LOG_FILES))

unit_test: clean_up_tests $(OUT_LOG_FILES)

# test-itoa.in
$(OUT_DIR)/%.log: %.in
	mkdir -p $(dir $@)
	$(CROSS_COMPILE)gdb -batch -x $^
	@grep -v Fail -q gdb.txt
	@mv -f gdb.txt $@

clean_up_tests:
	rm -rf $(OUT_LOG_FILES)/tests/*
