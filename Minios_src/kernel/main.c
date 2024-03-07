#include "console.h"
#include "debug.h"
#include "dir.h"
#include "fs.h"
#include "init.h"
#include "interrupt.h"
#include "memory.h"
#include "print.h"
#include "process.h"
#include "shell.h"
#include "stdio.h"
#include "stdio_kernel.h"
#include "string.h"
#include "sync.h"
#include "syscall.h"
#include "syscall_init.h"
#include "thread.h"
void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);
void load_user_code(uint32_t size, char* name);

int main(void) {
  put_str("I am kernel\n");
  init_all();
  intr_enable();
  /*************    写入应用程序    *************/
  // load_user_code(18292, "prog_arg");
  // load_user_code(20576, "cat");
  /*************    写入应用程序结束   *************/
  cls_screen();
  printf("\n");
  put_color("             ______            ____  _____     \n", 11);

  put_color("            / ____/           / __ \\/ ___/     ", 11);
  put_color("Name:", 11);
  printf("G-OS\n");
  put_color("           / / __   ______   / / / /\\__ \\      ", 11);
  put_color("Version:", 11);
  printf("1.0.0.0\n");
  put_color("          / /_/ /  /_____/  / /_/ /___/ /      ", 11);
  put_color("Architecture:", 11);
  printf("X86\n");
  put_color("          \\____/            \\____//____/       ", 11);
  put_color("Language:", 11);
  printf("C\n");
  printf("\n");

  for (int i = 0; i < 80; i++) {
    if (i % 9 == 0) {
      put_color("-", 7);
    } else if (i % 9 == 1) {
      put_color("-", 8);
    } else if (i % 9 == 2) {
      put_color("-", 9);
    } else if (i % 9 == 3) {
      put_color("-", 10);
    } else if (i % 9 == 4) {
      put_color("-", 11);
    } else if (i % 9 == 5) {
      put_color("-", 12);
    } else if (i % 9 == 6) {
      put_color("-", 13);
    } else if (i % 9 == 7) {
      put_color("-", 14);
    } else if (i % 9 == 8) {
      put_color("-", 15);
    }
  }

  printf("\n\n");
  console_put_str("[rabbit@localhost /]$ ");
  while (1)
    ;
  return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg) {
  while (1)
    ;
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {
  while (1)
    ;
}

/* 测试用户进程 */
void u_prog_a(void) {
  while (1)
    ;
}

/* 测试用户进程 */
void u_prog_b(void) {
  while (1)
    ;
}

/*init进程*/
void init(void) {
  uint32_t ret_pid = fork();
  if (ret_pid) {
    int status;
    int child_pid;
    /* init 在此处不停地回收僵尸进程 */
    while (1) {
      child_pid = wait(&status);
      printf(
          "I`m init, My pid is 1, I recieve a child, It`s pid is %d, status is "
          "%d\n",
          child_pid, status);
    }
  } else {
    my_shell();
  }
  while (1)
    ;
}
void load_user_code(uint32_t size, char* name) {
  uint32_t file_size = size;
  uint32_t sec_cnt = DIV_ROUND_UP(file_size, 512);

  struct disk* sda = &channels[0].devices[0];
  char namebuf[16] = {0};
  namebuf[0] = '/';
  namebuf[1] = 0;
  strcat(namebuf, name);
  void* prog_buf = sys_malloc(file_size);
  ide_read(sda, 300, prog_buf, sec_cnt);
  int32_t fd = sys_open(namebuf, O_CREAT | O_RDWR);
  if (fd != -1) {
    if (sys_write(fd, prog_buf, file_size) == -1) {
      printk("file write error!\n");
      while (1)
        ;
    }
  }
  close(fd);
}
