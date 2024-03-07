#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
#include "stdint.h"

typedef void* intr_handler;
void idt_init();

// 定义中断的两种状态
enum intr_status{
	INTR_OFF,	//值为 0，表示关闭中断
	INTR_ON		//值为 1，表示开启中断
};

enum intr_status intr_enable();
enum intr_status intr_disable();
enum intr_status intr_set_status(enum intr_status status);
enum intr_status intr_get_status();

#endif


