#ifndef USERPROG_TSS
#define USERPROG_TSS
#include "thread.h"
void tss_init();
void update_tss_esp(struct task_struct* pthread);
#endif /* USERPROG_TSS */
