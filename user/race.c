// user/race.c — Forked racer A vs B (sleep 1 tick to yield to scheduler)
#include "user.h"

void sys_sleep(int ticks);

static void racer_body(const char *name, char marker)
{
    for (int lap = 0; lap < child_iters; lap++) {
        put("  ["); put(name); put("] lap ");
        putdec(lap + 1); put("/"); putdec(child_iters); put(" ");
        for (int i = 0; i < 30; i++) {
            char s[2] = { (i < lap * 30 / child_iters) ? marker : '.', 0 };
            put(s);
        }
        put("\n");
        sys_sleep(1);  // yield to scheduler → other racer runs
    }
}

void race_main(void)
{
    put("\n  ===== RACE! =====\n\n");
    int pid = sys_fork();
    if (pid < 0) {
        put("  fork failed\n");
        sys_exit(0);
    } else if (pid == 0) {
        racer_body("B", '#');
        put("  [B] Finished!\n");
        sys_exit(0);
    } else {
        racer_body("A", '*');
        put("  [A] Finished!\n");
        int w = sys_wait();
        put("\n  ===== RACE OVER ("); putdec(w); put(") =====\n\n");
        sys_exit(0);
    }
}
