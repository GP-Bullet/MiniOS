#include "thread.h"

#include <stdio.h>

#include "bitmap.h"
#include "debug.h"
#include "file.h"
#include "fs.h"
#include "global.h"
#include "interrupt.h"
#include "list.h"
#include "memory.h"
#include "print.h"
#include "process.h"
#include "stdint.h"
#include "string.h"
#include "sync.h"

struct task_struct* main_thread;      // 主线程PCB
struct task_struct* idle_thread;      // ide线程
struct list thread_ready_list;        // 就绪队列
struct list thread_all_list;          // 所有任务队列
static struct list_elem* thread_tag;  // 用于保存队列中的线程结点

struct lock pid_lock;  // 分配pid锁
/* pid 的位图,最大支持 1024 个 pid */
uint8_t pid_bitmap_bits[128] = {0};
/*pid池*/
struct pid_pool {
  struct bitmap pid_bitmap;  // pid位图
  uint32_t pid_start;        // 起始pid
  struct lock pid_lock;      // 分配pid锁
} pid_pool;

extern void switch_to(struct task_struct* cur, struct task_struct* next);
extern void init(void);

/* 初始化 pid 池 */
static void pid_pool_init(void) {
  pid_pool.pid_start = 1;
  pid_pool.pid_bitmap.bits = pid_bitmap_bits;
  pid_pool.pid_bitmap.btmp_bytes_len = 128;
  bitmap_init(&pid_pool.pid_bitmap);
  lock_init(&pid_pool.pid_lock);
}

/*分配pid*/
static pid_t allocate_pid(void) {
  lock_acquire(&pid_pool.pid_lock);
  int32_t bit_idx = bitmap_scan(&pid_pool.pid_bitmap, 1);
  bitmap_set(&pid_pool.pid_bitmap, bit_idx, 1);
  lock_release(&pid_pool.pid_lock);
  return (bit_idx + pid_pool.pid_start);
}

/*释放pid*/
void release_pid(pid_t pid) {
  lock_acquire(&pid_pool.pid_lock);
  int32_t bit_idx = pid - pid_pool.pid_start;
  bitmap_set(&pid_pool.pid_bitmap, bit_idx, 0);
  lock_release(&pid_pool.pid_lock);
}

/* 回收 结束线程 的 pcb 和页表,并将其从调度队列中去除 */
void thread_exit(struct task_struct* thread_over, bool need_schedule) {
  /* 要保证 schedule 在关中断情况下调用 */
  intr_disable();
  thread_over->status = TASK_DIED;

  /* 如果 thread_over 不是当前线程,
 就有可能还在就绪队列中,将其从中删除 */
  if (elem_find(&thread_ready_list, &thread_over->general_tag)) {
    list_remove(&thread_over->general_tag);
  }
  if (thread_over->pgdir) {
    mfree_page(PF_KERNEL, thread_over->pgdir, 1);
  }
  /* 从 all_thread_list 中去掉此任务 */
  list_remove(&thread_over->all_list_tag);

  /* 回收 pcb 所在的页,主线程的 pcb 不在堆中,跨过 */
  if (thread_over != main_thread) {
    mfree_page(PF_KERNEL, thread_over, 1);
  }

  /*归来pid*/
  release_pid(thread_over->pid);

  /* 如果需要下一轮调度则主动调用 schedule */
  if (need_schedule) {
    schedule();
    PANIC("thread_exit: should not be here\n");
  }
}

/*对比任务的pid*/
static bool pid_check(struct list_elem* pelem, int32_t pid) {
  struct task_struct* pthread =
      elem2entry(struct task_struct, all_list_tag, pelem);
  if (pthread->pid == pid) {
    return true;
  }

  return false;
}

/* 根据 pid 找 pcb,若找到则返回该 pcb,否则返回 NULL */
struct task_struct* pid2thread(int32_t pid) {
  struct list_elem* pelem = list_traversal(&thread_all_list, pid_check, pid);
  if (pelem == NULL) {
    return NULL;
  }
  struct task_struct* thread =
      elem2entry(struct task_struct, all_list_tag, pelem);

  return thread;
}

/*系统空闲时运行的线程*/
static void idle(void* arg UNUSED) {
  while (1) {
    thread_block(TASK_BLOCKED);
    // 执行 hlt 时必须要保证目前处在开中断的情况下
    // (不然程序就会挂在下面那条指令上)
    asm volatile("sti; hlt" : : : "memory");
  }
}

/*获取当前pcb指针*/
struct task_struct* running_thread() {
  uint32_t esp;
  asm("mov %%esp,%0" : "=g"(esp));  // 获取当前栈指针

  // 进行对齐，对齐到pcb起始位置(一个pcb只占一个页(0~fff))
  return (struct task_struct*)uint32ToVoidptr(esp & 0xfffff000);
}

