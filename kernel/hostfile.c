// kernel/hostfile.c — Semihosting host file I/O (QEMU -semihosting)
//
// slli x0,x0,0x1f ; ebreak ; srai x0,x0,0x7
// QEMU intercepts the ebreak (not delivered to guest handler).
// a0=operation, a1=param_block
//
// All paths sandboxed to files/ directory.
//
// All paths sandboxed to files/ directory.

void uart_puts(const char *s);

static inline int semi_call(int op, void *param)
{
    register long r0 asm("a0") = op;
    register long r1 asm("a1") = (long)param;
    asm volatile(
        "slli x0, x0, 0x1f\n\t"
        ".option push\n\t"
        ".option norvc\n\t"
        "ebreak\n\t"
        "srai x0, x0, 0x7\n\t"
        ".option pop"
        : "+r"(r0), "+r"(r1) :: "memory");
    return (int)r0;
}

static const char* resolve_name(const char *name)
{
    static char path[64];
    const char *p = "files/";
    int i = 0;
    while (*p && i < 62) path[i++] = *p++;
    while (*name && i < 62) path[i++] = *name++;
    path[i] = 0;
    return path;
}

int hostfile_open(const char *name)
{
    const char *path = resolve_name(name);
    struct { const char *fname; unsigned long mode; unsigned long namelen; } param;
    param.fname = path;
    param.namelen = 0;
    while (path[param.namelen]) param.namelen++;
    // Try r+ first (don't truncate existing files)
    param.mode = 2;  // "r+"
    int fd = semi_call(0x01, &param);
    if (fd < 0) {
        // File doesn't exist — create it
        param.mode = 4;  // "wb+"
        fd = semi_call(0x01, &param);
    }
    return fd;
}

int hostfile_read(int fd, char *buf, int len)
{
    struct { unsigned long fd; char *buf; unsigned long len; } param;
    param.fd = fd;
    param.buf = buf;
    param.len = len;
    int not_read = semi_call(0x06, &param);
    // semihosting: 0=success, non-zero=bytes NOT read
    return (not_read == 0) ? len : (len - not_read);
}

int hostfile_write(int fd, char *buf, int len)
{
    struct { unsigned long fd; const char *buf; unsigned long len; } param;
    param.fd = fd;
    param.buf = buf;
    param.len = len;
    int not_written = semi_call(0x05, &param);
    // semihosting: 0=success, non-zero=bytes NOT written
    return (not_written == 0) ? len : (len - not_written);
}

int hostfile_size(int fd)
{
    struct { unsigned long fd; } param;
    param.fd = fd;
    return semi_call(0x0C, &param);
}

int hostfile_seek(int fd, int pos)
{
    struct { unsigned long fd; unsigned long pos; } param;
    param.fd = fd;
    param.pos = (unsigned long)(long)pos;
    return semi_call(0x0A, &param);
}

void hostfile_close(int fd)
{
    struct { unsigned long fd; } param;
    param.fd = fd;
    semi_call(0x02, &param);
}
