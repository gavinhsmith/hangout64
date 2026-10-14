CC ?= cc
ROOT := ..
include core.mk

test: $(CORE_SRC) $(addprefix $(GEN)/,$(GEN_SRC)) test.c world.h
	$(CC) -std=c99 -Wall -Wextra -Werror -I. -o test_core $(CORE_SRC) $(addprefix $(GEN)/,$(GEN_SRC)) test.c
	./test_core

.PHONY: test
