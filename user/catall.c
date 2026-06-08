// user/catall.c — List all files (embedded + FAT disk)
#include "user.h"

void catall_main(void)
{
    put("\n  [catall] === All Files ===\n");
    sys_fs_list();  // now also lists FAT disk entries
    put("  [catall] Done.\n\n");
    sys_exit(0);
}
