// user/user.h — Shared declarations for user programs
#ifndef _USER_H_
#define _USER_H_

// Syscall wrappers
void sys_write(int fd, const char *s, int len);
int  sys_read(int fd, char *buf, int len);
int  sys_open(const char *name, int flags);
void sys_close(int fd);
int  sys_getpid(void);
int  sys_uptime(void);
void sys_fs_list(void);
void sys_exit(int code);
int  sys_wait(void);
int  sys_fork(void);
void sys_sleep(int ticks);

// Utility (implemented in user/ulib.c)
int  slen(const char *s);
void put(const char *s);
void putdec(int n);
int  atoi2(const char *s, int len);
void readline(char *buf, int max);
void do_cat(const char *name);
void do_write(const char *name, const char *text);

// child_iters — set by Shell before exec(), demos read it
extern int child_iters;

#endif
