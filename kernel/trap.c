// kernel/trap.c — minimal trap handler, polling-only
#include "proc.h"

void uart_puts(const char *s);
void uart_puthex(unsigned long x);
void uart_putdec(int n);
void handle_syscall(void);

static inline unsigned long r_mstatus(void)
{ unsigned long x; asm("csrr %0,mstatus":"=r"(x)); return x; }
static inline void w_mstatus(unsigned long x)
{ asm("csrw mstatus,%0"::"r"(x)); }
static inline void w_mtvec(unsigned long a)
{ asm("csrw mtvec,%0"::"r"(a)); }
static inline unsigned long r_mcause(void)
{ unsigned long x; asm("csrr %0,mcause":"=r"(x)); return x; }
static inline unsigned long r_mepc(void)
{ unsigned long x; asm("csrr %0,mepc":"=r"(x)); return x; }
static inline void w_mepc(unsigned long x)
{ asm("csrw mepc,%0"::"r"(x)); }
static inline unsigned long r_mie(void)
{ unsigned long x; asm("csrr %0,mie":"=r"(x)); return x; }
static inline void w_mie(unsigned long x)
{ asm("csrw mie,%0"::"r"(x)); }

#define CLINT_BASE      0x02000000UL
#define CLINT_MTIMECMP   (CLINT_BASE + 0x4000)
#define CLINT_MTIME      (CLINT_BASE + 0xbff8)
#define TIMER_INTERVAL   8000000
volatile unsigned long ticks;
volatile unsigned long trap_sp;

static struct context sched_ctx;
static char sched_stack[4096] __attribute__((aligned(16)));
static volatile int in_scheduler = 0;

static void timer_set_next(void)
{
    volatile unsigned long *mt = (unsigned long*)CLINT_MTIME;
    volatile unsigned long *mc = (unsigned long*)CLINT_MTIMECMP;
    *mc = *mt + TIMER_INTERVAL;
}

void sched(void)
{
    Task *p = &tasks[current_task];
    for (int i = 0; i < 256 / 8; i++)
        ((unsigned long*)p->trapframe)[i] = ((unsigned long*)p->ksp)[i];
    swtch(&p->ctx, &sched_ctx);
}

void scheduler(void)
{
    unsigned long my_sp = ((unsigned long)(sched_stack + 4096)) & ~15UL;
    while (1) {
        in_scheduler = 1;
        // MIE is OFF here (entered via ecall which cleared it).
        // We keep it off during the scan+swtch to prevent timer IRQ
        // from corrupting a fork child's ksp during the swtch→trap_ret window.

        // Wake SLEEPING tasks whose timer has expired
        for (int i = 0; i < task_count; i++)
            if (tasks[i].state == TASK_SLEEPING
                && tasks[i].sleep_until > 0
                && ticks >= (unsigned long)tasks[i].sleep_until)
                tasks[i].state = TASK_READY;

        // xv6-style: scan for RUNNABLE, swtch to it
        int found = 0;
        for (int n = 0; n < task_count; n++) {
            int idx = (current_task + 1 + n) % task_count;
            if (tasks[idx].state == TASK_READY) {
                current_task = idx;
                tasks[idx].state = TASK_RUNNING;
                unsigned long *ksp = (unsigned long*)tasks[idx].ksp;
                for (int j = 0; j < 256 / 8; j++)
                    ksp[j] = ((unsigned long*)tasks[idx].trapframe)[j];
                trap_sp = (unsigned long)ksp;
                in_scheduler = 0;
                swtch(&sched_ctx, &tasks[idx].ctx);
                found = 1;
                break;
            }
        }
        if (!found) {
            int any_sleep = 0;
            for (int i = 0; i < task_count; i++)
                if (tasks[i].state == TASK_SLEEPING
                    && tasks[i].sleep_until != 0x7FFFFFFF)
                    { any_sleep = 1; break; }
            if (any_sleep) {
                // Busy-wait polling CLINT mtime instead of wfi+ISR.
                // Timer ISR is unreliable in our M-mode setup, so we
                // poll the hardware counter directly to advance ticks.
                asm volatile("mv sp, %0" :: "r"(my_sp));
                volatile unsigned long *mt = (unsigned long*)CLINT_MTIME;
                unsigned long now = *mt;
                while (1) {
                    unsigned long cur = *mt;
                    if (cur - now >= TIMER_INTERVAL / 100) {
                        ticks++;
                        now = cur;
                    }
                    int any_ready = 0;
                    for (int i = 0; i < task_count; i++)
                        if (tasks[i].state == TASK_SLEEPING
                            && tasks[i].sleep_until > 0
                            && ticks >= (unsigned long)tasks[i].sleep_until)
                            { any_ready = 1; break; }
                    if (any_ready) break;
                }
            } else {
                uart_puts("\n[HALT] All tasks terminated.\n");
                uart_puts("[HALT] Simulate physical power-off: use Ctrl+C or pkill qemu\n");
                while (1) asm volatile("wfi");
            }
        }
    }
}

