// kernel/fs.c — Embedded file registry (no fd layer — that's in file.c)
#include "fs.h"

void uart_puts(const char *s);
void uart_putc(char c);
void uart_putdec(int n);

EmbeddedFile file_table[MAX_FILES];
int          file_count = 0;

void fs_init(void)
{
    file_count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        file_table[i].data = 0;
        file_table[i].size = 0;
        file_table[i].name[0] = 0;
    }
}

int fs_register(const char *name, const char *data, unsigned long size)
{
    if (file_count >= MAX_FILES) return -1;
    EmbeddedFile *f = &file_table[file_count];
    int i = 0;
    while (name[i] && i < FILE_NAME_MAX - 1)
        { f->name[i] = name[i]; i++; }
    f->name[i] = 0;
    f->data = data;
    f->size = size;
    return file_count++;
}

int fs_find(const char *name)
{
    for (int i = 0; i < file_count; i++) {
        const char *a = name, *b = file_table[i].name;
        int match = 1;
        while (*a && *b) {
            if (*a != *b) { match = 0; break; }
            a++; b++;
        }
        if (match && *a == 0 && *b == 0)
            return i;
    }
    return -1;
}

int fs_read_raw(int id, char *buf, int maxlen, int offset)
{
    if (id < 0 || id >= file_count) return -1;
    EmbeddedFile *f = &file_table[id];
    if (!f->data) return -1;

    int remain = (int)(f->size - offset);
    if (remain <= 0) return 0;
    int n = (maxlen < remain) ? maxlen : remain;
    for (int i = 0; i < n; i++)
        buf[i] = f->data[offset + i];
    return n;
}

void fs_list(void)
{
    uart_puts("\n=== Embedded Files (");
    uart_putdec(file_count);
    uart_puts(" files) ===\n");
    for (int i = 0; i < file_count; i++) {
        uart_puts("  [");
        uart_putdec(i);
        uart_puts("] ");
        uart_puts(file_table[i].name);
        uart_puts("  (");
        uart_putdec((int)file_table[i].size);
        uart_puts(" bytes)\n");
    }
    uart_puts("===============================\n\n");
}
