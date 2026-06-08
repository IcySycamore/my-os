/*
 * uart.c — NS16550A UART driver for QEMU virt (RISC-V).
 */

#define UART_BASE 0x10000000L

static inline void uart_reg_write(int offset, unsigned char val)
{
    *(volatile unsigned char *)(UART_BASE + offset) = val;
}

static inline unsigned char uart_reg_read(int offset)
{
    return *(volatile unsigned char *)(UART_BASE + offset);
}

// Initialize UART to 38400 baud, 8N1 (no interrupts — polling mode)
void uart_init(void)
{
    uart_reg_write(1, 0x00); // IER = 0 (disable interrupts)
    uart_reg_write(3, 0x80); // LCR: enable DLAB
    uart_reg_write(0, 0x03); // DLL = 3 (38400 baud @ 1.8432MHz)
    uart_reg_write(1, 0x00); // DLM = 0
    uart_reg_write(3, 0x03); // LCR: 8N1, clear DLAB
    uart_reg_write(2, 0x07); // FCR: enable + clear FIFOs
}

// Wait for transmitter ready, then output one character
void uart_putc(char c)
{
    while ((uart_reg_read(5) & (1 << 5)) == 0)
        ;
    uart_reg_write(0, c);
}

// Write a string, converting \n to \r\n for raw terminal
void uart_puts(const char *s)
{
    while (*s)
    {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

// Non-blocking read: returns char or -1 if no data
int uart_getc(void)
{
    if (uart_reg_read(5) & 1)
        return (int)((unsigned char)uart_reg_read(0));
    return -1;
}

// Blocking read: waits until a character is available
int uart_getc_blocking(void)
{
    while ((uart_reg_read(5) & 1) == 0)
        ;
    return (int)((unsigned char)uart_reg_read(0));
}

// Print a 64-bit hex value
void uart_puthex(unsigned long x)
{
    static const char hex[] = "0123456789abcdef";
    char buf[19];
    int i;
    buf[0] = '0';
    buf[1] = 'x';
    for (i = 17; i >= 2; i--)
    {
        buf[i] = hex[x & 0xf];
        x >>= 4;
    }
    buf[18] = '\0';
    uart_puts(buf);
}
