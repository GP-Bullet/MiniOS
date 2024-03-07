#include "ioqueue.h"

#include "debug.h"
#include "global.h"
#include "interrupt.h"
#include "thread.h"

/*初始化io队列ioq*/
void ioqueue_init(struct ioqueue* ioq) {
  lock_init(&ioq->lock);  // 初始化iod队列的锁
  ioq->producer = ioq->consumer = NULL;
  ioq->head = ioq->tail = 0;
}

/*返回缓冲区的下一个位置*/
static int32_t next_pos(int32_t pos) { return (pos + 1) % bufsize; }

/*判断队列是否已满*/
bool ioq_full(struct ioqueue* ioq) {
  ASSERT(intr_get_status() == INTR_OFF);  // 确保中断关闭
  return next_pos(ioq->head) == ioq->tail;
}

/*判断队列是否为空*/
bool ioq_empty(struct ioqueue* ioq) {
  ASSERT(intr_get_status() == INTR_OFF);  // 确保中断关闭
  return ioq->head == ioq->tail;
}

/*使当前生产者或消费者在此缓冲区上等待*/
static void ioq_wait(struct task_struct** waiter) {
  ASSERT(waiter != NULL && *waiter == NULL);
  *waiter = running_thread();
  thread_block(TASK_BLOCKED);
}

/*唤醒waiter*/
static void wakeup(struct task_struct** waiter) {
  ASSERT(waiter != NULL && *waiter != NULL);
  thread_unblock(*waiter);
  *waiter = NULL;
}

/*获取字符*/
char ioq_getchar(struct ioqueue* ioq) {
  ASSERT(intr_get_status() == INTR_OFF);
  while (ioq_empty(ioq)) {
    // 如果该线程wait后，其他线程会那不到锁，也会进入block，这样做可以防止惊群效应
    lock_acquire(&ioq->lock);
    ioq_wait(&ioq->consumer);
    lock_release(&ioq->lock);
  }
  char byte = ioq->buf[ioq->tail];
  ioq->tail = next_pos(ioq->tail);

  if (ioq->producer != NULL) {
    wakeup(&ioq->producer);  // 唤醒生产者
  }

  return byte;
}

void ioq_putchar(struct ioqueue* ioq, char byte) {
  ASSERT(intr_get_status() == INTR_OFF);
  while (ioq_full(ioq)) {
    // 如果该线程wait后，其他线程会那不到锁，也会进入block
    lock_acquire(&ioq->lock);
    ioq_wait(&ioq->producer);
    lock_release(&ioq->lock);
  }
  ioq->buf[ioq->head] = byte;
  ioq->head = next_pos(ioq->head);

  if (ioq->consumer != NULL) {
    wakeup(&ioq->consumer);  // 唤醒消费者
  }
}

/* 返回环形缓冲区中的数据长度 */
uint32_t ioq_length(struct ioqueue* ioq) {
  uint32_t len = 0;
  if (ioq->head >= ioq->tail) {
    len = ioq->head - ioq->tail;
  } else {
    len = bufsize - (ioq->tail - ioq->head);
  }
  return len;
}