// kernel/fs.h — Embedded file registry (raw data, no fd layer)
#ifndef _FS_H_
#define _FS_H_

#define MAX_FILES   8
#define FILE_NAME_MAX 32

typedef struct {
    char          name[FILE_NAME_MAX];
    const char   *data;
    unsigned long size;
} EmbeddedFile;

extern EmbeddedFile file_table[MAX_FILES];
extern int  file_count;

void fs_init(void);
int  fs_register(const char *name, const char *data, unsigned long size);
int  fs_find(const char *name);   // returns embfile_id or -1
int  fs_read_raw(int id, char *buf, int maxlen, int offset);
void fs_list(void);

#endif
