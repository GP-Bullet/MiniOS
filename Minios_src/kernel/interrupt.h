#ifndef KERNEL_INTERRUPT
#define KERNEL_INTERRUPT
#include "stdint.h"

typedef void* intr_handler;  // 用于指向中断处理函数的地址

// 这里用的可编程中断控制器是8259A
#define PIC_M_CTRL 0x20  // 主片的控制端口是0x20
#define PIC_M_DATA 0x21  // 主片的数据端口是0x21
#define PIC_S_CTRL 0xa0  // 从片的控制端口是0xa0
#define PIC_S_DATA 0xa1  // 从片的数据端口是0xa1

#define IDT_DESC_CNT 0x81  // 支持的中断数

#define EFLAGS_IF 0x00000200  // eflags寄存器中的IF位为1

// 获取eflags寄存器的内容(先将eflags入栈，再pop)
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl;popl %0" : "=g"(EFLAG_VAR))
/*中断初始化*/
void idt_init();

// 定义两种状态
enum intr_status {
  INTR_OFF,  // 关闭中断
  INTR_ON    // 中断打开
};

/* 获取当前中断状态 */
enum intr_status intr_get_status();

/*开启中断并返回中断前的状态*/
enum intr_status intr_enable();

/*关闭中断并返回中断前的状态*/
enum intr_status intr_disable();

/*设置中断状态*/
enum intr_status intr_set_status(enum intr_status status);

/*中断处理程序的注册*/
void register_handler(uint8_t vector_no, intr_handler function);

#endif /* KERNEL_INTERRUPT */
