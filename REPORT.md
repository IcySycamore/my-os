# MY-OS v7.1 项目报告

## 系统结构

RISC-V M-mode 教学操作系统，~2300 行 C+汇编，QEMU virt 平台，参考 MIT 6.S081 xv6。

内核层：Sv39 页表（身份映射）→ 陷阱向量（timer+ecall）→ 调度器（sched/scheduler+swtch.S）→ syscall 分发 → VFS。

用户层：Init(pid=1) → Shell + ulib + 6 个 Demo。

驱动层：UART 轮询、virtio-blk MMIO。

## 技术路线

make qemu 一键运行。pkill -9 qemu 一键下电（模拟物理关机）。

文件 I/O 四路：嵌入 FS（.incbin 编译时嵌入）→ FAT 磁盘（disk/ 目录构建 fs.img）→ 宿主文件（semihosting 实时读写 files/ 沙箱）→ 控制台设备。

fat_init(virtio_blk_read) 函数指针 seam 解耦 FAT 格式与硬件。VFS 按 file->type（FD_INODE / FD_FAT / FD_DEVICE / FD_HOST）分发，devsw[] 设备开关表统一设备 I/O。

进程通过 exec()/fork() + filedup() 管理，sched() 协作让出，scheduler 独立栈轮询。孤儿进程在父进程 exit 时自动 reparent 给 init(pid=1)。

## 版本记录

v1 系统调用：ecall 指令 + handle_syscall() switch 分发，WRITE/READ/GETPID 等基础调用。

v2 用户调用封装：sys\_\*() ecall 桩函数（内联汇编）、user.h + ulib.c 共享库。

v3 页表：Sv39 三级 walk + mappages + kalloc bump allocator，satp 写后 sfence.vma。

v4 调度：swtch.S（28 指令同 xv6）+ sched/scheduler 分离 + 四态（READY/RUNNING/SLEEPING/ZOMBIE）。

v5 Shell：init→shell 架构。init(pid=1) fork 出 Shell 后 wait 循环，Shell exit 时自动重启。exec() 创建子进程 + sys_wait() 回收。修复 fork/wait 被唤醒后未重扫 zombie 以及孤儿进程无人回收的 bug。

v6 文件系统：嵌入 FS（.incbin 3 个文件编译进内核）+ FAT16 读取器（支持 FAT12/32，通过构建参数切换）+ virtio-blk 轮询驱动。FAT 镜像从 disk/ 目录构建，运行时不可同步修改。为支持实时读写，新增 FD_HOST 类型 + semihosting 设备，宿主文件通过 files/ 沙箱用宿主机文件 API 读写，运行时新建或修改文件立刻对 OS 可见。宿主文件不在 ls 列表中，通过 cat 按名访问。

v7 VFS 统一：file.c 四路分发（FD_INODE / FD_FAT / FD_DEVICE / FD_HOST）+ devsw[4] 设备开关表 + per-process ofile[] + filedup 引用计数共享。sysfile_open() 独立模块，fat_disk_read 函数指针 seam 可注入测试 fake。

## 项目特色

- 完整 OS 骨架：2300 行覆盖页表→陷阱→调度→syscall→VFS→驱动全链路
- 磁盘 I/O：手写 virtio-blk MMIO 驱动，原生 FAT16 解析，直接读 Windows 宿主机文件
- 宿主文件实时同步：semihosting FD_HOST，运行时改宿主文件立即可见，files/ 沙箱隔离
- xv6 同款调度：swtch.S 逐指令一致，sched()+scheduler() 分离架构，exec()/fork() filedup 语义
- init 守护进程：Shell 退出自动重启，孤儿进程 reparent 回收，真正 xv6 风格进程管理
- 教学友好：make 一键编译运行，pkill 一键下电，6 个 Demo 覆盖猜数/动画/并发/文件操作
