CC ?= cc

test: cube.c test.c cube.h
	$(CC) -std=c99 -Wall -Wextra -Werror -o test_core cube.c test.c
	./test_core

.PHONY: test
