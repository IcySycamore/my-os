// kernel/proc.h — Task structure & process management (xv6 proc.h)
#ifndef _PROC_H_
#define _PROC_H_

#include "types.h"

// xv6 kernel/proc.h — struct context (用于 swtch.S)
struct context {
    unsigned long ra;
    unsigned long sp;
    unsigned long s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
};

#define STACK_SIZE 4096
#define MAX_TASKS  8
#define NOFILE    16

enum { TASK_READY, TASK_RUNNING, TASK_SLEEPING, TASK_ZOMBIE };

typedef void (*TaskFunc)(void);

struct file;  // forward declaration

typedef struct {
    unsigned long entry;
    struct context ctx;
    unsigned long ksp;
    unsigned char  stack[STACK_SIZE];
    unsigned char  trapframe[256];
    int  state;
    int  pid;
    int  parent_pid;
    int  exit_code;
    int  sleep_until;
    struct file *ofile[NOFILE];    // ★ per-process open file table
} Task;

// 调度器外部接口
extern Task tasks[MAX_TASKS];
extern int  task_count;
extern int  current_task;
extern int  next_pid;
extern volatile unsigned long ticks;

int  exec(TaskFunc func);   // create new task, inherit parent fds 0-2
int  fork(void);
void sched(void);        // yield from current task → scheduler
void scheduler(void);    // main scheduling loop, never returns

// swtch.S
void swtch(struct context*, struct context*);

#endif
