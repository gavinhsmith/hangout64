#include "bench.h"
#include "bench_lua.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include <stdio.h>
#include <stdlib.h>

#define TICKS 30      /* one second of 30 Hz ticks per measurement */
#define HOOK_EVERY 100 /* count VM instructions in steps of this many */

static const bench_env *env;
static size_t mem_now, mem_peak;
static unsigned long hooks;

/* Lua's allocator, counting the bytes it holds. */
static void *alloc(void *ud, void *p, size_t osize, size_t nsize)
{
    (void)ud;
    if (p == NULL)
        osize = 0;
    if (nsize == 0) {
        free(p);
        mem_now -= osize;
        return NULL;
    }
    void *q = realloc(p, nsize);
    if (q != NULL) {
        mem_now = mem_now - osize + nsize;
        if (mem_now > mem_peak)
            mem_peak = mem_now;
    }
    return q;
}

static void count(lua_State *L, lua_Debug *ar)
{
    (void)L, (void)ar;
    hooks++;
}

/* Microseconds since t0, times `scale` (10 for tenths). */
static unsigned long us_since(unsigned long t0, unsigned scale)
{
    return (unsigned long)((unsigned long long)(env->now() - t0) * 1000 * scale / env->per_ms);
}

static void say(const char *fmt, unsigned long a, unsigned long b, unsigned long c)
{
    char line[64];
    snprintf(line, sizeof line, fmt, a, b, c);
    env->out(line);
}

/* Call the function at index fn `times` times; 0 on success. */
static int call(lua_State *L, int fn, int times)
{
    while (times--) {
        lua_pushvalue(L, fn);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            env->out(lua_tostring(L, -1));
            return 1;
        }
    }
    return 0;
}

void bench_run(const bench_env *e)
{
    static const luaL_Reg libs[] = {
        {LUA_GNAME, luaopen_base},         {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},  {LUA_MATHLIBNAME, luaopen_math},
        {LUA_COLIBNAME, luaopen_coroutine}, {LUA_UTF8LIBNAME, luaopen_utf8},
    };
    static const char *const work[] = {"movers", "overlaps", "events", "strings", "alloc", "mix"};
    env = e;
    env->out(env->name);
    say("Lua 5.4: %lu-bit int, %lu-bit float", sizeof(lua_Integer) * 8, sizeof(lua_Number) * 8, 0);

    unsigned long t = env->now();
    lua_State *L = lua_newstate(alloc, NULL);
    for (unsigned i = 0; i < sizeof libs / sizeof libs[0]; i++) {
        luaL_requiref(L, libs[i].name, libs[i].func, 1);
        lua_pop(L, 1);
    }
    say("new state: %lu KB, %lu us", (mem_now + 1023) / 1024, us_since(t, 1), 0);

    size_t before = mem_now;
    t = env->now();
    if (luaL_loadbuffer(L, bench_lua, sizeof bench_lua - 1, "bench") != LUA_OK) {
        env->out(lua_tostring(L, -1));
        return;
    }
    say("compile %lu B: %lu us, +%lu KB", sizeof bench_lua - 1, us_since(t, 1), (mem_now - before + 1023) / 1024);
    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
        env->out(lua_tostring(L, -1));
        return;
    }
    say("after setup: %lu KB", (mem_now + 1023) / 1024, 0, 0);

    env->out("work     us/tk instr  i/ms  %30Hz");
    int table = lua_gettop(L);
    for (unsigned i = 0; i < sizeof work / sizeof work[0]; i++) {
        lua_getfield(L, table, work[i]);
        int fn = lua_gettop(L);
        hooks = 0;
        lua_sethook(L, count, LUA_MASKCOUNT, HOOK_EVERY);
        if (call(L, fn, TICKS))
            return;
        lua_sethook(L, NULL, 0, 0);
        unsigned long instr = hooks * HOOK_EVERY / TICKS;

        t = env->now();
        if (call(L, fn, TICKS))
            return;
        unsigned long us10 = us_since(t, 10) / TICKS; /* tenths of a microsecond per tick */
        unsigned long per_ms = us10 ? instr * 10000 / us10 : 0;
        unsigned long share = us10 / 333; /* of a 30 Hz tick (33333 us), in tenths of a percent */
        char line[64];
        snprintf(line, sizeof line, "%-8s%5lu.%lu %6lu %5lu %3lu.%lu", work[i], us10 / 10, us10 % 10, instr, per_ms,
                 share / 10, share % 10);
        env->out(line);
        lua_pop(L, 1);
    }

    lua_gc(L, LUA_GCCOLLECT);
    say("peak %lu KB, after GC %lu KB", (mem_peak + 1023) / 1024, (mem_now + 1023) / 1024, 0);
    lua_close(L);
}
