TEST_FILES= test-strlen.in         \
			test-strcpy.in         \
			test-strcmp.in         \
			test-strncmp.in        \
			test-cmdtok.in         \
			test-hist_expand.in    \
			test-find_env_var.in   \
			test-env_var_expand.in \
			test-export_envvar.in

LOG_FILES=$(patsubst %.in, %.log, $(TEST_FILES))

all:$(LOG_FILES)

# test-itoa.in
%.log: %.in
	@echo
	$(CROSS_COMPILE)gdb -batch -x $^
	@grep -v Fail -q gdb.txt
	@mv -f gdb.txt $@

clean:
	rm -f *.log

