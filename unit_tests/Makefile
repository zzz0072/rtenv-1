all:
	@echo
	$(CROSS_COMPILE)gdb -batch -x test-strlen.in
	@mv -f gdb.txt test-strlen.txt
	@echo
	$(CROSS_COMPILE)gdb -batch -x test-strcpy.in
	@mv -f gdb.txt test-strcpy.txt
	@echo
	$(CROSS_COMPILE)gdb -batch -x test-strcmp.in
	@mv -f gdb.txt test-strcmp.txt
	@echo
	$(CROSS_COMPILE)gdb -batch -x test-strncmp.in
	@mv -f gdb.txt test-strncmp.txt
	@echo
	$(CROSS_COMPILE)gdb -batch -x test-cmdtok.in
	@mv -f gdb.txt test-cmdtok.txt
	# FIXME: Rewrite itoa testcase due to use macro and prototypeing
	# is different than original API
	#@echo
	#$(CROSS_COMPILE)gdb -batch -x test-itoa.in
	#@mv -f gdb.txt test-itoa.txt
	@echo
	$(CROSS_COMPILE)gdb -batch -x test-hist_expand.in
	@mv -f gdb.txt test-hist_expand.txt
	@echo
	$(CROSS_COMPILE)gdb -batch -x test-find_env_var.in
	@mv -f gdb.txt test-find_env_var.txt
	@echo
	$(CROSS_COMPILE)gdb -batch -x test-env_var_expand.in
	@mv -f gdb.txt test-env_var_expand.txt
	@echo
	$(CROSS_COMPILE)gdb -batch -x test-export_envvar.in
	@mv -f gdb.txt test-export_envvar.txt
	@echo


clean:
	rm -f *.txt
