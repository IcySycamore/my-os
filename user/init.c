// user/init.c — xv6-style init: fork + exec shell, wait, restart on exit
#include "user.h"

// Task creation
int exec(void (*func)(void));

// Shell entry point (in user/shell.c)
extern void shell_main(void);

void init_main(void)
{
    for (;;) {
        int sh_pid = sys_fork();
        if (sh_pid < 0) {
            put("[init] fork failed, retry...\n");
            for (volatile int d = 0; d < 500000; d++);
            continue;
        }
        if (sh_pid == 0) {
            // xv6-style: exec into shell
            exec(shell_main);
            sys_exit(0);
        }
        // Parent: wait for ALL children to exit
        for (;;) {
            int wpid = sys_wait();
            if (wpid < 0) break;
        }
        put("[init] Shell exited, restarting...\n\n");
    }
}