// machine_trap — naked stub: call C helper, then jr trap_ret
__attribute__((naked, used))
void machine_trap(void)
{
    asm volatile(
        "    addi sp, sp, -64\n"
        "    sd   ra, 0(sp)\n"
        "    call machine_trap_c\n"
        "    ld   ra, 0(sp)\n"
        "    addi sp, sp, 64\n"
        "    la   t0, trap_ret\n"
        "    jr   t0\n"
    );
}

__attribute__((used))
void machine_trap_c(void)
{
    Task *t = &tasks[current_task];

    // 只在进程上下文中保存 trapframe（不在 scheduler 内）
    if (!in_scheduler) {
        t->ksp = trap_sp;
        for (int i = 0; i < 256 / 8; i++)
            ((unsigned long*)t->trapframe)[i] = ((unsigned long*)trap_sp)[i];
    }

    unsigned long cause = r_mcause();
    if (cause & (1UL << 63)) {
        if ((cause & 0xff) == 7) {
            ticks++; timer_set_next();
        }
    } else {
        if (cause == 11) {
            unsigned long epc = r_mepc();
            ((unsigned long*)t->trapframe)[31] = epc + 4;
            ((unsigned long*)trap_sp)[31] = epc + 4;
            w_mepc(epc + 4);
            handle_syscall();
        } else {
            // Load/store fault — skip the faulting instruction
            unsigned long epc = r_mepc();
            w_mepc(epc + 4);
            ((unsigned long*)trap_sp)[31] = epc + 4;
        }
    }
}

__attribute__((naked)) void trap_vector(void)
{
    asm volatile(
        "    addi sp, sp, -256\n"
        "    la   t0, trap_sp\n" "    sd   sp, 0(t0)\n"
        "    sd  ra,   0(sp)\n" "    sd  t0,   8(sp)\n"
        "    sd  t1,  16(sp)\n" "    sd  t2,  24(sp)\n"
        "    sd  t3,  32(sp)\n" "    sd  t4,  40(sp)\n"
        "    sd  t5,  48(sp)\n" "    sd  t6,  56(sp)\n"
        "    sd  a0,  64(sp)\n" "    sd  a1,  72(sp)\n"
        "    sd  a2,  80(sp)\n" "    sd  a3,  88(sp)\n"
        "    sd  a4,  96(sp)\n" "    sd  a5, 104(sp)\n"
        "    sd  a6, 112(sp)\n" "    sd  a7, 120(sp)\n"
        "    sd  s0, 128(sp)\n" "    sd  s1, 136(sp)\n"
        "    sd  s2, 144(sp)\n" "    sd  s3, 152(sp)\n"
        "    sd  s4, 160(sp)\n" "    sd  s5, 168(sp)\n"
        "    sd  s6, 176(sp)\n" "    sd  s7, 184(sp)\n"
        "    sd  s8, 192(sp)\n" "    sd  s9, 200(sp)\n"
        "    sd s10, 208(sp)\n" "    sd s11, 216(sp)\n"
        "    call machine_trap\n"
        ".globl trap_ret\n"
        "trap_ret:\n"
        "    la   t0, trap_sp\n" "    ld   sp, 0(t0)\n"
        "    ld  ra,   0(sp)\n" "    ld  t0,   8(sp)\n"
        "    ld  t1,  16(sp)\n" "    ld  t2,  24(sp)\n"
        "    ld  t3,  32(sp)\n" "    ld  t4,  40(sp)\n"
        "    ld  t5,  48(sp)\n" "    ld  t6,  56(sp)\n"
        "    ld  a0,  64(sp)\n" "    ld  a1,  72(sp)\n"
        "    ld  a2,  80(sp)\n" "    ld  a3,  88(sp)\n"
        "    ld  a4,  96(sp)\n" "    ld  a5, 104(sp)\n"
        "    ld  a6, 112(sp)\n" "    ld  a7, 120(sp)\n"
        "    ld  s0, 128(sp)\n" "    ld  s1, 136(sp)\n"
        "    ld  s2, 144(sp)\n" "    ld  s3, 152(sp)\n"
        "    ld  s4, 160(sp)\n" "    ld  s5, 168(sp)\n"
        "    ld  s6, 176(sp)\n" "    ld  s7, 184(sp)\n"
        "    ld  s8, 192(sp)\n" "    ld  s9, 200(sp)\n"
        "    ld s10, 208(sp)\n" "    ld s11, 216(sp)\n"
        "    ld  t0, 248(sp)\n" "    csrw mepc, t0\n"
        "    addi sp, sp, 256\n"
        "    mret\n"
    );
}

void trap_init(void)
{
    w_mtvec((unsigned long)&trap_vector);
    timer_set_next();
    w_mie(r_mie() | (1 << 7));
    w_mstatus(r_mstatus() | (1 << 3));
}
