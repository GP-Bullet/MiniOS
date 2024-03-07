#include "timer.h"

#include "debug.h"
#include "interrupt.h"
#include "io.h"
#include "print.h"
#include "thread.h"

#define IRQ0_FREQUENCY 100                            // 一秒一百次
#define mil_seconds_per_intr (1000 / IRQ0_FREQUENCY)  // 一次时钟中断多少毫秒

uint32_t ticks;  // ticks 是内核自中断开启以来总共的嘀嗒数
// 设置控制字寄存器，并且设置计数初始寄存器
static void frequency_set(uint8_t counter_port, uint8_t counter_on, uint8_t rwl,
                          uint8_t counter_mode, uint16_t counter_value) {
  uint8_t pit = counter_on << 6 | rwl << 4 | counter_mode << 1;
  // 设置控制寄存器
  outb(PIT_CONTROL_PORT, pit);

  // 设置计数初始寄存器
  outb(counter_port, (uint8_t)counter_value);       // 低8位
  outb(counter_port, (uint8_t)counter_value >> 8);  // 高8位
}

/*时钟中断处理函数*/
static void intr_timer_handler(void) {
  struct task_struct* cur_thread = running_thread();
  ASSERT(cur_thread->stack_magic == STACK_MAGIC);  // 检查PCB栈是否溢出

  cur_thread->elapsed_ticks++;  // 记录次线程占用CPU的时间
  ticks++;
  if (cur_thread->ticks == 0) {  // 若进程时间片用完,就开始调度新的进程上 cpu
    schedule();                  // 进行调度
  } else {
    cur_thread->ticks--;  // 将当前进程的时间片-1
  }
}

// 初始化PIT8253
void timer_init() {
  put_str("time_init start\n");
  /* 设置 8253 的定时周期,也就是发中断的周期 */
  frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE,
                COUNTER0_VALUE);

  register_handler(0x20, intr_timer_handler);  // 注册中断处理函数
  put_str("timer_init done\n");
}

/* 以 tick 为单位的 sleep,任何时间形式的 sleep 会转换此 ticks 形式 */
static void tisks_to_sleep(uint32_t sleep_ticks) {
  uint32_t start_ticks = ticks;

  /* 若间隔的 ticks 数不够便让出 cpu */
  while (ticks - start_ticks < sleep_ticks) {
    thread_yield();
  }
}

/*以毫秒为单位的sleep*/
void mtime_sleep(uint32_t m_seconds) {
  uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
  ASSERT(sleep_ticks > 0);
  tisks_to_sleep(sleep_ticks);
}

/*以秒为单位的sleep*/
void stime_sleep(uint32_t s_seconds) {
  uint32_t sleep_ticks = DIV_ROUND_UP(s_seconds * 1000, mil_seconds_per_intr);
  ASSERT(sleep_ticks > 0);
  tisks_to_sleep(sleep_ticks);
}