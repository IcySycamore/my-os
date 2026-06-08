// kernel/proc.c — Process management (xv6 proc.c)
#include "proc.h"
#include "file.h"

void uart_puts(const char *s);
void uart_putdec(int n);
void uart_puthex(unsigned long x);
extern char trap_ret[];

Task tasks[MAX_TASKS];
int  task_count   = 0;
int  current_task = 0;
int  next_pid = 1;

// Find a free slot (NONE or reaped SLEEPING with entry==0)
static int alloc_slot(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING
            && tasks[i].entry == 0
            && tasks[i].sleep_until == 0x7FFFFFFF)
            return i;
        if (tasks[i].entry == 0 && tasks[i].state == 0)
            return i;
    }
    if (task_count < MAX_TASKS)
        return task_count;
    return -1;
}

int exec(TaskFunc func)
{
    int slot = alloc_slot();
    if (slot < 0) return -1;

    Task *t = &tasks[slot];
    t->pid   = next_pid++;
    t->parent_pid = (task_count > 0) ? tasks[current_task].pid : 0;
    t->exit_code = 0;
    t->entry = (unsigned long)func;
    t->state = TASK_READY;
    t->sleep_until = 0;
    t->ctx.ra = (unsigned long)func;
    t->ctx.sp = (((unsigned long)(t->stack + STACK_SIZE)) & ~15UL) - 256;
    t->ksp    = t->ctx.sp;
    for (int i = 2; i < 14; i++)
        ((unsigned long*)&t->ctx)[i] = 0;
    for (int i = 0; i < 256/8; i++)
        ((unsigned long*)t->trapframe)[i] = 0;
    // Inherit parent's console fds (0/1/2) via filedup
    if (task_count > 0) {
        Task *parent = &tasks[current_task];
        for (int i = 0; i < 3; i++) {
            if (parent->ofile[i])
                t->ofile[i] = filedup(parent->ofile[i]);
            else
                t->ofile[i] = 0;
        }
        for (int i = 3; i < NOFILE; i++)
            t->ofile[i] = 0;
    } else {
        for (int i = 0; i < NOFILE; i++)
            t->ofile[i] = 0;
    }
    if (slot == task_count)
        task_count++;
    return slot;
}

// fork: clone current task. ofile[] is dup'd (filedup).
int fork(void)
{
    int slot = alloc_slot();
    if (slot < 0) return -1;
    Task *parent = &tasks[current_task];
    Task *child  = &tasks[slot];

    child->pid        = next_pid++;
    child->parent_pid = parent->pid;
    child->exit_code  = 0;
    child->entry      = parent->entry;
    child->state      = TASK_READY;
    child->sleep_until = 0;

    // Copy parent's trapframe — exactly like xv6: *(np->trapframe) = *(p->trapframe)
    for (int i = 0; i < 256 / 8; i++)
        ((unsigned long*)child->trapframe)[i] = ((unsigned long*)parent->trapframe)[i];
    ((unsigned long*)child->trapframe)[8] = 0;   // a0=0 — trapframe must agree with ksp

    // Fresh ksp — copy ALL parent registers (27 regs + mepc), then set a0=0
    unsigned long child_ksp_val = (((unsigned long)(child->stack + STACK_SIZE)) & ~15UL) - 256;
    child->ksp = child_ksp_val;
    for (int i = 0; i < 256 / 8; i++)
        ((unsigned long*)child_ksp_val)[i] = ((unsigned long*)parent->ksp)[i];
    ((unsigned long*)child_ksp_val)[8] = 0;  // a0=0 — fork returns 0 in child

    // ★ Copy open file table (dup each fd)
    for (int i = 0; i < NOFILE; i++) {
        if (parent->ofile[i]) {
            child->ofile[i] = filedup(parent->ofile[i]);
        } else {
            child->ofile[i] = 0;
        }
    }

    // ctx: trap_ret → swtch → trap_ret restores regs → mret → back to ecall+4
    extern char trap_ret[];
    child->ctx.ra = (unsigned long)trap_ret;
    child->ctx.sp = child_ksp_val;
    for (int i = 2; i < 14; i++)
        ((unsigned long*)&child->ctx)[i] = 0;

    if (slot == task_count)
        task_count++;

    return child->pid;
}
