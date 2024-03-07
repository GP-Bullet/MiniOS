#ifndef USERPROG_PROCESS
#define USERPROG_PROCESS
#include "thread.h"
#include "stdint.h"
#define USER_STACK3_VADDR (0xc0000000 - 0x1000)
#define USER_VADDR_START 0x8048000
#define default_prio 20
uint32_t* create_page_dir(void);
void create_user_vaddr_bitmap(struct task_struct* user_prog);
void process_activate(struct task_struct* p_thread);
/*创建用户进程*/
void process_execute(void* filename, char* name);
void page_dir_activate(struct task_struct* p_thread);
#endif /* USERPROG_PROCESS */
