// kernel/file.c — xv6-style VFS: ftable, filealloc, fileread, filewrite

#include "file.h"
#include "fs.h"
#include "proc.h"
#include "hostfile.h"

struct file ftable[NFILE];
struct devsw devsw[NDEV];

// FAT external API
void fat_init(int (*read_fn)(unsigned long sector, char *buf));
int  fat_find(const char *name, unsigned long *cluster, unsigned int *size);
int  fat_read(unsigned long cluster, unsigned int offset, char *buf, int maxlen, unsigned int file_size);

void fileinit(void)
{
    for (int i = 0; i < NFILE; i++)
        ftable[i].type = FD_NONE;
}

// Allocate an empty file struct from ftable
struct file* filealloc(void)
{
    for (int i = 0; i < NFILE; i++) {
        if (ftable[i].type == FD_NONE) {
            ftable[i].type = FD_INODE;
            ftable[i].ref = 1;
            ftable[i].readable = 1;
            ftable[i].writable = 0;
            ftable[i].major = 0;
            ftable[i].embfile_id = -1;
            ftable[i].off = 0;
            return &ftable[i];
        }
    }
    return 0;
}

// Duplicate — for fork()
struct file* filedup(struct file *f)
{
    if (f->ref < 1)
        return 0;
    f->ref++;
    return f;
}

// Close
void fileclose(struct file *f)
{
    if (f->ref < 1)
        return;
    f->ref--;
    if (f->ref == 0) {
        if (f->type == FD_HOST && f->host_fd >= 0)
            hostfile_close(f->host_fd);
        f->type = FD_NONE;
    }
}

// Read: dispatch by type
int fileread(struct file *f, char *addr, int n)
{
    if (!f->readable)
        return -1;

    switch (f->type) {
    case FD_INODE: {
        if (f->embfile_id < 0 || f->embfile_id >= file_count)
            return -1;
        int ret = fs_read_raw(f->embfile_id, addr, n, f->off);
        if (ret > 0)
            f->off += ret;
        return ret;
    }
    case FD_FAT: {
        int ret = fat_read(f->fat_start_cluster, (unsigned int)f->off,
                             addr, n, f->fat_file_size);
        if (ret > 0)
            f->off += ret;
        return ret;
    }
    case FD_DEVICE:
        if (f->major >= 0 && f->major < NDEV && devsw[f->major].read)
            return devsw[f->major].read(addr, n);
        return -1;

    case FD_HOST: {
        int ret = hostfile_read(f->host_fd, addr, n);
        if (ret > 0)
            f->off += ret;
        return ret;
    }

    default:
        return -1;
    }
}

// Write: dispatch by type
int filewrite(struct file *f, char *addr, int n)
{
    if (!f->writable)
        return -1;

    switch (f->type) {
    case FD_INODE:
        return -1;  // embedded files are read-only

    case FD_DEVICE:
        if (f->major >= 0 && f->major < NDEV && devsw[f->major].write)
            return devsw[f->major].write(addr, n);
        return -1;

    case FD_HOST: {
        int ret = hostfile_write(f->host_fd, addr, n);
        if (ret > 0)
            f->off += ret;
        return ret;
    }

    default:
        return -1;
    }
}
