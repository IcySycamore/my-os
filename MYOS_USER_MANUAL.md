# MY-OS v5.2 — 用户手册

---

## 一、概述

MY-OS 是一个运行在 QEMU RISC-V 上的教学操作系统。支持 VFS（嵌入 FS + FAT12/16 磁盘 + 设备）、virtio-blk 驱动、交互式 Shell 及 6 个 Demo 程序。

### 系统规格

| 项目     | 说明                                     |
| -------- | ---------------------------------------- |
| 平台     | QEMU RISC-V `virt` (RV64IMA_Zicsr)       |
| 运行级别 | M-mode（单特权级）                       |
| 内存     | 64MB RAM @ 0x80000000                    |
| 串口     | NS16550A UART @ 0x10000000               |
| 磁盘     | virtio-blk MMIO, QEMU VVFAT              |
| 代码量   | ~2300 行 (C + 汇编)                      |
| 支持 FS  | 嵌入 FS (3 文件) + FAT12/16 外部磁盘读取 |

---

## 二、构建与启动

### 环境准备

```bash
# 需要安装 RISC-V 交叉编译工具链和 QEMU
# Ubuntu/Debian:
sudo apt install gcc-riscv64-unknown-elf qemu-system-misc
```

### 编译运行

```bash
cd my-os
make          # 编译 → kernel.elf
make qemu     # 启动 QEMU（含 VVFAT 虚拟磁盘）
make clean    # 清理
```

启动后你应该看到：

```
================================================
  MY-OS v7.1 — with fat16/32 File System
================================================
[init] Sv39 paging enabled
...
[init] trap vector installed
[virtio] disk ready
[fat] ready, type=FAT16
[init] embedded file system ready (3 files)
[init] Shell task created (pid=1)

========================================
  MY-OS Shell v4.2
  Type 'help' for commands
========================================

myos>
```

### files/ 目录

```
files/
├── hello.txt
├── readme.txt
├── test.txt
└── DISKONLY.TXT     ← 仅存于虚拟磁盘，不在嵌入 FS 中
```

---

## 三、Shell 命令参考

在 `myos>` 提示符下输入命令，回车执行。

### help — 显示帮助

```
myos> help
Commands:
  help              This help
  ls                List embedded files
  cat <name>         Print file
  run <task> [N]    Run task N times (default 3)
  fork              Fork → parent & child
  exit              Quit Shell
```

### ls — 列出嵌入文件

```
myos> ls
=== Embedded Files (3 files) ===
  [0] hello.txt  (335 bytes)
  [1] readme.txt  (793 bytes)
  [2] test.txt  (229 bytes)
==========================
```

这些文件来自 `kernel/files_embed.S`，通过 `.incbin` 汇编指令在编译时嵌入内核镜像。外部 FAT 磁盘文件可通过 `cat DISKONLY.TXT` 读取（见下文）。

### cat \<name\> — 查看文件内容（嵌入 FS 或 FAT 磁盘）

```bash
myos> cat hello.txt           # 嵌入文件
Hello from Windows host filesystem!
...

myos> cat DISKONLY.TXT        # FAT 虚拟磁盘文件
This file is only on the VVFAT disk, not embedded.
If you can read this, virtio-blk + FAT32 is working!
```

`cat` 自动路由：先查嵌入 FS（`fs_find`），未命中则查 FAT 磁盘（`fat_find`）。

### run \<demo\> [N] — 运行 Demo N 次

| Demo 名  | 说明                              |
| -------- | --------------------------------- |
| `guess`  | 交互式猜数字游戏 (1-100)          |
| `worm`   | 动画进度条 `@` 爬行               |
| `typewr` | 逐字读取 FAT 磁盘 DISKONLY.TXT    |
| `catall` | 列出所有嵌入 + FAT 文件并输出内容 |
| `tree`   | 生长 ASCII 树                     |
| `race`   | Fork A/B 竞速（并发调度）         |

```bash
myos> run guess        # 猜数字游戏
myos> run worm 2       # 跑 2 次进度条动画
myos> run tree 4       # 4 层高树
myos> run race         # A/B 竞速
```

### fork — 复制当前进程

```
myos> fork
[parent] fork returned pid=2
[child] fork returned 0, pid=3
[child] loop 1 tick=15
[child] loop 2 tick=15
[child] loop 3 tick=15
[child] exiting
[parent] child pid=3 reaped
```

父进程得到子进程的 PID（>0），子进程得到 0。

> **注意**：fork 后父子进程并发运行，UART 输出可能交叉（无锁竞态）。这是预期行为，不是 bug。

### exit — 退出系统

```
myos> exit
[Shell] Exiting. Goodbye!
========== System Halted ==========
[HALT] Powering off...
```

系统会自动关闭 QEMU。

---

## 四、架构速览

```
Shell (pid=1)          ← 6 条命令 + 6 个 Demo
  │  ecall
  ▼
syscall.c              ← 13 个系统调用分发
  │  sysfile_open()    ← 文件打开三路分发 (sysfile.c)
  ▼
VFS (file.c)           ← 统一分发
  ├─ FD_INODE  → 嵌入 FS (fs.c, 3 个 .incbin 文件)
  ├─ FD_FAT    → FAT12/16 读取器 (fat.c, 函数指针 seam)
  └─ FD_DEVICE → 控制台 (console.c, devsw[0])
  │
  ▼
virtio-blk (virtio_blk.c)  ← MMIO 轮询磁盘驱动
  │
  ▼
QEMU VVFAT → Windows 主机 files/ 目录
```

---

## 五、故障排查

### `make` 报错 "riscv64-unknown-elf-gcc not found"

未安装 RISC-V 工具链。在 WSL/Ubuntu 中执行 `sudo apt install gcc-riscv64-unknown-elf`。

### QEMU 启动后无输出

检查 `kernel.elf` 是否成功编译（`ls -la kernel.elf`），确保在 `my-os` 目录下执行命令。

### 输入字符无回显 / 终端卡住

- 确保在运行 `make qemu` 的同一个终端窗口中直接键入
- 不要通过其他程序（如 `send_to_terminal`）中转按键
- 不要 Ctrl+C QEMU 进程——用 `exit` 命令正常退出

### `run file 1` 执行到一半卡住

如果输出未完整显示就停止，通常是 UART 输出缓冲区还在排队。等待定时器中断（8M 周期）刷新即可。如果超过 5 秒无响应，检查 QEMU 进程是否 crash。
