#ifndef DEVICE_TIMER
#define DEVICE_TIMER
#include "stdint.h"
#define IRQ0_FREQUENCY 100       // IRQ0的频率(时钟中断频率)
#define INPUT_FREQUENCY 1193180  // 作脉冲信号频率
#define COUNTER0_VALUE INPUT_FREQUENCY / IRQ0_FREQUENCY  // 初始值

#define COUNTER0_PORT 0x40  // 是计数器0的操作端口号

// 控制字寄存器配置项
#define COUNTER0_NO 0       // 选择计数器(选择计数器0)
#define COUNTER_MODE 2      // 选择工作方式（方式2）
#define READ_WRITE_LATCH 3  // 选择读写方式（先读写低，再读写高）
#define PIT_CONTROL_PORT 0x43  // 控制字寄存器操作端口

// 初始化PIT8253
void timer_init();
void mtime_sleep(uint32_t m_seconds);
void stime_sleep(uint32_t s_seconds);
#endif /* DEVICE_TIMER */
