#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall-init.h"

/* 负责初始化所有模块 */
void init_all(){
	put_str("init_all\n");
	idt_init();		// 初始化 中断
	mem_init();		// 初始化内存池
	thread_init();	// 初始化线程
	timer_init();	// 初始化 PIT
	console_init();	// 初始化终端
	keyboard_init();// 初始化键盘
	tss_init();		// 初始化 TSS
	syscall_init();	// 初始化系统调用

    //intr_enable();      // 后面的 ide_init 需要打开中断
    ide_init();         // 初始化硬盘

}

