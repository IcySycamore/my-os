// kernel/console.c — xv6-style console driver (simplified)
// Line-buffered input, interrupt-safe, sync output.

#include "proc.h"

void uart_putc(char c);
void uart_puts(const char *s);
int  uart_getc(void);

// ---- Console buffer ----
#define INPUT_BUF 128
static struct {
    char buf[INPUT_BUF];
    unsigned int r;  // read index
    unsigned int w;  // write index
    unsigned int e;  // edit index
} cons;

void console_init(void)
{
    cons.r = 0;
    cons.w = 0;
    cons.e = 0;
}

// Called from UART interrupt context (or polling context for us).
// Handles backspace (0x7F), ctrl-U (0x15), ctrl-D (0x04), newline.
void console_intr(int c)
{
    if (c == 0x04) {  // Ctrl-D (EOF)
        // No-op for now
        return;
    }

    switch (c) {
    case 0x15:  // Ctrl-U — kill line
        while (cons.e != cons.w &&
               cons.buf[(cons.e - 1) % INPUT_BUF] != '\n') {
            cons.e--;
            uart_putc('\b'); uart_putc(' '); uart_putc('\b');
        }
        break;

    case 0x7F:  // Backspace
    case '\b':
        if (cons.e != cons.w) {
            cons.e--;
            uart_putc('\b'); uart_putc(' '); uart_putc('\b');
        }
        break;

    default:
        if (c == '\r')
            c = '\n';

        // Echo
        uart_putc((char)c);

        // Store
        cons.buf[cons.e % INPUT_BUF] = (char)c;
        cons.e++;

        if (c == '\n') {
            cons.w = cons.e;       // Line complete — wake readers
        }

        // If buffer full, also wake reader
        if (cons.e == cons.w + INPUT_BUF)
            cons.w = cons.e;
        break;
    }
}

// Blocking read one line from console into user buffer.
int console_read(char *dst, int max)
{
    int i;
    for (i = 0; i < max - 1; ) {
        // Busy-poll UART — M-mode single core, no other work to do
        while (cons.w == cons.r) {
            int c = uart_getc();
            if (c >= 0)
                console_intr(c);
        }
        char c = cons.buf[cons.r % INPUT_BUF];
        cons.r++;
        if (c == '\n') { dst[i] = 0; return i; }
        dst[i++] = c;
    }
    dst[i] = 0;
    return i;
}

// Raw write to console (for devsw[] registration)
int console_write_raw(char *addr, int n)
{
    for (int i = 0; i < n; i++) {
        if (addr[i] == '\n')
            uart_putc('\r');
        uart_putc(addr[i]);
    }
    return n;
}
