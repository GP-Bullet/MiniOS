#include "syscall_init.h"

#include "console.h"
#include "exec.h"
#include "fork.h"
#include "fs.h"
#include "memory.h"
#include "pipe.h"
#include "print.h"
#include "stdint.h"
#include "stdio_kernel.h"
#include "string.h"
#include "syscall.h"
#include "thread.h"
#include "wait_exit.h"
#define syscall_nr 32
typedef void* syscall;
syscall syscall_table[syscall_nr];

/* 返回当前任务的pid */
uint32_t sys_getpid(void) { return running_thread()->pid; }

/* 显示系统支持的内部命令 */
void sys_help(void) {
  printk(
      "\
 buildin commands:\n\
       ls: show directory or file information\n\
       cd: change current work directory\n\
       mkdir: create a directory\n\
       rmdir: remove a empty directory\n\
       rm: remove a regular file\n\
       pwd: show current work directory\n\
       ps: show process information\n\
       clear: clear screen\n\
 shortcut key:\n\
       ctrl+l: clear screen\n\
       ctrl+u: clear input\n\n");
}

/* 初始化系统调用 */
void syscall_init(void) {
  put_str("syscall_init start\n");
  syscall_table[SYS_GETPID] = sys_getpid;
  syscall_table[SYS_WRITE] = sys_write;
  syscall_table[SYS_MALLOC] = sys_malloc;
  syscall_table[SYS_FREE] = sys_free;
  syscall_table[SYS_FORK] = sys_fork;
  syscall_table[SYS_READ] = sys_read;
  syscall_table[SYS_PUTCHAR] = sys_putchar;
  syscall_table[SYS_CLEAR] = cls_screen;
  syscall_table[SYS_GETCWD] = sys_getcwd;
  syscall_table[SYS_OPEN] = sys_open;
  syscall_table[SYS_CLOSE] = sys_close;
  syscall_table[SYS_LSEEK] = sys_lseek;
  syscall_table[SYS_UNLINK] = sys_unlink;
  syscall_table[SYS_MKDIR] = sys_mkdir;
  syscall_table[SYS_OPENDIR] = sys_opendir;
  syscall_table[SYS_CLOSEDIR] = sys_closedir;
  syscall_table[SYS_CHDIR] = sys_chdir;
  syscall_table[SYS_RMDIR] = sys_rmdir;
  syscall_table[SYS_READDIR] = sys_readdir;
  syscall_table[SYS_REWINDDIR] = sys_rewinddir;
  syscall_table[SYS_STAT] = sys_stat;
  syscall_table[SYS_PS] = sys_ps;
  syscall_table[SYS_PUT_COLOR] = console_str_color;
  syscall_table[SYS_CREAT] = sys_create;
  syscall_table[SYS_EXECV] = sys_execv;
  syscall_table[SYS_EXIT] = sys_exit;
  syscall_table[SYS_WAIT] = sys_wait;
  syscall_table[SYS_PIPE] = sys_pipe;
  syscall_table[SYS_FD_REDIRECT] = sys_fd_redirect;
  syscall_table[SYS_HELP] = sys_help;
  put_str("syscall_init done\n");
}