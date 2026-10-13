# Lua benchmark: PC build with the host compiler (the reference numbers).
# Consoles: Makefile.n64 and Makefile.nds, built in Docker via the root Makefile (make bench-n64).
CC ?= cc
PYTHON ?= python3
LUA := ../../third_party/lua
SRC := bench.c main_pc.c $(filter-out $(LUA)/lua.c $(LUA)/luac.c,$(wildcard $(LUA)/*.c))
CFLAGS := -O2 -std=c99 -I$(LUA) -I.

all: luabench64 luabench32

bench_lua.h: bench.lua embed.py
	$(PYTHON) embed.py bench.lua bench_lua > $@

luabench64: $(SRC) bench_lua.h
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm

luabench32: $(SRC) bench_lua.h
	$(CC) $(CFLAGS) -DLUA_32BITS=1 -o $@ $(SRC) -lm

clean:
	rm -rf build luabench64 luabench32 *.exe *.z64 *.nds *.elf bench_lua.h

.PHONY: all clean
