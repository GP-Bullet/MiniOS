#include "process.h"

#include "bitmap.h"
#include "console.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"
#include "memory.h"
#include "print.h"
#include "string.h"
#include "tss.h"
/*构建用户进程初始化上下文*/
void start_process(void* filename_) {
  void* function = filename_;
  struct task_struct* cur = running_thread();
  cur->self_kstack += sizeof(struct thread_stack);  // 跳过进程栈，指向中断栈
  struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;

  proc_stack->edi = 0;
  proc_stack->esi = 0;
  proc_stack->ebp = 0;
  proc_stack->esp_dummy = 0;
  proc_stack->ebx = 0;
  proc_stack->edx = 0;
  proc_stack->ecx = 0;
  proc_stack->eax = 0;
  proc_stack->gs = 0;
  proc_stack->fs = proc_stack->ds = proc_stack->es = SELECTOR_U_DATA;

  proc_stack->eip = function;  // 待执行的用户处理程序
  proc_stack->cs = SELECTOR_U_CODE;
  proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
  // 用户栈放在虚拟地址，用户空间的最高处
  proc_stack->esp =
      (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE);
  proc_stack->ss = SELECTOR_U_DATA;

  asm volatile("movl %0, %%esp; jmp intr_exit" : : "g"(proc_stack) : "memory");
}

/*激活页表*/
void page_dir_activate(struct task_struct* p_thread) {
  // 切换进程时会切换页目录，不确定上一次被调度的是否是进程，所以需要执行此函数，重新安装页目录

  // 内核进程使用的
  uint32_t pagedir_phy_addr = 0x100000;  // 需要重新填充页目录
  if (p_thread->pgdir != NULL) {         // 用户进程有自己的也目录表
    pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
  }
  // 更新cr3,页表生效
  asm volatile("movl %0,%%cr3" ::"r"(pagedir_phy_addr) : "memory");
}

/* 激活线程或进程的页表,更新 tss 中的 esp0 为进程的特权级 0 的栈 */
void process_activate(struct task_struct* p_thread) {
  ASSERT(p_thread != NULL);
  /*激活该进程或线程的页表*/
  page_dir_activate(p_thread);
  if (p_thread->pgdir) {
    /* 更新该进程的 esp0,用于此进程被中断时保留上下文 */
    update_tss_esp(p_thread);
  }
}

/*创建用户页目录*/
uint32_t* create_page_dir(void) {
  /* 用户进程的页表不能让用户直接访问到,所以在内核空间来申请 */
  uint32_t* page_dir_vaddr = get_kernel_pages(1);
  if (page_dir_vaddr == NULL) {
    console_put_str("create_page_dir: get_kernel_page failed!");
    return NULL;
  }
  // 1.复制页表
  // page_dir_vaddr + 0x300*4 ：页表目录768项
  // 0xfffff000,内核也目录表的虚拟地址
  // 1024=4*256 (内核空间的256个页表)
  memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300 * 4),
         (uint32_t*)(0xfffff000 + 0x300 * 4), 1024);
  // 2.更新页目录地址
  uint32_t new_page_dir_phy_addr =
      addr_v2p((uint32_t)page_dir_vaddr);  // 获取物理地址
  /* 页目录地址是存入在页目录的最后一项,更新页目录地址为新页目录的物理地址 */
  page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;
  return page_dir_vaddr;
}

/* 创建用户进程虚拟地址位图 */
void create_user_vaddr_bitmap(struct task_struct* user_prog) {
  user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
  // 位图要占用的页数量
  uint32_t bitmap_pg_cnt =
      DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
  user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
  user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len =
      (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
  bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

/*创建用户进程*/
void process_execute(void* filename, char* name) {
  // PCB
  struct task_struct* thread = get_kernel_pages(1);

  init_thread(thread, name, default_prio);
  create_user_vaddr_bitmap(thread);
  thread_create(thread, start_process, filename);
  thread->pgdir = create_page_dir();

  block_desc_init(thread->u_block_desc);  // 初始化用户进程内存块
  // 关闭中断
  enum intr_status old_status = intr_disable();
  ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
  list_append(&thread_ready_list, &thread->general_tag);
  ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
  list_append(&thread_all_list, &thread->all_list_tag);
  intr_set_status(old_status);
}