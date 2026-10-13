#ifndef BENCH_H
#define BENCH_H

/* What each platform gives the benchmark: a free-running timer and a way to print a line. */
typedef struct {
    const char *name;
    unsigned long (*now)(void); /* timer ticks; may wrap */
    unsigned long per_ms;       /* timer ticks per millisecond */
    void (*out)(const char *line);
} bench_env;

/* Measure Lua: state size, compile time, and per-tick cost of game-like workloads. */
void bench_run(const bench_env *env);

#endif
