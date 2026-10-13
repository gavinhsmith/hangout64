#include "bench.h"
#include <stdio.h>
#include <time.h>

static unsigned long now(void)
{
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (unsigned long)(t.tv_sec * 1000000UL + t.tv_nsec / 1000);
}

static void out(const char *line) { puts(line); }

int main(void)
{
    bench_run(&(bench_env){"PC", now, 1000, out});
    return 0;
}
