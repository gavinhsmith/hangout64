# Lua from the third_party/lua submodule (github.com/lua/lua, tag v5.4.9).
#
# luaconf.h hard-codes LUA_32BITS to 0, and Lua's own files find it before anything on the
# include path, so a 32-bit build compiles a copy of the sources (in $(LUA32)) whose luaconf.h
# has it switched on. The submodule itself stays unmodified. Set LUA32 before including this.
# Keep these rules from becoming the default target.
LUA_MK_GOAL := $(.DEFAULT_GOAL)
LUA      := ../../third_party/lua
LUA_SRC  := $(filter-out lua.c luac.c onelua.c ltests.c,$(notdir $(wildcard $(LUA)/*.c)))
LUA_HDR  := $(notdir $(wildcard $(LUA)/*.h))
LUA32_HDR = $(addprefix $(LUA32)/,$(LUA_HDR))

ifeq ($(LUA_SRC),)
$(error third_party/lua is empty: run git submodule update --init)
endif

$(LUA32)/luaconf.h: $(LUA)/luaconf.h
	@mkdir -p $(dir $@)
	sed 's/^\(#define LUA_32BITS[[:space:]]*\)0/\11/' $< > $@
	grep -q '^#define LUA_32BITS[[:space:]]*1' $@

$(LUA32)/%.h: $(LUA)/%.h
	@mkdir -p $(dir $@)
	cp $< $@

$(LUA32)/%.c: $(LUA)/%.c
	@mkdir -p $(dir $@)
	cp $< $@

.DEFAULT_GOAL := $(LUA_MK_GOAL)
