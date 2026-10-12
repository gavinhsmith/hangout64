CC ?= cc

test: world.c place.c guy_mesh.c default_place.c test.c world.h
	$(CC) -std=c99 -Wall -Wextra -Werror -o test_core world.c place.c guy_mesh.c default_place.c test.c
	./test_core

.PHONY: test
