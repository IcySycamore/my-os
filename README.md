# MY-OS v7.1

> RISC-V M-mode 教学操作系统 | xv6 简化实现 | ~2300 行 C + 汇编

---

## 快速开始

```bash
# ========== 方式一：本地构建（推荐） ==========
# 前置条件: riscv64-unknown-elf 工具链 + QEMU riscv64
make          # → kernel.elf
make qemu     # → QEMU 启动

# ========== 方式二：Docker ==========
# 国内用户先配镜像加速: Docker Desktop → Settings → Docker Engine →
#   "registry-mirrors": ["https://docker.m.daocloud.io"]
docker build -t myos-build .
docker run --rm -v "$(pwd):/src" -w /src myos-build make
```

---

## 项目结构

```
my-os/
├── Makefile                # riscv64-unknown-elf-* 工具链
├── kernel.ld               # 链接脚本 (0x80000000)
├── Dockerfile              # 可重现构建环境
├── fs.img                  # FAT16 磁盘镜像
├── kernel/                 # 内核 (~1800 行)
│   ├── entry.S             # 启动入口 (BSS 清零 → start)
│   ├── start.c             # 内核初始化 (页表/陷阱/VFS/init进程)
│   ├── proc.c / proc.h     # 进程管理 (exec/fork/wait/exit)
│   ├── swtch.S             # 上下文切换 (callee-saved 寄存器)
│   ├── trap.c              # 陷阱处理 (ecall ISR + 调度器)
│   ├── syscall.c           # 系统调用分发 (13 个 syscall)
│   ├── vm.c                # Sv39 页表 (walk/mappages)
│   ├── file.c / file.h     # VFS 文件层 (x v6 file API)
│   ├── fs.c / fs.h         # 嵌入式文件系统
│   ├── files_embed.S       # 二进制嵌入 (hello.txt 等)
│   ├── virtio_blk.c        # VirtIO 块设备驱动
│   ├── fat.c               # FAT16 只读驱动
│   ├── hostfile.c          # Semihosting 主机文件读写
│   ├── sysfile.c           # 文件系统系统调用
│   ├── console.c           # 控制台设备 (devsw)
│   ├── uart.c              # NS16550 UART 驱动
│   ├── types.h / virtio.h  # 公用类型 & 常量
│   └── *.bak               # 备份文件
├── user/                   # 用户程序 (~500 行)
│   ├── ulib.c / user.h     # 用户库 & 系统调用包装
│   ├── init.c              # init 进程 (fork + exec shell)
│   ├── shell.c             # MY-OS Shell (交互式命令行)
│   ├── test_fork.c         # fork 最小诊断测试
│   ├── race.c              # 双线程竞速演示
│   ├── worm.c              # 动画进度条
│   ├── tree.c              # ASCII 树生长
│   ├── guess.c             # 猜数字游戏
│   ├── catall.c            # 全文件列表
│   ├── typewriter.c        # FAT 磁盘文件读取
│   └── tasks.c             # 早期任务 demo
├── disk/                   # FAT 磁盘源文件
│   ├── hello.txt / readme.txt / test.txt
│   └── DISKONLY.TXT
├── files/                  # Semihosting 测试文件
│   ├── HOSTTEST.TXT
```

---

## 架构

```
┌─────────────────────────────────────────────────┐
│  用户程序 (M-mode, 统一地址空间)                 │
│  shell / race / worm / guess / catall / tree    │
├─────────────────────────────────────────────────┤
│  系统调用 (ecall, mcause=11)                      │
│  SYS_FORK/EXEC/WAIT/EXIT/WRITE/READ/OPEN/CLOSE  │
├──────────────┬──────────────────────────────────┤
│  陷阱 & 调度  │  VFS 文件层                      │
│  trap_vector  │  file.c (FD_INODE/FD_DEVICE/     │
│  scheduler()  │          FD_FAT/FD_HOST)          │
│  swtch.S      │  fs.c (嵌入式) + fat.c (FAT16)   │
├──────────────┴──────────────────────────────────┤
│  进程管理 (proc.c)                               │
│  Task[8] / exec() / fork() / alloc_slot()       │
├─────────────────────────────────────────────────┤
│  Sv39 页表 (vm.c)                                │
│  0x80000000 起 4MB 恒等映射                      │
├─────────────────────────────────────────────────┤
│  UART / VirtIO / CLINT / Semihosting            │
│  QEMU virt 平台 (RV64IMA)                        │
└─────────────────────────────────────────────────┘
```

### 关键实现细节

| 功能       | 实现方式                                      |
| ---------- | --------------------------------------------- |
| 特权级     | M-mode only（无 U/S 委托）                    |
| 系统调用   | `ecall` → `mcause=11` → ksp[8]=返回值         |
| 调度器     | 协作式轮转（`sys_sleep` / `sys_yield`）       |
| Timer      | CLINT mtime 忙等轮询（ISR 方案见下方）        |
| 上下文切换 | `swtch.S` 保存/恢复 14 个 callee-saved 寄存器 |
| fork       | 全量拷贝 trapframe + ksp + 文件表             |
| 文件系统   | VFS 层：嵌入式 + FAT16 + semihosting          |
| 页表       | Sv39，内核 4MB 恒等映射 (RWX)                 |

### Timer ISR 为什么用轮询代替

RISC-V `ecall` 硬件自动清零 `mstatus.MIE`。进入 scheduler 后 MIE=0，Machine-mode timer 中断被屏蔽。xv6 无此问题因为委托到 S-mode 并用 `sstatus.SIE` 独立控制。

当前方案：scheduler idle 时直接轮询 `CLINT_MTIME` 硬件计数器自增 `ticks`。

---

## Shell 命令

```
help            帮助
ls              列出文件
cat <name>       打印文件
run <demo> [N]  运行演示 (guess, worm, typewr, catall, tree, race)
fork            Fork 父子进程测试
exit            退出 Shell
```

---

## 技术指标

| 指标       | 数值                         |
| ---------- | ---------------------------- |
| 总代码量   | ~2300 行 (C + 汇编)          |
| 最大任务数 | 8 (MAX_TASKS)                |
| 每任务栈   | 4KB                          |
| 文件描述符 | 16/进程                      |
| 系统调用   | 13 个                        |
| 用户程序   | 11 个                        |
| 支持存储   | 嵌入式 + FAT16 + Semihosting |

---

## 许可

MIT
