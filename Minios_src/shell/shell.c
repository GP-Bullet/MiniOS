#include "shell.h"

#include "assert.h"
#include "buildin_cmd.h"
#include "dir.h"
#include "exec.h"
#include "file.h"
#include "fs.h"
#include "global.h"
#include "stdio.h"
#include "string.h"
#include "syscall.h"
#define cmd_len 128    // 最大支持键入128个字符的命令行输入
#define MAX_ARG_NR 16  // 加上命令名外,最多支持15个参数

/* 存储输入的命令 */
static char cmd_line[cmd_len] = {0};
char final_path[MAX_PATH_LEN] = {0};  // 用于洗路径时的缓冲
/* 用来记录当前目录,是当前目录的缓存,每次执行cd命令时会更新此内容 */
char cwd_cache[64] = {0};

/* 输出提示符 */
void print_prompt(void) { printf("[rabbit@localhost %s]$ ", cwd_cache); }

/* 从键盘缓冲区中最多读入count个字节到buf。*/
static void readline(char* buf, int32_t count) {
  assert(buf != NULL && count > 0);
  char* pos = buf;

  while (read(stdin_no, pos, 1) != -1 &&
         (pos - buf) < count) {  // 在不出错情况下,直到找到回车符才返回
    switch (*pos) {
        /* 找到回车或换行符后认为键入的命令结束,直接返回 */
      case '\n':
      case '\r':
        *pos = 0;  // 添加cmd_line的终止字符0
        putchar('\n');
        return;

      case '\b':
        if (cmd_line[0] != '\b') {  // 阻止删除非本次输入的信息
          --pos;  // 退回到缓冲区cmd_line中上一个字符
          putchar('\b');
        }
        break;

        /* ctrl+l 清屏 */
      case 'l' - 'a':
        /* 1 先将当前的字符'l'-'a'置为0 */
        *pos = 0;
        /* 2 再将屏幕清空 */
        clear();
        /* 3 打印提示符 */
        print_prompt();
        /* 4 将之前键入的内容再次打印 */
        printf("%s", buf);
        break;

      /* ctrl+u 清掉输入 */
      case 'u' - 'a':
        while (buf != pos) {
          putchar('\b');
          *(pos--) = 0;
        }
        break;

      /* 非控制键则输出字符 */
      default:
        putchar(*pos);
        pos++;
    }
  }
  printf(
      "readline: can`t find enter_key in the cmd_line, max num of char is "
      "128\n");
}

/* 分析字符串 cmd_str 中以 token 为分隔符的单词,将各单词的指针存入 argv 数组 */
static int32_t cmd_parse(char* cmd_str, char** argv, char token) {
  assert(cmd_str != NULL);
  int32_t arg_idx = 0;
  while (arg_idx < MAX_ARG_NR) {
    argv[arg_idx] = NULL;
    arg_idx++;
  }
  char* next = cmd_str;
  int32_t argc = 0;
  /* 外层循环处理整个命令行 */
  while (*next) {
    /* 去除命令字或参数之间的空格 */
    while (*next == token) {
      next++;
    }
    if (*next == 0) {
      break;
    }
    argv[argc] = next;
    while (*next && *next != token) {
      next++;
    }
    if (*next) {
      *next++ = 0;
    }
    if (argc > MAX_ARG_NR) {
      return -1;
    }
    argc++;
  }
  return argc;
}

