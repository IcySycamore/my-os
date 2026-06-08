// user/typewriter.c — Typewriter FAT disk read
#include "user.h"

void typewriter_main(void)
{
    put("\n  === Typewriter Demo (FAT disk read) ===\n\n  ");

    int fd = sys_open("DISKONLY.TXT", 0);
    if (fd < 0) {
        put("  Cannot open DISKONLY.TXT — FAT disk missing?\n");
        sys_exit(0);
        return;
    }
    char ch;
    while (sys_read(fd, &ch, 1) > 0) {
        sys_write(1, &ch, 1);
        if (ch == ' ')      for (volatile int d = 0; d < 1000000; d++);
        else if (ch != '\n') for (volatile int d = 0; d < 2000000; d++);
    }
    put("\n\n  === End of typewriter ===\n\n");
    sys_close(fd);
    sys_exit(0);
}
