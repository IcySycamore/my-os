# MY-OS v5.2 — 开发者手册

> 逐文件详解 ~2300 行代码，覆盖 VFS、virtio-blk、FAT12/16 磁盘读取。

---

## 目录

1. [文件清单](#一文件清单)
2. [入口 → Shell 完整执行追踪](#二入口--shell-完整执行追踪)
3. [逐文件详解](#三逐文件详解)
4. [内存布局](#四内存布局)
5. [系统调用表](#五系统调用表)
6. [与 xv6 对照](#六与-xv6-对照)

---

## 一、文件清单

```
my-os/
├── kernel.ld              链接脚本（内存布局）
├── Makefile                构建规则
├── files/
│   ├── hello.txt           示例文件
│   ├── readme.txt          功能说明文件
│   └── DISKONLY.TXT        VVFAT 磁盘专属文件
├── kernel/
│   ├── entry.S             CPU 启动入口（汇编）
│   ├── start.c             内核 C 入口
│   ├── swtch.S             上下文切换（汇编，28 条指令）
│   ├── uart.c              NS16550A 串口驱动
│   ├── vm.c                Sv39 三级页表管理
│   ├── proc.h              任务控制块定义
│   ├── proc.c              进程管理（exec/fork）
│   ├── trap.c              陷阱入口 + 调度器
│   ├── syscall.c           系统调用分发器（13 个）
│   ├── sysfile.c           文件打开分发（sysfile_open）
│   ├── fs.h                嵌入文件系统头文件
│   ├── fs.c                嵌入文件系统实现
│   ├── files_embed.S       用 .incbin 嵌入外部文件
│   ├── file.h / file.c     VFS + devsw 设备表
│   ├── console.c           xv6 风格控制台驱动
│   ├── virtio.h            VirtIO MMIO 寄存器定义
│   ├── virtio_blk.c        virtio-blk 轮询磁盘驱动
│   ├── fat.c               FAT12/16 读取器（通过函数指针 seam 解耦磁盘 I/O）
│   └── types.h             基础类型定义
└── user/
    ├── user.h              用户程序共享头文件
    ├── ulib.c              用户库（slen/put/putdec/readline/do_cat/atoi2）
    ├── shell.c             交互式 Shell（6 条命令 + 6 个 Demo 调度）
    ├── guess.c             猜数字游戏
    ├── worm.c              动画进度条
    ├── typewriter.c        逐字 FAT 磁盘读取
    ├── catall.c            列出所有嵌入 + FAT 文件
    ├── tree.c              生长 ASCII 树
    └── race.c              Fork A/B 竞速
```

---

## 二、入口 → Shell 完整执行追踪

以下是按指令级精度的追踪。标注 `★` 的是关键节点。

### 阶段 A：CPU 上电 (`entry.S`)

```
QEMU 上电
  PC = 0x80000000           ← 链接脚本 ENTRY(_entry) 指定
  priv = M-mode
  mstatus.MIE = 0           ← 中断关

_entry:
  csrr t0, mhartid          → t0 = 0 (单核)
  bnez t0, park             → 不跳（hart 0）

  la t0, __bss_start        → BSS 起始地址
  la t1, __bss_end          → BSS 结束地址
  循环: sd zero, (t0) ...   → BSS 清零

  la sp, __stack_top         → sp = 约 0x80210000 (128KB 栈)
  j start                    → ★ 进入 C 代码
```

### 阶段 B：`start()` — 内核初始化

```c
start():
  uart_init()            → 配置 NS16550A（38400 baud, 8N1）
  uart_puts(banner)      → 打印横幅到终端

  kalloc_init()          → 物理页分配器起始地址 = ALIGN(__end, 4096)
  kvminit()              → ★ 建立 Sv39 身份映射 + 写 satp 启用分页
    ① kalloc_page()       分配 4KB 零页 → kernel_pagetable
    ② mappages(0x80000000, 0x80000000, 2MB, R|W|X)  身份映射内核
    ③ mappages(0x10000000, 0x10000000, 4KB, R|W)     UART
    ④ mappages(0x02000000, 0x02000000, 4KB, R|W)     CLINT
    ⑤ w_satp(MAKE_SATP(kernel_pagetable))            启用 MMU
    ⑥ sfence_vma                                    刷新 TLB
  vmprint()              → 打印页表树状结构

  trap_init()            → ★ 安装陷阱向量
    w_mtvec(&trap_vector)   设 mtvec CSR = trap_vector 地址
    timer_set_next()        设 mtimecmp = mtime + 8,000,000
    w_mie(MTIE)             开定时器中断
    w_mstatus(MIE)          开全局中断

  fs_init()              → 清空文件表和文件描述符表
  console_init()         → 初始化控制台行缓冲 (cons.r = cons.w = cons.e = 0)
  virtio_blk_init()      → 探测 MMIO 基址，协商 feature，设置 queue
  fat_init(virtio_blk_read) → ★ 注入磁盘读函数指针 seam，解析 MBR/BPB

  task_create(shell_main, 0) → ★ 创建 Shell 任务 (pid=1, parent=0)
    tasks[0].ctx.ra = shell_main
    tasks[0].ctx.sp = stack_top - 256
    tasks[0].ksp = ctx.sp
    tasks[0].state = TASK_READY
    task_count = 1

  scheduler()            → ★ 进入调度器主循环，永不返回
```

### 阶段 C：`scheduler()` → `swtch` → `shell_main()`

```c
scheduler() 循环:
  ① 扫描 tasks[0..task_count-1]，找 state == TASK_READY
  ② 找到 tasks[0] (Shell), state = READY
  ③ current_task = 0, tasks[0].state = TASK_RUNNING
  ④ 恢复 tasks[0].trapframe → tasks[0].ksp 区域
  ⑤ trap_sp = tasks[0].ksp   ★ 全局变量，trap_ret 从此加载 sp
  ⑥ swtch(&sched_ctx, &tasks[0].ctx)   ★ 上下文切换

swtch.S 28 条指令:
  sd ra, 0(a0)    → 保存调度器的 ra 到 sched_ctx.ra
  sd sp, 8(a0)    → 保存调度器的 sp 到 sched_ctx.sp
  sd s0-s11        → 保存 12 个 callee-saved 寄存器
  ld ra, 0(a1)    → ra = tasks[0].ctx.ra = shell_main ★
  ld sp, 8(a1)    → sp = tasks[0].ctx.sp = Shell 栈顶 - 256
  ld s0-s11        → 恢复 s0-s11 (全零)
  ret             → ★ jump shell_main
```

### 阶段 D：`shell_main()` — 打印横幅 + 输入循环

```c
shell_main():
  put("=====...=====")      → sys_write(1, s, len) → ecall → SYS_WRITE
  put("Type 'help'...")
  while (running):
    put("myos> ")          → 打印提示符
    readline(cmd, 64)      → sys_read(0, buf, 64) → ecall → SYS_READ

SYS_READ(fd=0):
  → console_read(buf, 64)  ★
    while (cons.w == cons.r):              ← 忙等直到有数据
      uart_getc()   → 读 UART LSR 寄存器
      无输入 → 返回 -1 → 继续循环
      有输入 → console_intr(c)             ← 回显 + 行缓冲
        可打印字符 → uart_putc 回显 → 存入 cons.buf
        '\n' → cons.w = cons.e            ← 行完成！
    cons.w != cons.r → 逐字节复制到用户 buf → 返回

  用户输入 "help\n" → cmd = "help"
  Shell 解析 → 匹配 str_eq("help") → put("Commands:\n...")
```

### 阶段 E：`run file 1` — 子进程创建与回收

```
Shell 解析 "run file 1":
  → task_create(child_task_file, 1)   父 PID=1 (Shell)
    tasks[1].pid=2, parent_pid=1, state=READY
    tasks[1].ctx.ra = child_task_file
    返回 idx=1

  → 打印 "started child pid=2"
  → sys_wait() → ecall → SYS_WAIT
    扫描无 ZOMBIE 子进程 → SLEEPING → sched()

sched():
  保存 tasks[0] trapframe → scheduler() 循环
  找到 tasks[1] (READY) → swtch(&sched_ctx, &tasks[1].ctx)
  ★ 子进程 child_task_file() 开始执行

子进程:
  sys_fs_list() → 列出 3 个文件
  do_cat("hello.txt") → sys_open → fs_open → fd=3
                     → sys_read(fd=3) → fs_read → 输出内容
  do_cat("readme.txt")
  busy-wait 迭代 child_iters 次
  sys_exit(0) → ecall → SYS_EXIT
    state = TASK_ZOMBIE
    唤醒 parent (shell) state = READY
    sched() → 切回 scheduler

scheduler():
  找到 tasks[0] (READY) → swtch → Shell 恢复

Shell:
  sys_wait() 重新扫描 → 找到 ZOMBIE child pid=2
  回收 → 打印 "child pid=2 exited"
  put("myos> ") → 等待下一条命令
```

---

## 三、逐文件详解

### `kernel.ld` — 链接脚本

```ld
OUTPUT_ARCH(riscv)
ENTRY(_entry)                     ← QEMU 从 _entry 开始执行
SECTIONS {
    . = 0x80000000;               ← 起始地址 = QEMU virt RAM 基址
    .text : { *(.text.init) *(.text .text.*) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { __bss_start = .; ... __bss_end = .; }
    __end = .;                     ← 内核镜像结尾（物理页分配器起点）
    . = ALIGN(16);
    __stack_top = . + 0x20000;     ← 128KB 内核栈顶
}
```

**关键点**：

- `*(.text.init)` 把 `entry.S` 放到最前面（通过 `.section .text.init`）
- `__end` 是 bump allocator 的起始地址
- `__stack_top` 是启动栈的栈顶（向下增长）

### `Makefile` — 构建系统

```makefile
TOOLPREFIX = riscv64-unknown-elf-
CFLAGS = -Wall -Werror -O2 -nostdlib -nostartfiles -ffreestanding
         -march=rv64ima_zicsr -mabi=lp64 -mcmodel=medany
K_OBJS = kernel/entry.o kernel/swtch.o kernel/uart.o kernel/vm.o
         kernel/proc.o kernel/trap.o kernel/syscall.o kernel/start.o
         kernel/fs.o kernel/files_embed.o kernel/console.o
U_OBJS = user/shell.o
```

**关键点**：

- `-nostdlib -nostartfiles -ffreestanding`：裸机编译，无标准库
- `-march=rv64ima_zicsr`：RISC-V 64 位整数 + 乘除 + 原子 + Zicsr 扩展
- `-mcmodel=medany`：代码模型，支持 2GB 寻址范围
- `kernel/files_embed.o` 由 `files_embed.S` 生成，内含嵌入文件数据

### `kernel/entry.S` — CPU 启动入口（25 行汇编）

```
.section .text.init        ← 链接脚本确保此段在最前面
_entry:
    csrr t0, mhartid       ← 读核编号
    bnez t0, park          ← 非 0 核 → 休眠
    la t0, __bss_start     ← BSS 清零循环
    la t1, __bss_end
1:  bge t0, t1, 2f
    sd zero, (t0)
    addi t0, t0, 8
    j 1b
2:  la sp, __stack_top     ← 设栈指针
    j start                 ← ★ 跳 C 入口
park:
    wfi; j park
```

**为什么需要 BSS 清零**：全局变量（如 `tasks[]`, `ticks`）在 `.bss` 段中，链接器只记录了起止地址。CPU 上电时 RAM 内容是随机的，必须手动清零。

### `kernel/start.c` — C 语言内核入口（~85 行）

```c
__attribute__((noinline))
void start(void) {
    uart_init();                          // 串口就绪
    uart_puts(banner);                    // 打印横幅
    kalloc_init(); kvminit(); vmprint();  // 页表
    trap_init();                          // 陷阱向量 + 定时器
    fs_init(); console_init();            // 文件系统 + 控制台
    fs_register(...);                     // 注册 3 个嵌入文件
    task_create(shell_main, 0);           // 创建 Shell (pid=1)
    scheduler();                          // ★ 进入调度器，永不返回
}
```

初始化顺序是严格的：UART 必须最先（否则无法打印），页表在陷阱之前（trap_vector 地址需要通过 MMU），调度器最后（它接管控制权）。

### `kernel/swtch.S` — 上下文切换（30 行汇编）

```asm
swtch:                    # void swtch(struct context *old, struct context *new)
    sd ra, 0(a0)          # 保存 14 个 registers 到 *old
    sd sp, 8(a0)
    sd s0, 16(a0)
    ...                   # s1-s11
    ld ra, 0(a1)          # 从 *new 加载 14 个 registers
    ld sp, 8(a1)
    ld s0, 16(a1)
    ...
    ret                   # ★ 跳到 new->ra
```

这段代码**与 xv6 的 `swtch.S` 逐条指令完全相同**。保存 callee-saved 寄存器（RISC-V ABI 要求被调用者保存的 ra/sp/s0-s11），加载新任务的对应寄存器，`ret` 跳转到新任务的 ra。

### `kernel/uart.c` — NS16550A 串口驱动（~80 行）

| 函数             | 功能                                           |
| ---------------- | ---------------------------------------------- |
| `uart_init()`    | 初始化 38400 baud, 8N1, 关中断                 |
| `uart_putc(c)`   | 忙等 LSR bit5(THRE) → 写 THR                   |
| `uart_puts(s)`   | 逐字符输出，`\n` → `\r\n`                      |
| `uart_getc()`    | 非阻塞读：LSR bit0(DR)=1 则读 RHR，否则返回 -1 |
| `uart_puthex(x)` | 输出 64 位十六进制                             |
| `uart_putdec(n)` | 输出十进制整数                                 |

**MMIO 寄存器地址**：`UART_BASE = 0x10000000`，各寄存器偏移见代码注释。

### `kernel/vm.c` — Sv39 三级页表（~170 行）

**核心数据结构**：Sv39 虚拟地址 = VPN[2]:VPN[1]:VPN[0]:offset (9:9:9:12 bits)

| 函数                             | 功能                                 |
| -------------------------------- | ------------------------------------ |
| `walk(pt, va, alloc)`            | L2→L1→L0 三级遍历，等价 CPU MMU 硬件 |
| `mappages(pt, va, sz, pa, perm)` | 建立 VA→PA 逐页映射                  |
| `kvminit()`                      | 身份映射 2MB RAM + UART + CLINT      |
| `kalloc_page()`                  | bump allocator，从 `__end` 顺序分配  |
| `vmprint()`                      | 递归打印页表树状结构                 |

**关键细节**：`kvminit()` 写 `satp` CSR 后调用 `sfence_vma` 刷新 TLB。此后所有内存访问都经过 MMU 翻译。

### `kernel/proc.h` — 任务控制块（~45 行）

```c
struct context {                       // swtch.S 的 14 寄存器快照
    unsigned long ra, sp, s0-s11;
};

typedef struct {
    unsigned long entry;               // 任务函数地址
    struct context ctx;                // swtch 上下文
    unsigned long ksp;                 // ★ trap_vector 入口 sp
    unsigned char stack[4096];         // 独立栈
    unsigned char trapframe[256];      // ★ 陷阱帧快照（27 个寄存器 + mepc）
    int state;                         // READY/RUNNING/SLEEPING/ZOMBIE
    int pid;
    int parent_pid;                    // 父进程 PID（wait 回收）
    int exit_code;
    int sleep_until;
} Task;

enum { TASK_READY, TASK_RUNNING, TASK_SLEEPING, TASK_ZOMBIE };
```

**关键字段说明**：

- `ksp`：`trap_vector` 入口 sp（`sp - 256`）。`trap_ret` 从此地址加载寄存器。这是 MY-OS 特有的设计——xv6 用 `p->kstack + PGSIZE` 作为陷阱栈顶。
- `trapframe[31]`：保存 `mepc`（ecall 返回地址）。其他 27 个寄存器占据 trapframe[0..26]。
- `stack[4096]`：每任务独立 4KB 内核栈。预留顶部 256 字节给陷阱帧。

### `kernel/proc.c` — 进程管理（~70 行）

**`task_create(func, parent_pid)`**：

1. 分配 `tasks[task_count]` 槽位
2. 设置 pid = task_count + 1, parent_pid, state = READY
3. 初始化 `ctx.ra = func`, `ctx.sp = stack_top - 256`, `ksp = ctx.sp`
4. 清零 trapframe

**`fork()`**：

1. 在 `tasks[task_count]` 创建子任务
2. 复制父进程的 trapframe → 子进程 trapframe
3. 设置子进程 `trapframe[8] = 0`（fork 返回 0 给子进程）
4. 复制父进程的 ksp 区域（256 字节栈帧） → 子进程 ksp
5. 设置 `child->ctx.ra = trap_ret`（首次调度时从 trap_ret 开始执行 → `mret` 从 trapframe 恢复）
6. 父进程获得 `task_count`（子进程的槽位索引），转换后返回 PID

### `kernel/trap.c` — 陷阱处理 + 调度器（~185 行）

**架构总览**：

```
trap_vector (naked 汇编)
  ├── sp -= 256, 保存 27 寄存器 → trap_sp 快照
  ├── call machine_trap (naked stub)
  │     └── call machine_trap_c (C helper)
  │           ├── t->ksp = trap_sp
  │           ├── 复制 256B → t->trapframe
  │           ├── 读 mcause
  │           │   ├── timer IRQ (bit63=1, code=7) → ticks++, timer_set_next
  │           │   └── ecall (cause=11) → 存 mepc+4 → handle_syscall
  │           └── return
  ├── jr trap_ret              ← ★ 绕过 C epilogue
trap_ret (汇编标签)
  ├── sp = trap_sp             ← 从全局变量恢复 sp
  ├── ld 27 寄存器从栈
  ├── ld t0, 248(sp); csrw mepc, t0
  ├── sp += 256
  └── mret                     ← ★ 返回被中断的代码
```

**`machine_trap` 为什么是 naked 函数？** 编译器为非 naked 函数生成的序言/尾声会改变 sp 和 gp，而 trap handler 必须在 `trap_sp` 上操作。naked + 手动 `addi sp,-64 / sd ra / call / ld ra / addi sp,64` 保证控制流精确可控。

**`trap_ret` 为什么要从 `trap_sp` 重新加载 sp？** 因为 `machine_trap_c` 是 C 函数，其栈帧在 `trap_sp + 256` 之上（trap_vector 在栈上分配了 256 字节）。正常 C 返回后 sp 指向 C 函数栈帧而非 trap_vector 的保存区。必须显式恢复。

**scheduler() — xv6 风格调度循环**：

- 在独立栈 `sched_stack[4096]` 上运行
- 循环：唤醒定时睡眠任务 → 找 READY 任务 → swtch(&sched_ctx, &task.ctx)
- 全部 SLEEPING 时 `wfi` 等待中断
- 全部 DEAD 时 `[HALT]` + `wfi` 永久休眠

**sched() — 当前任务让出 CPU**：

- 保存当前任务 ksp → trapframe
- `swtch(&p->ctx, &sched_ctx)` → 回到 scheduler 循环

### `kernel/syscall.c` — 系统调用分发（~210 行）

**入口**：`handle_syscall()`，从 `t->trapframe[15]` 读 `a7`（syscall 号），switch 分发。

**13 个系统调用**（详见[系统调用表](#五系统调用表)）。

**返回值机制**：所有返回值写入 `t->ksp[8]`（即 trap_vector 栈上的 a0 位置）。`trap_ret` 恢复寄存器时 `ld a0, 64(sp)` 读取此值 → 用户态 `ecall` 的 `"=r"(a0)` 约束拿到返回值。

**关键设计决策**：

- `SYS_EXIT`/`SYS_SLEEP`/`SYS_YIELD`/`SYS_WAIT` 在 switch 内调用 `sched()` → 直接切回 scheduler，不返回
- `SYS_WRITE` fd=1 时逐字节输出 UART（含 `\n`→`\r\n` 转换）
- `SYS_READ` fd=0 → 路由到 `console_read()`
- `SYS_OPEN` 识别 `"console"` 字符串 → 返回 fd=0
- `SYS_FORK` → `fork()` 复制 trapframe+stack → 父子共享代码和数据

### `kernel/console.c` — 控制台驱动（~90 行）

**设计**：简化版 xv6 console 驱动。行缓冲输入，回显控制字符。

| 函数                     | 功能                                             |
| ------------------------ | ------------------------------------------------ |
| `console_init()`         | 初始化缓冲索引                                   |
| `console_intr(c)`        | 处理字符：回显 + 退格/Ctrl-U/换行 + 存入环形缓冲 |
| `console_read(dst, max)` | 忙等到 `cons.w != cons.r`，逐字节复制到用户空间  |

**环形缓冲结构**：

```c
struct { char buf[128]; uint r, w, e; } cons;
// r = 读游标, w = 已确认行尾游标, e = 编辑游标
// 用户输入 → e 前进 → 换行时 w = e → console_read 从 r 读到 w
```

**忙等策略**：单核 M-mode 裸机，无其他任务竞争 CPU。`console_read` 的 `while(cons.w == cons.r)` 循环每圈读一次 UART LSR 寄存器。无数据时返回 -1 → 继续循环。定时器中断照常触发（`ticks++`），但不触发调度。

### `kernel/fs.c` + `kernel/fs.h` — 嵌入文件系统（~120 行）

**原理**：`.incbin` 汇编指令在编译时将外部文件二进制内容直接嵌入 `.rodata` 段。内核通过符号名 `_binary_xxx_start/_end` 访问。

**数据结构**：

```c
EmbeddedFile { char name[32]; const char *data; unsigned long size; }
FileDesc     { int file_id; int offset; int valid; }
```

| 函数                            | 功能                                                      |
| ------------------------------- | --------------------------------------------------------- |
| `fs_init()`                     | 清零文件表和 fd 表                                        |
| `fs_register(name, data, size)` | 注册嵌入文件                                              |
| `fs_open(name)`                 | 名称匹配 → 分配 fd（从 3 开始，0/1/2 留给 stdin/out/err） |
| `fs_read(fd, buf, max)`         | 按 offset 读取数据                                        |
| `fs_close(fd)`                  | 释放 fd                                                   |
| `fs_list()`                     | 打印所有文件                                              |

### `kernel/files_embed.S` — 文件嵌入（~25 行汇编）

```asm
.section .rodata
_binary_files_hello_txt_start:
    .incbin "files/hello.txt"
_binary_files_hello_txt_end:
# 同样处理 readme.txt 和 test.txt
```

`make` 时 assembler 将文件内容直接嵌入 `.o` 文件，链接器放入最终 `kernel.elf` 的 `.rodata` 段。

### `user/shell.c` — 交互式 Shell（~250 行）

**纯 syscall 实现**：所有 I/O 通过 `sys_write(1, ...)` 和 `sys_read(0, ...)` 完成，不直接访问 UART MMIO。

**命令实现**：

- `help` → `put()` 打印帮助文本
- `ls` → `sys_fs_list()` 列出文件
- `cat <name>` → `sys_open` + `sys_read` 循环 + `sys_write`
- `run <task> [N]` → `task_create(func, sys_getpid())` + `sys_wait()` 循环
- `fork` → `sys_fork()` → 父子分支
- `exit` → 退出循环 → 打印 `[HALT]` → `wfi` 永久休眠

**`readline()` 实现**：`sys_read(0, buf, 64)` → `console_read()`。控制台驱动负责回显和行缓冲，Shell 只需解析返回的字符串。

---

## 四、内存布局

```
QEMU virt RAM (32MB, 起始 0x80000000)
────────────────────────────────────────────
0x80000000  .text          内核代码（entry.S → start.c → ...）
            .rodata        只读数据（嵌入文件、字符串常量）
            .data          已初始化全局变量
            .bss           未初始化全局变量 (ticks, tasks[], trap_sp...)
            __end          ← bump allocator 起点
            ...            页表、物理页
            __stack_top    ← 启动栈顶 (128KB)
────────────────────────────────────────────
0x80210000  tasks[0].stack  Shell 独立栈 (4KB)
            tasks[1].stack  Child 独立栈 (4KB)
            ...
            sched_stack     调度器独立栈 (4KB)
────────────────────────────────────────────
```

---

## 五、系统调用表

| #   | 宏          | 功能        | 参数 (a0,a1,a2) | 返回值      | xv6 对应 |
| --- | ----------- | ----------- | --------------- | ----------- | -------- |
| 1   | SYS_WRITE   | 写文件/串口 | fd, buf, len    | 写入字节数  | write    |
| 2   | SYS_YIELD   | 让出 CPU    | —               | 0           | yield    |
| 3   | SYS_GETPID  | 获取 PID    | —               | pid         | getpid   |
| 4   | SYS_UPTIME  | 系统 tick   | —               | ticks       | uptime   |
| 5   | SYS_SLEEP   | 睡眠        | ticks           | 0           | sleep    |
| 6   | SYS_OPEN    | 打开文件    | name, flags     | fd          | open     |
| 7   | SYS_READ    | 读取        | fd, buf, len    | 读取字节数  | read     |
| 8   | SYS_CLOSE   | 关闭        | fd              | 0           | close    |
| 9   | SYS_FS_LIST | 列出文件    | —               | 0           | —        |
| 10  | SYS_EXIT    | 进程退出    | code            | —           | exit     |
| 11  | SYS_WAIT    | 等待子进程  | —               | child_pid   | wait     |
| 12  | SYS_GETCHAR | 读 UART     | —               | char 或 -1  | —        |
| 13  | SYS_FORK    | 复制进程    | —               | child_pid/0 | fork     |

---

## 六、与 xv6 对照

| 方面       | xv6                                              | MY-OS                                                                      |
| ---------- | ------------------------------------------------ | -------------------------------------------------------------------------- |
| 运行级别   | S-mode (核) + U-mode (用户)                      | M-mode only（教学简化）                                                    |
| 进程创建   | `fork()` 复制完整地址空间                        | `task_create(func)` + `fork()` 复制栈帧                                    |
| 地址空间   | 每进程独立页表                                   | 全局单页表，身份映射                                                       |
| 上下文切换 | `swtch.S` 14 寄存器                              | **逐条指令完全相同**                                                       |
| 调度器     | `scheduler()` per-CPU + `sched()`                | `scheduler()` + `sched()`（相同架构）                                      |
| 陷阱入口   | `trampoline.S` + `kernelvec.S` + `usertrap()`    | `trap_vector` (naked) + `machine_trap` (naked stub) + `machine_trap_c` (C) |
| 控制台     | `console.c` ~300 行（interrupt-driven）          | `console.c` ~90 行（polling）                                              |
| 文件系统   | `fs.c` ~800 行（buffer cache + logging + inode） | 嵌入 `.incbin` FS + FAT12/16 读取器 (~290 行)                              |
| 磁盘驱动   | virtio-blk (interrupt-driven)                    | virtio-blk (polling MMIO, MBR 分区解析)                                    |
| VFS        | `file.c` FD_INODE/FD_PIPE/FD_DEVICE              | `file.c` FD_INODE/FD_FAT/FD_DEVICE + `devsw[]`                             |
| 进程 FD    | per-process `ofile[NOFILE]`                      | 同 xv6，`fork()` 时 `filedup()` 复制                                       |
| UART       | interrupt-driven TX/RX with spinlock             | polling RX, direct TX, no lock                                             |
| 代码量     | ~8500 行                                         | ~2200 行                                                                   |

**MY-OS 独有而 xv6 没有的特性**：

- `.incbin` 嵌入文件系统（读取 Windows 主机文件）
- VirtIO MMIO 自动探测 + virtio-blk 轮询驱动
- FAT12/16 BPB 解析、MBR 分区表读取、8.3 目录匹配、跨簇文件读取
- QEMU VVFAT 集成：直接读取 Windows `files/` 目录文件
- `devsw[]` 设备开关表，控制台注册为 `devsw[DEV_CONSOLE]`
- 每进程 `ofile[16]` 文件描述符表，`fork()` 自动 `filedup()`
- M-mode 直接 MMIO UART 读写（无需 SBI）
