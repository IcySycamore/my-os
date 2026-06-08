// user/worm.c — Animated progress worm (single-line with \r)
#include "user.h"

void worm_main(void)
{
    int width = 40;
    put("\n  Worm crawl:\n  |");
    for (int i = 0; i < width; i++) put("-");
    put("|\n");

    for (int step = 0; step < child_iters; step++) {
        char line[64];
        int pos = (step * width) / child_iters;
        for (int i = 0; i < width; i++) line[i] = ' ';
        line[pos] = '@';
        line[width] = 0;

        put("  \r  "); put(line);
        sys_sleep(1);  // 1 tick delay
    }
    put("\n  Done!\n\n");
    sys_exit(0);
}
