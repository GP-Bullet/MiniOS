#ifndef USERPROG_WAIT_EXIT
#define USERPROG_WAIT_EXIT
#include "stdint.h"
#include "thread.h"
void sys_exit(int32_t status);
pid_t sys_wait(int32_t* status);
#endif /* USERPROG_WAIT_EXIT */
