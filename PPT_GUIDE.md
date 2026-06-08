# MY-OS 5 页 PPT 指南

---

## 第 1 页：项目概览

**MY-OS — 从零构建 RISC-V 教学操作系统**

- 平台：QEMU RISC-V `virt`，RV64IMA_Zicsr，M-mode only，~2300 行
- 参考：MIT 6.S081 xv6（Lab 2/3/4/6）
- Sv39 三级页表 + 身份映射
- xv6 风格 sched/scheduler + swtch.S 协作调度（13 个 syscall）
- VFS 三层分发（嵌入 FS + FAT12/16 磁盘 + 设备）
- virtio-blk 轮询驱动 + QEMU VVFAT 集成
- 交互式 Shell + 6 个 Demo（guess/worm/typewriter/catall/tree/race）

---

## 第 2 页：系统架构

```
Shell (pid=1)  ──ecall──→  syscall.c (13 syscalls)
                               │
                          sysfile_open()
                         ┌───┼───┐
                    FD_INODE  FD_FAT  FD_DEVICE
                       │        │        │
                    fs.c     fat.c   console.c
                    (.incbin)  │    (行缓冲)
                               │
                          fat_disk_read()  ← 函数指针 seam
                               │
                          virtio_blk.c (MMIO 轮询)
                               │
                          QEMU VVFAT → files/
```

**关键数据流**：`sys_open` → `sysfile_open` 三路分发；`fat_read` 通过函数指针 seam 调用磁盘驱动；编译器屏障 `asm("":::"memory")` 防 -O2 优化死循环。

---

## 第 3 页：进程与调度

**四态模型**：READY → RUNNING → SLEEPING / ZOMBIE

- `exec(func)`：创建新任务，filedup 继承父进程 fd 0-2
- `fork()`：克隆 trapframe + ksp + ofile[]（filedup 共享 ftable）
- `sched()` → swtch(&task.ctx, &sched_ctx) → 回到 scheduler
- `scheduler()` 独立栈轮询：找 READY → swtch → 运行
- 全部 SLEEPING 时 `wfi`，无任务时写 SiFive test 触发 QEMU 关机

**trap_vector** (naked asm)：保存 27 寄存器 → machine_trap → 判断 mcause（定时器/ecall）→ jr trap_ret → mret。

---

## 第 4 页：FAT 磁盘 + VFS

**FAT 读取器**（fat.c）：

- MBR 解析 → 提取分区 LBA → 读 BPB → 判断 FAT12/16/32
- 8.3 文件名匹配 → 返回 start_cluster + file_size
- 沿 FAT 簇链逐簇读取

**virtio-blk**（virtio_blk.c）：

- MMIO 基址 0x10001000，3-desc 链（header + data + status）
- 编译器屏障轮询 `disk_used.idx`

**架构解耦**：`fat_init(virtio_blk_read)` — 注入函数指针 seam，格式逻辑与硬件驱动分离。

---

## 第 5 页：关键 Bug & 总结

| Bug           | 根因                                     | 修复                        |
| ------------- | ---------------------------------------- | --------------------------- |
| virtio 死循环 | -O2 缓存 `disk_used.idx` 到寄存器        | `asm("":::"memory")` 屏障   |
| FAT 读失败    | BPB 扇区号相对分区，缺 `part_offset`     | 所有偏移加 MBR LBA          |
| 栈溢出        | `cluster_buf[4096]` < 16 扇区/簇 (8192B) | 改为 32KB static            |
| ecall 返回错  | mepc+4 只写 trapframe，trap_ret 从栈读   | 同时写 `trap_sp` 位置       |
| wait 返回 -1  | 被唤醒后未重扫 zombie 列表               | `for(;;)` 包裹扫描+sched    |
| exit 不关机   | Shell 自己 wfi，scheduler 不知           | `sys_exit(0)` + SiFive test |

**总结**：2300 行实现完整 OS 骨架，xv6 风格陷阱/调度/VFS，独有 FAT+VVFAT 集成，FAT/disk 通过 seam 解耦可测试。
