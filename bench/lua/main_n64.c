#include "bench.h"
#include <libdragon.h>
#include <stdio.h>

static unsigned long now(void) { return (unsigned long)get_ticks(); }

static void out(const char *line)
{
    printf("%s\n", line);
    console_render();
}

int main(void)
{
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    bench_run(&(bench_env){"N64 (93.75 MHz VR4300)", now, TICKS_PER_SECOND / 1000, out});
    out("done");
    for (;;)
        ;
}
