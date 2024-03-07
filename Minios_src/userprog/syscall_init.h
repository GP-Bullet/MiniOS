#ifndef USERPROG_SYSCALL_INIT
#define USERPROG_SYSCALL_INIT
#include "stdint.h"
void syscall_init(void);
uint32_t sys_getpid(void);
void sys_help(void);
#endif /* USERPROG_SYSCALL_INIT */
