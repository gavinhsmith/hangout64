#include "bench.h"
#include <nds.h>
#include <stdio.h>

static unsigned long now(void) { return cpuGetTiming(); }

static void out(const char *line) { printf("%s\n", line); }

int main(void)
{
    consoleDemoInit();
    cpuStartTiming(0);
    bench_run(&(bench_env){"DS (67 MHz ARM946E-S)", now, BUS_CLOCK / 1000, out});
    out("done");
    for (;;)
        swiWaitForVBlank();
}
