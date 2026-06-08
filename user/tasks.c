// user/tasks.c — User task functions (xv6 user/*.c)
// These run as "user" tasks in M-mode

void uart_putdec(int n);
void sys_write(const char *s);
int  sys_getpid(void);
int  sys_uptime(void);
int  sys_open(const char *name);
int  sys_read(int fd, char *buf, int len);
void sys_close(int fd);
void sys_fs_list(void);

void task_a(void)
{
    sys_write("[A] pid="); uart_putdec(sys_getpid()); sys_write("\n");
    while (1) {
        sys_write("[A] "); uart_putdec(sys_uptime()); sys_write("\n");
        for (volatile int i = 0; i < 3000000; i++);
    }
}

void task_b(void)
{
    sys_write("[B] pid="); uart_putdec(sys_getpid()); sys_write("\n");
    while (1) {
        sys_write("[B] "); uart_putdec(sys_uptime()); sys_write("\n");
        for (volatile int i = 0; i < 3000000; i++);
    }
}

void task_c(void)
{
    sys_write("[C] pid="); uart_putdec(sys_getpid()); sys_write("\n");
    while (1) {
        sys_write("[C] "); uart_putdec(sys_uptime()); sys_write("\n");
        for (volatile int i = 0; i < 3000000; i++);
    }
}

// v5.0: File reader task — demonstrates reading external Windows files
void task_fileread(void)
{
    char buf[128];
    int  n, fd;

    sys_write("\n[FILEREAD] pid=");
    uart_putdec(sys_getpid());
    sys_write(" — External File Reader Demo\n");

    // List all available files
    sys_fs_list();

    // Read hello.txt
    fd = sys_open("hello.txt");
    if (fd >= 0) {
        sys_write("[FILEREAD] Opened 'hello.txt' (fd=");
        uart_putdec(fd);
        sys_write(")\n");
        sys_write("[FILEREAD] --- Content of hello.txt ---\n");

        while ((n = sys_read(fd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = 0;
            sys_write(buf);
        }
        sys_write("\n[FILEREAD] --- End of hello.txt ---\n");
        sys_close(fd);
    } else {
        sys_write("[FILEREAD] ERROR: cannot open hello.txt\n");
    }

    // Read readme.txt
    fd = sys_open("readme.txt");
    if (fd >= 0) {
        sys_write("[FILEREAD] Opened 'readme.txt' (fd=");
        uart_putdec(fd);
        sys_write(")\n");
        sys_write("[FILEREAD] --- Content of readme.txt ---\n");

        while ((n = sys_read(fd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = 0;
            sys_write(buf);
        }
        sys_write("\n[FILEREAD] --- End of readme.txt ---\n");
        sys_close(fd);
    }

    // Keep running to show integration with scheduler
    while (1) {
        sys_write("[FILEREAD] ");
        uart_putdec(sys_uptime());
        sys_write(" (file reading demo complete, looping)\n");
        for (volatile int i = 0; i < 5000000; i++);
    }
}

// v5.0: One-shot test.txt read task — reads and prints test.txt once
void task_testread(void)
{
    char buf[128];
    int  n, fd;

    sys_write("\n[TESTREAD] pid=");
    uart_putdec(sys_getpid());
    sys_write(" — Reading test.txt from Windows host\n");

    fd = sys_open("test.txt");
    if (fd >= 0) {
        sys_write("[TESTREAD] Opened 'test.txt' (fd=");
        uart_putdec(fd);
        sys_write(")\n");
        sys_write("[TESTREAD] === test.txt content ===\n");

        while ((n = sys_read(fd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = 0;
            sys_write(buf);
        }
        sys_write("\n[TESTREAD] === End of test.txt ===\n");
        sys_close(fd);
    } else {
        sys_write("[TESTREAD] ERROR: cannot open test.txt\n");
    }

    // Keep looping with scheduler integration
    while (1) {
        sys_write("[TESTREAD] ");
        uart_putdec(sys_uptime());
        sys_write(" ticks\n");
        for (volatile int i = 0; i < 5000000; i++);
    }
}