/*执行命令*/
static void cmd_exectue(uint32_t argc, char** argv) {
  if (!strcmp("ls", argv[0])) {
    buildin_ls(argc, argv);
  } else if (!strcmp("cd", argv[0])) {
    if (buildin_cd(argc, argv) != NULL) {
      memset(cwd_cache, 0, MAX_PATH_LEN);
      strcpy(cwd_cache, final_path);
    }
  } else if (!strcmp("cd", argv[0])) {
    buildin_cd(argc, argv);
  } else if (!strcmp("ps", argv[0])) {
    buildin_ps(argc, argv);
  } else if (!strcmp("clear", argv[0])) {
    buildin_clear(argc, argv);
  } else if (!strcmp("mkdir", argv[0])) {
    buildin_mkdir(argc, argv);
  } else if (!strcmp("rmdir", argv[0])) {
    buildin_rmdir(argc, argv);
  } else if (!strcmp("rm", argv[0])) {
    buildin_rm(argc, argv);
  } else if (!strcmp("touch", argv[0])) {
    buildin_touch(argc, argv);
  } else if (!strcmp("pwd", argv[0])) {
    buildin_pwd(argc, argv);
  } else if (!strcmp("help", argv[0])) {
    buildin_help(argc, argv);
  } else {  // 如果是外部命令,需要从磁盘上加载
    int32_t pid = fork();
    if (pid) {  // 父进程
      int32_t status;
      int32_t child_pid = wait(&status);
      if (child_pid == -1) {
        panic("my_shell: no child\n");
      }
      printf("\n");
      printf("child_pid %d, it's status: %d\n", child_pid, status);
    } else {
      make_clear_abs_path(argv[0], final_path);
      /* 先判断下文件是否存在 */
      struct stat file_stat;
      argv[0] = final_path;
      if (stat(argv[0], &file_stat) == -1) {
        printf("my_shell: cannot access %s,No such file or directory\n",
               argv[0]);
      } else {
        execv(argv[0], argv);
      }
    }
    int32_t arg_idx = 0;
    while (arg_idx < MAX_ARG_NR) {
      argv[arg_idx] = NULL;
      arg_idx++;
    }
  }
}

char* argv[MAX_ARG_NR];
int32_t argc = -1;

/* 简单的shell */
void my_shell(void) {
  cwd_cache[0] = '/';
  while (1) {
    print_prompt();
    memset(final_path, 0, MAX_PATH_LEN);
    memset(cmd_line, 0, cmd_len);
    readline(cmd_line, cmd_len);
    if (cmd_line[0] == 0) {  // 若只键入了一个回车
      continue;
    }
    /*针对管道的处理*/
    char* pipe_symbol = strchr(cmd_line, '|');
    if (pipe_symbol) {
      int32_t fd[2] = {-1, -1};
      pipe(fd);
      // 将标准输出重定位到管道输入
      fd_redirect(1, fd[1]);

      // 第一个命令
      char* each_cmd = cmd_line;
      pipe_symbol = strchr(each_cmd, '|');
      *pipe_symbol = 0;
      // 执行第一个命令
      argc = -1;
      argc = cmd_parse(each_cmd, argv, ' ');
      cmd_exectue(argc, argv);
      /* 跨过'|',处理下一个命令 */
      each_cmd = pipe_symbol + 1;
      // 将标准输入重定位到管道的输出
      fd_redirect(0, fd[0]);
      /*中间的命令,命令的输入和输出都是指向环形缓冲区 */
      while ((pipe_symbol = strchr(each_cmd, '|'))) {
        *pipe_symbol = 0;
        argc = -1;
        argc = cmd_parse(each_cmd, argv, ' ');
        cmd_exectue(argc, argv);
        each_cmd = pipe_symbol + 1;
      }

      /*处理最后一个命令*/
      // 恢复标准输出
      fd_redirect(1, 1);

      argc = -1;

      argc = cmd_parse(each_cmd, argv, ' ');
      cmd_exectue(argc, argv);

      fd_redirect(0, 0);

      close(fd[0]);
      close(fd[1]);
    } else {  // 无管道的命令
      argc = -1;
      argc = cmd_parse(cmd_line, argv, ' ');
      if (argc == -1) {
        printf("num of arguments exceed %d\n", MAX_ARG_NR);
        continue;
      }
      cmd_exectue(argc, argv);
    }
  }
  panic("my_shell: should not be here");
}
