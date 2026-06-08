# Makefile for my-os v7.1 — with embedded file system
TOOLPREFIX = riscv64-unknown-elf-
CC  = $(TOOLPREFIX)gcc
LD  = $(TOOLPREFIX)ld
CFLAGS  = -Wall -Werror -O2 -nostdlib -nostartfiles -ffreestanding
# Detect whether GCC needs explicit _zicsr extension (GCC 13+) or includes it (GCC 12-)
HAS_ZICSR := $(shell $(CC) -march=rv64ima_zicsr -mabi=lp64 -E -x c /dev/null -o /dev/null 2>/dev/null && echo 1 || echo 0)
ifeq ($(HAS_ZICSR),1)
  MARCH := rv64ima_zicsr
else
  MARCH := rv64ima
endif
CFLAGS += -march=$(MARCH) -mabi=lp64 -mcmodel=medany
CFLAGS += -I. -Ikernel

K_OBJS = kernel/entry.o kernel/swtch.o kernel/uart.o kernel/vm.o
K_OBJS += kernel/proc.o kernel/trap.o kernel/syscall.o kernel/start.o
K_OBJS += kernel/fs.o kernel/files_embed.o kernel/console.o kernel/file.o
K_OBJS += kernel/virtio_blk.o kernel/fat.o kernel/sysfile.o kernel/hostfile.o
U_OBJS = user/test_fork.o user/init.o user/shell.o user/ulib.o user/guess.o user/worm.o user/typewriter.o user/catall.o user/tree.o user/race.o
OBJS   = $(K_OBJS) $(U_OBJS)

all: kernel.elf

kernel.elf: $(OBJS) kernel.ld
	$(LD) -T kernel.ld -o $@ $(OBJS)

# files_embed.o depends on the actual embedded files
kernel/files_embed.o: disk/hello.txt disk/readme.txt disk/test.txt

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

qemu: kernel.elf fs.img
	qemu-system-riscv64 -machine virt -cpu rv64 \
		-bios none -kernel kernel.elf -m 64M -nographic \
		-global virtio-mmio.force-legacy=false \
		-semihosting \
		-drive file=fs.img,format=raw,if=none,id=drive0 \
		-device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0

# Build FAT16 disk image from disk/ directory
disk: fs.img

fs.img: $(wildcard disk/*)
	@echo "=== Building FAT16 disk image from disk/ ==="
	dd if=/dev/zero of=$@ bs=1M count=10 2>/dev/null
	mkfs.fat -F 16 $@ >/dev/null 2>&1
	mcopy -i $@ disk/* :: 2>/dev/null || (echo "ERROR: install mtools (sudo apt install mtools)" && rm -f $@ && exit 1)
	@echo "=== fs.img ready ==="

clean:
	rm -f *.o kernel/*.o user/*.o
	rm -f kernel.elf fs.img

.PHONY: all qemu clean disk
