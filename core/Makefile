CC ?= cc
include core.mk

test: $(CORE_SRC) test.c world.h
	$(CC) -std=c99 -Wall -Wextra -Werror -o test_core $(CORE_SRC) test.c
	./test_core

.PHONY: test
