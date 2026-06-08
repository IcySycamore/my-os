// kernel/syscall.c — System call dispatch (VFS-backed)
#include "proc.h"
#include "fs.h"
#include "file.h"

void uart_puts(const char *s);
void uart_putc(char c);
void uart_putdec(int n);
int  uart_getc(void);
void console_init(void);
void fat_list(void);
struct file* sysfile_open(const char *name);

#define SYS_WRITE    1
#define SYS_YIELD    2
#define SYS_GETPID   3
#define SYS_UPTIME   4
#define SYS_SLEEP    5
#define SYS_OPEN     6
#define SYS_READ     7
#define SYS_CLOSE    8
#define SYS_FS_LIST  9
#define SYS_EXIT    10
#define SYS_WAIT    11
#define SYS_GETCHAR 12
#define SYS_FORK    13

// Helper: get file from current process's ofile[]
static struct file* fd2file(int fd) {
    Task *t = &tasks[current_task];
    if (fd < 0 || fd >= NOFILE) return 0;
    return t->ofile[fd];
}

void handle_syscall(void)
{
    Task *t = &tasks[current_task];
    unsigned long *tf = (unsigned long *)t->trapframe;
    unsigned long *ksp = (unsigned long *)t->ksp;
    unsigned long a7 = tf[15];

    switch (a7) {
    case SYS_WRITE: {
        struct file *f = fd2file((int)tf[8]);
        if (!f) { ksp[8] = (unsigned long)(-1); break; }
        char *buf = (char*)tf[9];
        int len = (int)tf[10];
        ksp[8] = (unsigned long)(long)filewrite(f, buf, len);
        break;
    }
    case SYS_GETPID:
        ksp[8] = (unsigned long)t->pid;
        break;
    case SYS_UPTIME:
        ksp[8] = ticks;
        break;
    case SYS_SLEEP:
        t->sleep_until = (int)(ticks + tf[8]);
        t->state = TASK_SLEEPING;
        ksp[8] = 0;
        sched();
        break;
    case SYS_YIELD:
        t->state = TASK_READY;
        ksp[8] = 0;
        sched();
        break;
    case SYS_EXIT:
        t->exit_code = (int)tf[8];
        // Reparent children to init (pid=1) — xv6-style orphan handling
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].parent_pid == t->pid) {
                tasks[i].parent_pid = 1;
                if (tasks[i].state == TASK_ZOMBIE) {
                    // Init may be sleeping in wait; wake it
                    for (int j = 0; j < task_count; j++)
                        if (tasks[j].pid == 1 && tasks[j].state == TASK_SLEEPING)
                            tasks[j].state = TASK_READY;
                }
            }
        }
        // Close all open files
        for (int i = 0; i < NOFILE; i++) {
            if (t->ofile[i]) {
                fileclose(t->ofile[i]);
                t->ofile[i] = 0;
            }
        }
        t->state = TASK_ZOMBIE;
        {
            int pp = t->parent_pid;
            for (int i = 0; i < task_count; i++)
                if (tasks[i].pid == pp && tasks[i].state == TASK_SLEEPING)
                    tasks[i].state = TASK_READY;
        }
        ksp[8] = 0;
        sched();
        break;
    case SYS_WAIT: {
        for (;;) {
            for (int i = 0; i < task_count; i++) {
                if (tasks[i].parent_pid == t->pid && tasks[i].state == TASK_ZOMBIE) {
                    int cpid = tasks[i].pid;
                    tasks[i].state       = TASK_SLEEPING;
                    tasks[i].sleep_until = 0x7FFFFFFF;
                    tasks[i].parent_pid  = 0;
                    tasks[i].entry       = 0;
                    ksp[8] = (unsigned long)cpid;
                    return;
                }
            }
            t->sleep_until = 0;
            t->state = TASK_SLEEPING;
            ksp[8] = (unsigned long)(-1);
            sched();
        }
    }
    case SYS_GETCHAR: {
        int c = uart_getc();
        ksp[8] = (unsigned long)c;  // -1 = no input, no sleep
        break;
    }
    case SYS_FORK: {
        int child_pid = fork();
        ksp[8] = (unsigned long)(child_pid > 0 ? child_pid : 0);
        break;
    }
    case SYS_OPEN: {
        struct file *f = sysfile_open((const char*)tf[8]);
        if (!f) { ksp[8] = (unsigned long)(-1); break; }
        int fd = -1;
        for (int i = 0; i < NOFILE; i++) {
            if (t->ofile[i] == 0) { t->ofile[i] = f; fd = i; break; }
        }
        if (fd < 0) { fileclose(f); }
        ksp[8] = (unsigned long)fd;
        break;
    }
    case SYS_READ: {
        struct file *f = fd2file((int)tf[8]);
        if (!f) { ksp[8] = (unsigned long)(-1); break; }
        char *buf = (char*)tf[9];
        int len = (int)tf[10];
        ksp[8] = (unsigned long)(long)fileread(f, buf, len);
        break;
    }
    case SYS_CLOSE: {
        int fd = (int)tf[8];
        if (fd >= 0 && fd < NOFILE && t->ofile[fd]) {
            fileclose(t->ofile[fd]);
            t->ofile[fd] = 0;
        }
        ksp[8] = 0;
        break;
    }
    case SYS_FS_LIST:
        fs_list();
        fat_list();
        ksp[8] = 0;
        break;
    }
}

