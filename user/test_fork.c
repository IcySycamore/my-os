// user/test_fork.c — Minimal fork test
#include "user.h"

void test_fork_main(void)
{
    put("[TEST] parent start\n");
    int pid = sys_fork();
    if (pid < 0) {
        put("[TEST] fork failed\n");
    } else if (pid == 0) {
        put("[TEST] child running\n");
        sys_exit(0);
    } else {
        put("[TEST] parent pid="); putdec(pid); put("\n");
        int w = sys_wait();
        put("[TEST] waited pid="); putdec(w); put("\n");
    }
    put("[TEST] done\n\n");
    sys_exit(0);
}
