// kernel/sysfile.c — File-open dispatch (extracted from syscall.c)
// Resolves name across all backends: console, embedded FS, FAT disk, host.
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "hostfile.h"

int  fs_find(const char *name);
int  fat_find(const char *name, unsigned long *cluster, unsigned int *size);

// sysfile_open: resolve a filename and allocate a struct file.
// Returns the file (ref=1) or 0 on failure. Caller assigns fd.
struct file* sysfile_open(const char *name)
{
    struct file *f = 0;

    // 1. Console device (hardcoded name)
    if (name[0] == 'c' && name[1] == 'o' && name[2] == 'n' && name[3] == 's'
        && name[4] == 'o' && name[5] == 'l' && name[6] == 'e' && name[7] == 0) {
        f = filealloc();
        if (!f) return 0;
        f->type = FD_DEVICE;
        f->major = DEV_CONSOLE;
        f->readable = 1;
        f->writable = 1;
        return f;
    }

    // 2. Embedded file (fs.c)
    int id = fs_find(name);
    if (id >= 0) {
        f = filealloc();
        if (!f) return 0;
        f->type = FD_INODE;
        f->embfile_id = id;
        f->readable = 1;
        f->writable = 0;
        return f;
    }

    // 3. FAT disk file (fat.c)
    {
        unsigned long fat_cluster;
        unsigned int  fat_size;
        if (fat_find(name, &fat_cluster, &fat_size) == 0) {
            f = filealloc();
            if (!f) return 0;
            f->type = FD_FAT;
            f->fat_start_cluster = fat_cluster;
            f->fat_file_size = fat_size;
            f->readable = 1;
            f->writable = 0;
            return f;
        }
    }

    // 4. Host file (semihosting, files/ sandbox)
    {
        int hfd = hostfile_open(name);
        if (hfd >= 0) {
            f = filealloc();
            if (!f) { hostfile_close(hfd); return 0; }
            f->type = FD_HOST;
            f->host_fd = hfd;
            f->readable = 1;
            f->writable = 1;
            return f;
        }
    }

    return 0;  // not found
}