/* 由 kernel_thread 去执行 function(func_arg) */
static void kernel_thread(thread_func* function, void* func_arg) {
  intr_enable();  // 打开中断
  function(func_arg);
}

/* 初始化线程栈 thread_stack,
将待执行的函数和参数放到 thread_stack 中相应的位置 */
void thread_create(struct task_struct* pthread, thread_func function,
                   void* func_arg) {
  // 先预留中断使用栈的空间
  pthread->self_kstack -= sizeof(struct intr_stack);
  // 再预留线程栈使用的空间
  pthread->self_kstack -= sizeof(struct thread_stack);
  struct thread_stack* kthread_stack =
      (struct thread_stack*)pthread->self_kstack;
  kthread_stack->eip = kernel_thread;
  kthread_stack->func_arg = func_arg;
  kthread_stack->function = function;
  kthread_stack->ebp = 0;
  kthread_stack->ebp = 0;
  kthread_stack->edi = 0;
  kthread_stack->esi = 0;
}

/*初始化线程基本信息*/
void init_thread(struct task_struct* pthread, char* name, int prio) {
  memset(pthread, 0, sizeof(*pthread));
  pthread->pid = allocate_pid();
  strcpy(pthread->name, name);
  if (pthread == main_thread) {
    pthread->status = TASK_RUNNING;
  } else {
    pthread->status = TASK_READY;
  }

  pthread->self_kstack = (uint32_t*)((char*)pthread + PG_SIZE);
  pthread->priority = prio;
  pthread->ticks = prio;
  pthread->elapsed_ticks = 0;
  pthread->pgdir = NULL;
  /*预留标准输入输出*/
  pthread->fd_table[0] = 0;
  pthread->fd_table[1] = 1;
  pthread->fd_table[2] = 2;
  /*其余置为-1*/
  uint8_t fd_idx = 3;
  while (fd_idx < MAX_FILES_OPEN_PER_PROC) {
    pthread->fd_table[fd_idx] = -1;
    fd_idx++;
  }
  pthread->cwd_inode_nr = 0;  // 以根目录作为默认工作路径
  pthread->parent_pid = -1;
  pthread->stack_magic = STACK_MAGIC;  // 魔数
}

struct task_struct* thread_start(char* name, int prio, thread_func fuction,
                                 void* func_arg) {
  // PCB都位于内核空间
  struct task_struct* thread = get_kernel_pages(1);
  init_thread(thread, name, prio);
  thread_create(thread, fuction, func_arg);
  /*确保之前不再队列里面*/
  ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
  /*加入就绪线程队列*/
  list_append(&thread_ready_list, &thread->general_tag);

  /* 确保之前不在队列中 */
  ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
  /*加入全部线程线程队列*/
  list_append(&thread_all_list, &thread->all_list_tag);

  return thread;
}

/*实现任务调度*/
void schedule() {
  // 此时中断应该处于关闭状态
  ASSERT(intr_get_status() == INTR_OFF);
  struct task_struct* cur = running_thread();
  if (cur->status == TASK_RUNNING) {
    // 该线程时间片已经使用完，将其加入队尾
    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
    list_append(&thread_ready_list, &cur->general_tag);
    cur->ticks = cur->priority;
    cur->status = TASK_READY;
  } else {
    /* 若此线程需要某事件发生后才能继续上 cpu 运行,
      不需要将其加入队列,因为当前线程不在就绪队列中 */
  }

  // 没有任务时就唤醒ide线程
  if (list_empty(&thread_ready_list)) {
    thread_unblock(idle_thread);
  }

  thread_tag = NULL;
  thread_tag = list_pop(&thread_ready_list);  // 弹出一个线程上cpu

  struct task_struct* next =
      elem2entry(struct task_struct, general_tag, thread_tag);
  next->status = TASK_RUNNING;
  /* 激活任务页表等 */
  process_activate(next);
  switch_to(cur, next);
}

/*将kernel中的main函数完善为主线程*/
static void make_main_thread(void) {
  /* 因为 main 线程早已运行,
   * 咱们在 loader.S 中进入内核时的 mov esp,0xc009f000,
   * 就是为其预留 pcb 的,因此 pcb 地址为 0xc009e000,
   * 不需要通过 get_kernel_page 另分配一页*/
  main_thread = running_thread();
  init_thread(main_thread, "main", 31);

  // main线程正在运行，所以不需要添加在就绪队列当中
  ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
  list_append(&thread_all_list, &main_thread->all_list_tag);
}

