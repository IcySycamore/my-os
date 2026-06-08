// user/shell.c — xv6-style Shell using pure syscalls (no direct UART)

// Syscall wrappers
void sys_write(int fd, const char *s, int len);
int  sys_read(int fd, char *buf, int len);
int  sys_open(const char *name, int flags);
void sys_close(int fd);
int  sys_getpid(void);
int  sys_uptime(void);
void sys_fs_list(void);
void sys_exit(int code);
int  sys_wait(void);
int  sys_fork(void);
void uart_puts(const char *s);

// Task creation (now exec-style: inherits parent fds 0-2)
int  exec(void (*func)(void));

#define MAX_CMD  64

static int str_eq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static int slen(const char *s) { int n=0; while(s[n])n++; return n; }
static void put(const char *s) { sys_write(1, s, slen(s)); }

static void putdec(int n)
{
    char buf[12]; int i = 0;
    if (n < 0) { put("-"); n = -n; }
    if (n == 0) { put("0"); return; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) { char s[2] = { buf[--i], 0 }; put(s); }
}

// readline: uses sys_read(0) — console driver does echo + line buffering
static void readline(char *buf, int max)
{
    int n = sys_read(0, buf, max);
    if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = 0;
    else buf[n > 0 ? n : 0] = 0;
}

// do_cat: read from embedded file fd, write to stdout
static void do_cat(const char *name)
{
    char buf[128]; int n;
    int fd = sys_open(name, 0);
    if (fd < 0) { put("  Error: file not found\n"); return; }
    while ((n = sys_read(fd, buf, sizeof(buf))) > 0)
        sys_write(1, buf, n);
    put("\n");
    sys_close(fd);
}

// ---- Demo dispatch ----

int child_iters;  // shared: Shell sets before exec(), demos read via user.h

extern void guess_main(void);
extern void worm_main(void);
extern void typewriter_main(void);
extern void catall_main(void);
extern void tree_main(void);
extern void race_main(void);

// ---- Main Shell ----

void shell_main(void)
{
    char cmd[MAX_CMD];
    int  running = 1;

    put("\n========================================\n");
    put("  MY-OS Shell v4.2\n");
    put("  Type 'help' for commands\n");
    put("========================================\n\n");

    while (running) {
        put("myos> ");
        readline(cmd, MAX_CMD);
        if (cmd[0] == 0) continue;

        const char *t = cmd;
        while (*t == ' ') t++;
        const char *arg = t;
        while (*arg && *arg != ' ') arg++;
        int has_arg = (*arg == ' ');
        if (has_arg) { *(char*)arg = 0; arg++; while (*arg == ' ') arg++; }
        else arg = 0;

        if (str_eq(t, "help")) {
            put("Commands:\n");
            put("  help              This help\n");
            put("  ls                List embedded files\n");
            put("  cat <name>         Print file (embedded or FAT disk)\n");
            put("  run <demo> [N]    Run demo. N times for race and worm(default 3)\n");
            put("  fork              Fork → parent & child\n");
            put("  exit              Quit Shell\n\n");
            put("Demos (run <demo> [N]):\n");
            put("  guess    - Number guessing game (1-100)\n");
            put("  worm     - Animated progress worm\n");
            put("  typewr - Typewriter reads DISKONLY.TXT\n");
            put("  catall   - Print all embedded + FAT files\n");
            put("  tree     - Growing ASCII tree\n");
            put("  race     - Forked racer A vs B\n\n");
        }
        else if (str_eq(t, "ls")) {
            sys_fs_list();
        }
        else if (str_eq(t, "cat")) {
            if (!arg) { put("Usage: cat <filename>\n"); continue; }
            do_cat(arg);
        }
        else if (str_eq(t, "run")) {
            if (!arg) { put("Usage: run <a|b|c|file|test|race> [N]\n"); continue; }
            const char *name = arg;
            while (*arg && *arg != ' ') arg++;
            int has_n = (*arg == ' ');
            if (has_n) { *(char*)arg = 0; arg++; while (*arg == ' ') arg++; }

            int n = 3;
            if (has_n && *arg >= '0' && *arg <= '9') {
                n = 0;
                while (*arg >= '0' && *arg <= '9') n = n * 10 + (*arg++ - '0');
            }
            if (n <= 0) n = 1;
            if (n > 500) n = 500;//硬性截断大小

            child_iters = n;
            void (*func)(void) = 0;

            if      (str_eq(name, "guess"))      func = guess_main;
            else if (str_eq(name, "worm"))      func = worm_main;
            else if (str_eq(name, "typewr"))      func = typewriter_main;
            else if (str_eq(name, "catall"))   func = catall_main;
            else if (str_eq(name, "tree"))   func = tree_main;
            else if (str_eq(name, "race"))   func = race_main;
            else { put("Unknown task: "); put(name); put("\n"); continue; }

            int idx = exec(func);
            if (idx < 0) { put("Error: too many tasks\n"); continue; }

            int waited;
            while ((waited = sys_wait()) < 0)
                for (volatile int d = 0; d < 50000; d++);

            put("[Shell] child pid="); putdec(waited);
            put(" exited\n\n");
        }
        else if (str_eq(t, "fork")) {
            int pid = sys_fork();
            if (pid < 0) {
                put("fork failed\n");
            } else if (pid == 0) {
                put("[child] fork returned 0, pid=");
                putdec(sys_getpid()); put("\n");
                for (int i = 0; i < 3; i++) {
                    put("[child] loop "); putdec(i + 1);
                    put(" tick="); putdec(sys_uptime()); put("\n");
                    for (volatile int d = 0; d < 3000000; d++);
                }
                put("[child] exiting\n");
                sys_exit(0);
            } else {
                put("[parent] fork returned pid=");
                putdec(pid); put("\n");
                int w = sys_wait();
                put("[parent] child pid=");
                putdec(w); put(" reaped\n\n");
            }
        }
        else if (str_eq(t, "exit")) {
            put("\n[Shell] Exiting. Goodbye!\n");
            running = 0;
        }
        else {
            put("Unknown: "); put(t);
            put("\nType 'help' for commands.\n");
        }
    }

    put("\n========== System Halted ==========\n");
    while (1) asm volatile("wfi");
}
