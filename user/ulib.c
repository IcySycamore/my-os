// user/ulib.c — Shared utility functions for user programs
#include "user.h"

int slen(const char *s) { int n=0; while(s[n])n++; return n; }

void put(const char *s) { sys_write(1, s, slen(s)); }

void putdec(int n)
{
    char buf[12]; int i = 0;
    if (n < 0) { put("-"); n = -n; }
    if (n == 0) { put("0"); return; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) { char s[2] = { buf[--i], 0 }; put(s); }
}

int atoi2(const char *s, int len)
{
    int val = 0;
    for (int i = 0; i < len && s[i] >= '0' && s[i] <= '9'; i++)
        val = val * 10 + (s[i] - '0');
    return val;
}

void readline(char *buf, int max)
{
    int n = sys_read(0, buf, max);
    if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = 0;
    else buf[n > 0 ? n : 0] = 0;
}

void do_cat(const char *name)
{
    char buf[128]; int n;
    int fd = sys_open(name, 0);
    if (fd < 0) { put("  Error: file not found\n"); return; }
    while ((n = sys_read(fd, buf, sizeof(buf))) > 0)
        sys_write(1, buf, n);
    put("\n");
    sys_close(fd);
}

void do_write(const char *name, const char *text)
{
    int fd = sys_open(name, 0);
    if (fd < 0) { put("  Error: cannot open file for write\n"); return; }
    int len = slen(text);
    sys_write(fd, text, len);
    sys_close(fd);
    put("  wrote "); putdec(len); put(" bytes to "); put((char*)name); put("\n");
}
