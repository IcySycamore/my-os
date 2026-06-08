# 构建命令

# build
wsl sh -c "cd /mnt/d/PROJECT/OS/my-os && make"
# 清理
wsl sh -c "cd /mnt/d/PROJECT/OS/my-os && rm -f kernel.elf kernel/syscall.o && make 2>&1"
# 下电
wsl sh -c "pkill -9 qemu 2>/dev/null; echo done"
# 跑
wsl sh -c "cd /mnt/d/PROJECT/OS/my-os && make qemu"
wsl sh -c "cd /mnt/d/PROJECT/OS/my-os && qemu-system-riscv64 -machine virt -cpu rv64 -bios none -kernel kernel.elf -m 64M -nographic -global virtio-mmio.force-legacy=false -drive file=fat:rw:files,format=raw,if=none,id=drive0 -device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0" 2>&1