// ---- 系统调用辅助宏 (给任务代码用) ----

void sys_write(int fd, const char *s, int len)
{
    register long a0 asm("a0") = (long)fd;
    register long a1 asm("a1") = (long)s;
    register long a2 asm("a2") = (long)len;
    register long a7 asm("a7") = SYS_WRITE;
    asm volatile("ecall" : : "r"(a0), "r"(a1), "r"(a2), "r"(a7) : "memory");
}

int sys_getpid(void)
{
    register long a7 asm("a7") = SYS_GETPID, a0 asm("a0");
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return (int)a0;
}

int sys_uptime(void)
{
    register long a7 asm("a7") = SYS_UPTIME, a0 asm("a0");
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return (int)a0;
}

int sys_open(const char *name, int flags)
{
    register long a0 asm("a0") = (long)name;
    register long a1 asm("a1") = (long)flags;
    register long a7 asm("a7") = SYS_OPEN;
    asm volatile("ecall" : "=r"(a0) : "r"(a0), "r"(a1), "r"(a7) : "memory");
    return (int)a0;
}

int sys_read(int fd, char *buf, int len)
{
    register long a0 asm("a0") = (long)fd;
    register long a1 asm("a1") = (long)buf;
    register long a2 asm("a2") = (long)len;
    register long a7 asm("a7") = SYS_READ;
    asm volatile("ecall" : "=r"(a0) : "r"(a0), "r"(a1), "r"(a2), "r"(a7) : "memory");
    return (int)a0;
}

void sys_close(int fd)
{
    register long a0 asm("a0") = (long)fd, a7 asm("a7") = SYS_CLOSE;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}

void sys_fs_list(void)
{
    register long a7 asm("a7") = SYS_FS_LIST;
    asm volatile("ecall" : : "r"(a7) : "memory");
}

void sys_exit(int code)
{
    register long a0 asm("a0") = (long)code, a7 asm("a7") = SYS_EXIT;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    while (1); // never reached — schedule() switches away
}

int sys_wait(void)
{
    register long a7 asm("a7") = SYS_WAIT, a0 asm("a0");
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return (int)a0;
}

int sys_getchar(void)
{
    register long a7 asm("a7") = SYS_GETCHAR, a0 asm("a0");
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return (int)a0;
}

int sys_fork(void)
{
    register long a7 asm("a7") = SYS_FORK, a0 asm("a0");
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return (int)a0;
}

void sys_sleep(int ticks)
{
    register long a0 asm("a0") = (long)ticks, a7 asm("a7") = SYS_SLEEP;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}
