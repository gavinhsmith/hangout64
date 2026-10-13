# Lua benchmark: PC build with the host compiler (the reference numbers).
# Consoles: Makefile.n64 and Makefile.nds, built in Docker via the root Makefile (make bench-n64).
CC ?= cc
PYTHON ?= python3
LUA32 := build/pc/lua32
include lua.mk
CFLAGS := -O2 -std=c99 -I.

all: luabench64 luabench32

bench_lua.h: bench.lua embed.py
	$(PYTHON) embed.py bench.lua bench_lua > $@

luabench64: bench.c main_pc.c $(addprefix $(LUA)/,$(LUA_SRC)) bench_lua.h
	$(CC) $(CFLAGS) -I$(LUA) -o $@ $(filter %.c,$^) -lm

luabench32: bench.c main_pc.c $(addprefix $(LUA32)/,$(LUA_SRC)) $(LUA32_HDR) bench_lua.h
	$(CC) $(CFLAGS) -I$(LUA32) -o $@ $(filter %.c,$^) -lm

clean:
	rm -rf build luabench64 luabench32 *.exe luabench*.z64 luabench*.nds luabench*.elf bench_lua.h

.PHONY: all clean
