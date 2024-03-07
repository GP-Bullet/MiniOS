#ifndef THREAD_THREAD
#define THREAD_THREAD

#include "list.h"
#include "memory.h"
#include "stdint.h"

#define MAX_FILES_OPEN_PER_PROC 8  // 每个进程允许打开文件的最大数量
typedef void thread_func(void*);
typedef int16_t pid_t;

#define STACK_MAGIC 0x19870916  // 自定义魔术
/*进程状态*/
enum task_status {
  TASK_RUNNING,  // 运行
  TASK_READY,    // 准备
  TASK_BLOCKED,  // 阻塞
  TASK_WAITING,  // 等待
  TASK_HANGING,  // 挂起
  TASK_DIED,     // 死亡
};

// 中断产生时一系列压栈操作是压入了此结构中
struct intr_stack {
  uint32_t vec_no;  // 中断号
  /* 下面是八个通用寄存器*/
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t esp_dummy;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
  /* 下面是四个段寄存器*/
  uint32_t gs;
  uint32_t fs;
  uint32_t es;
  uint32_t ds;
  /*以下由 cpu 从低特权级进入高特权级时压入*/
  uint32_t err_code;  // 错误号
  void (*eip)(void);
  uint32_t cs;
  uint32_t eflags;
  void* esp;
  uint32_t ss;
};

// 线程栈thread_stack,内核中位置不确定
struct thread_stack {
  uint32_t ebp;
  uint32_t ebx;
  uint32_t edi;
  uint32_t esi;
  void (*eip)(thread_func* func, void* func_arg);
  void(*unused_retaddr);  // 参数 unused_ret 只为占位置充数为返回地址
  thread_func* function;  // 由 kernel_thread 所调用的函数名
  void* func_arg;         // 由 kernel_thread 所调用的函数所需的参数
};

/***** 以下仅供第一次被调度上 cpu 时使用****/

/* 进程或线程的 pcb,程序控制块 */
struct task_struct {
  uint32_t* self_kstack;  // 各内核线程都用自己的内核栈
  pid_t pid;
  enum task_status status;
  char name[16];
  uint8_t priority;        // 线程优先级
  uint8_t ticks;           // 每次在处理器上执行的时间嘀嗒数
  uint32_t elapsed_ticks;  // 运行时间嘀嗒总数（总运行时间）

  int32_t fd_table[MAX_FILES_OPEN_PER_PROC];  // 文件描述符数组

  struct list_elem general_tag;  // 用于线程在一般的队列(就绪/等待队列)中的结点
  struct list_elem all_list_tag;  // 总队列(所有线程)中的节点

  uint32_t* pgdir;                     // 进程自己页表的虚拟地址
  struct virtual_addr userprog_vaddr;  // 放进程页目录表的虚拟地址
  struct mem_block_desc u_block_desc[DESC_CNT];  // 用户进程内存块描述符
  uint32_t cwd_inode_nr;  // 进程所在的工作目录的inode编号
  int16_t parent_pid;     // 父进程的pid
  int8_t exit_status;     // 进程结束时自己调用exit传入的返回值
  uint32_t stack_magic;   // 栈的边界标记,用于检测栈的溢出
};

extern struct list thread_ready_list;
extern struct list thread_all_list;

struct task_struct* thread_start(char* name, int prio, thread_func fuction,
                                 void* func_arg);

/*获取当前pcb指针*/
struct task_struct* running_thread();

/*实现任务调度*/
void schedule();

/* 初始化线程环境 */
void thread_init(void);

/*设置线程的阻塞状态*/
void thread_block(enum task_status stat);

/*解除pthread的阻塞状态*/
void thread_unblock(struct task_struct* pthread);

void init_thread(struct task_struct* pthread, char* name, int prio);
void thread_create(struct task_struct* pthread, thread_func function,
                   void* func_arg);
void thread_yield(void);
int32_t pcb_fd_install(uint32_t fd_idx);

pid_t fork_pid(void);
void sys_ps(void);
void release_pid(pid_t pid);
void thread_exit(struct task_struct* thread_over, bool need_schedule);
struct task_struct* pid2thread(int32_t pid);
#endif /* THREAD_THREAD */
