#include "init.h"

#include "print.h"
#include "fs.h"
#include "ide.h"
#include "init.h"
#include "interrupt.h"
#include "keyboard.h"
#include "memory.h"
#include "console.h"
#include "syscall_init.h"
#include "thread.h"
#include "timer.h"
#include "tss.h"
/*负责初始化所有模块 */
void init_all() {
  put_str("init_all\n");
  idt_init();       // 初始化中断
  timer_init();     // 初始化PIT
  mem_init();       // 内存池初始化
  keyboard_init();  // 键盘初始化
  tss_init();       // tss初始化
  thread_init();    // 初始化线程环境
  console_init();   //
  syscall_init();
  ide_init();  // 硬盘初始化
  filesys_init();
}