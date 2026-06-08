// kernel/hostfile.h — Semihosting-backed host file access (files/ sandbox)
#ifndef _HOSTFILE_H_
#define _HOSTFILE_H_

#define HOSTFILE_MAX  8

int hostfile_open(const char *name);
int hostfile_read(int fd, char *buf, int len);
int hostfile_write(int fd, char *buf, int len);
int hostfile_size(int fd);
int hostfile_seek(int fd, int pos);
void hostfile_close(int fd);

#endif
