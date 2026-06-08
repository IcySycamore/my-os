// kernel/start.c — Kernel entry point (xv6 main.c + proc.c userinit)
#include "proc.h"
#include "fs.h"
#include "file.h"

// External from other kernel files
void uart_init(void);
void uart_puts(const char *s);
void uart_puthex(unsigned long x);
void uart_putc(char c);
void kalloc_init(void);
void kvminit(void);
void vmprint(void);
void trap_init(void);
void fs_init(void);
int  fs_register(const char *name, const char *data, unsigned long size);
void scheduler(void);
void console_init(void);
void fileinit(void);
void virtio_blk_init(void);
int  virtio_blk_read(unsigned long sector, char *buf);
void fat_init(int (*read_fn)(unsigned long sector, char *buf));

// External from user/init.c + user/shell.c
void init_main(void);
void test_fork_main(void);

// External embedded file symbols (from files_embed.S)
extern char _binary_files_hello_txt_start[];
extern char _binary_files_hello_txt_end[];
extern char _binary_files_readme_txt_start[];
extern char _binary_files_readme_txt_end[];
extern char _binary_test_txt_start[];
extern char _binary_test_txt_end[];

// Utility (xv6 printf.c)
void uart_putdec(int n)
{
    char b[12]; int i = 0;
    if (n < 0) { uart_putc('-'); n = -n; }
    if (n == 0) { uart_putc('0'); return; }
    while (n > 0) { b[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) uart_putc(b[--i]);
}

__attribute__((noinline))
void start(void)
{
    uart_init();
    uart_puts("\n================================================\n");
    uart_puts("  MY-OS v7.1\n");
    uart_puts("  kernel/: entry.o swtch.o uart.o vm.o proc.o trap.o\n");
    uart_puts("           syscall.o start.o fs.o files_embed.o console.o file.o\n");
    uart_puts("           virtio_blk.o fat.o sysfile.o hostfile.o\n");
    uart_puts("  user/:   init.o shell.o ulib.o guess.o worm.o typewriter.o\n");
    uart_puts("           catall.o tree.o race.o\n");
    uart_puts("================================================\n\n");

    // Reset PID space — clean start
    next_pid = 1;

    // Lab 3: 页表
    kalloc_init();
    kvminit();
    uart_puts("[init] Sv39 paging enabled\n");
    vmprint();

    // Lab 4: 陷阱
    trap_init();
    uart_puts("[init] trap vector installed\n");

    // v5.1: VFS + 控制台 + devsw + virtio-blk
    fileinit();
    fs_init();
    console_init();
    uart_puts("[init] console ready\n");
    virtio_blk_init();
    uart_puts("[init] virtio done, calling fat_init...\n");
    fat_init(virtio_blk_read);
    uart_puts("[init] fat_init returned\n");

    // Register console device in devsw[]
    {
        extern struct devsw devsw[];
        extern int console_read(char *dst, int max);
        extern int console_write_raw(char *addr, int n);
        devsw[DEV_CONSOLE].read  = console_read;
        devsw[DEV_CONSOLE].write = console_write_raw;
    }

    // Pre-allocate stdin/stdout/stderr & assign to Init (pid=1)
    //exec(test_fork_main);
    exec(init_main);  // init
    for (int i = 0; i < 3; i++) {
        extern struct file* filealloc(void);
        struct file *f = filealloc();
        f->type = FD_DEVICE;
        f->major = DEV_CONSOLE;
        f->readable = 1;
        f->writable = 1;
        tasks[0].ofile[i] = f;
    }
    fs_register("hello.txt",
        _binary_files_hello_txt_start,
        (unsigned long)(_binary_files_hello_txt_end - _binary_files_hello_txt_start));
    fs_register("readme.txt",
        _binary_files_readme_txt_start,
        (unsigned long)(_binary_files_readme_txt_end - _binary_files_readme_txt_start));
    fs_register("test.txt",
        _binary_test_txt_start,
        (unsigned long)(_binary_test_txt_end - _binary_test_txt_start));
    uart_puts("[init] embedded file system ready (");
    uart_putdec(file_count);
    uart_puts(" files)\n");

    uart_puts("[init] Init task created (pid=1)\n\n");

    // xv6-style: enter scheduler loop, never returns
    scheduler();


    while (1);
}
