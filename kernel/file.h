// kernel/file.h — xv6-style virtual file system layer
#ifndef _FILE_H_
#define _FILE_H_

#define NFILE  16
#define NDEV    4
#define DEV_CONSOLE 0

// File types
enum { FD_NONE, FD_INODE, FD_DEVICE, FD_FAT, FD_HOST };

// Device switch — one entry per device type
struct devsw {
    int (*read)(char *addr, int n);
    int (*write)(char *addr, int n);
};

extern struct devsw devsw[NDEV];

struct file {
    int type;              // FD_NONE, FD_INODE, FD_DEVICE, FD_FAT
    int ref;               // reference count
    char readable;
    char writable;
    int major;             // device type (DEV_CONSOLE for console)
    int embfile_id;        // for FD_INODE: index into file_table[]
    int off;               // read/write offset
    unsigned long fat_start_cluster;  // for FD_FAT: first cluster
    unsigned int  fat_file_size;      // for FD_FAT: file size in bytes
    int host_fd;                      // for FD_HOST: semihosting file descriptor
};

// Global open file table
extern struct file ftable[NFILE];

// API
void            fileinit(void);
struct file*    filealloc(void);
struct file*    filedup(struct file *f);
void            fileclose(struct file *f);
int             fileread(struct file *f, char *addr, int n);
int             filewrite(struct file *f, char *addr, int n);

#endif
