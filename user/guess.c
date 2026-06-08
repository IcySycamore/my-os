// user/guess.c — Number guessing game
#include "user.h"

void guess_main(void)
{
    int secret = (sys_uptime() % 100) + 1;
    int attempts = 0;
    char buf[16];

    put("\n  ===== Guess the Number (1-100) =====\n");
    put("  I'm thinking of a number...\n\n");

    while (1) {
        put("  Your guess> ");
        int n = sys_read(0, buf, sizeof(buf) - 1);
        if (n <= 0) continue;
        buf[n] = 0;

        int len = 0;
        for (int i = 0; i < n && buf[i] != '\n' && buf[i] != '\r'; i++) len = i + 1;
        int guess = atoi2(buf, len);
        if (guess == 0) { put("  Please enter a number\n"); continue; }

        attempts++;
        if (guess < secret)       put("  Too LOW!  Try higher.\n");
        else if (guess > secret)  put("  Too HIGH! Try lower.\n");
        else {
            put("  *** CORRECT! ***  You got it in ");
            putdec(attempts);
            put(" tries!\n\n");
            break;
        }
    }
    sys_exit(0);
}
