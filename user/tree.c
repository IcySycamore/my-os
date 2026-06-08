// user/tree.c — Growing ASCII tree
#include "user.h"

void tree_main(void)
{
    put("\n");
    for (int h = 1; h <= child_iters; h++) {
        for (int row = 0; row < h; row++) {
            put("  ");
            for (int sp = 0; sp < child_iters - row; sp++) put(" ");
            for (int star = 0; star < 2 * row + 1; star++) put("*");
            put("\n");
        }
        put("  ");
        for (int sp = 0; sp < child_iters; sp++) put(" ");
        put("|||\n\n");
        put("  [Tree height="); putdec(h); put("]\n\n");
        for (volatile int d = 0; d < 4000000; d++);
        if (h < child_iters) {
            for (int cl = 0; cl < 20; cl++) put("\n");
        }
    }
    sys_exit(0);
}