/*设置线程的阻塞状态*/
void thread_block(enum task_status stat) {
  ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) ||
          (stat == TASK_HANGING)))
  enum intr_status old_status = intr_disable();  // 关闭中断
  struct task_struct* cur_thread = running_thread();
  cur_thread->status = stat;
  schedule();  // 调度到其他线程
  /* 待当前线程被解除阻塞后才继续运行下面的 intr_set_status */
  intr_set_status(old_status);
}

/*解除pthread的阻塞状态*/
void thread_unblock(struct task_struct* pthread) {
  enum intr_status old_status = intr_disable();  // 关闭中断

  ASSERT(((pthread->status == TASK_BLOCKED) ||
          (pthread->status == TASK_WAITING) ||
          (pthread->status == TASK_HANGING)));
  if (pthread->status != TASK_READY) {
    ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
    if (elem_find(&thread_ready_list, &pthread->general_tag)) {
      PANIC("thread_unblock: blocked thread in ready_list\n");
    }
    list_push(&thread_ready_list, &pthread->general_tag);
    pthread->status = TASK_READY;
  }
  intr_set_status(old_status);
}

/*主动让出CPU(重新入队等待下一轮调度)*/
void thread_yield(void) {
  put_str("thread_init start\n");
  struct task_struct* cur = running_thread();
  enum intr_status old_status = intr_disable();
  ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
  list_append(&thread_ready_list, &cur->general_tag);
  cur->status = TASK_READY;
  schedule();
  intr_set_status(old_status);
}

int32_t pcb_fd_install(uint32_t fd_idx) {
  struct task_struct* cur_thread = running_thread();
  uint32_t idx = 3;
  while (idx < MAX_FILES_OPEN_PER_PROC) {
    if (cur_thread->fd_table[idx] == -1) {
      cur_thread->fd_table[idx] = fd_idx;
      return idx;
    }
    idx++;
  }
  return -1;
}
pid_t fork_pid(void) { return allocate_pid(); }

/*以填充空格的方式输出buf*/
static void pad_print(char* buf, int32_t buf_len, void* ptr, char format) {
  memset(buf, 0, buf_len);
  uint8_t out_pad_0idx = 0;
  switch (format) {
    case 's':
      out_pad_0idx = sprintf(buf, "%s", ptr);
      break;
    case 'd':
      out_pad_0idx = sprintf(buf, "%d", *((int16_t*)ptr));
      break;
    case 'x':
      out_pad_0idx = sprintf(buf, "%x", *((int32_t*)ptr));
      break;
  }
  while (out_pad_0idx < buf_len) {
    buf[out_pad_0idx] = ' ';
    out_pad_0idx++;
  }
  sys_write(stdout_no, buf, buf_len - 1);
}

/* 用于在 list_traversal 函数中的回调函数,用于针对线程队列的处理 */
static bool elem2thread_info(struct list_elem* pelem, int arg UNUSED) {
  struct task_struct* pthread =
      elem2entry(struct task_struct, all_list_tag, pelem);
  char out_pad[16] = {0};
  pad_print(out_pad, 16, &pthread->pid, 'd');
  if (pthread->parent_pid == -1) {
    pad_print(out_pad, 16, "NULL", 's');
  } else {
    pad_print(out_pad, 16, &pthread->parent_pid, 'd');
  }

  switch (pthread->status) {
    case 0:
      pad_print(out_pad, 16, "RUNNING", 's');
      break;
    case 1:
      pad_print(out_pad, 16, "READY", 's');
      break;
    case 2:
      pad_print(out_pad, 16, "BLOCKED", 's');
      break;
    case 3:
      pad_print(out_pad, 16, "WAITING", 's');
      break;
    case 4:
      pad_print(out_pad, 16, "HANGING", 's');
      break;
    case 5:
      pad_print(out_pad, 16, "DIED", 's');
  }
  pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');
  memset(out_pad, 0, 16);
  ASSERT(strlen(pthread->name) < 17);
  memcpy(out_pad, pthread->name, strlen(pthread->name));
  strcat(out_pad, "\n");
  sys_write(stdout_no, out_pad, strlen(out_pad));

  return false;
}

/*打印任务列表*/
void sys_ps(void) {
  char* ps_title =
      "PID            PPID           STAT           TICKS          "
      "COMMAND\n";
  sys_write(stdout_no, ps_title, strlen(ps_title));
  list_traversal(&thread_all_list, elem2thread_info, 0);
}

/* 初始化线程环境 */
void thread_init(void) {
  put_str("thread_init start\n");
  list_init(&thread_ready_list);
  list_init(&thread_all_list);
  lock_init(&pid_lock);
  pid_pool_init();
  /* 先创建第一个用户进程:init */
  process_execute(init, "init");
  /* 将当前 main 函数创建为线程 */
  make_main_thread();
  /* 创建 idle 线程 */
  idle_thread = thread_start("idle", 10, idle, NULL);
  put_str("thread_init done\n");
}
