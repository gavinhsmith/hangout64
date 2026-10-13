# Lua 5.4.9

`src/` of the official release, without the standalone programs (`lua.c`, `luac.c`) and the
Makefile. One change: `luaconf.h` only defines `LUA_32BITS` if it isn't already defined, so a
build can pass `-DLUA_32BITS=1` (marked "workbench64").

- Source: https://www.lua.org/ftp/lua-5.4.9.tar.gz
- SHA-256: `2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6`
- License: MIT, Copyright (C) 1994-2026 Lua.org, PUC-Rio (full text at the end of `lua.h`)
