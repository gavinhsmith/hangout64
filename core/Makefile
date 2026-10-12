CC ?= cc

test: world.c guy_mesh.c test.c world.h
	$(CC) -std=c99 -Wall -Wextra -Werror -o test_core world.c guy_mesh.c test.c
	./test_core

.PHONY: test
