#include "sync.h"

#include "debug.h"
#include "interrupt.h"
#include "list.h"
#include "print.h"
#include "thread.h"

/*初始化信号量*/
void sema_init(struct semaphore* psema, uint8_t value) {
  psema->value = value;
  list_init(&psema->waiters);
}

/*初始化锁*/
void lock_init(struct lock* plock) {
  plock->holder = NULL;
  plock->holder_repeat_nr = 0;
  sema_init(&plock->semaphore, 1);
}

/*信号量down操作*/
void sema_down(struct semaphore* psema) {
  /*关中断保证原子操作*/
  enum intr_status old_status = intr_disable();  // 关闭中断
  while (psema->value == 0) {
    // 此处要使用while,唤醒后再次判断(因为锁可能再次被抢用)
    // 若 value 为 0,表示已经被别人持有
    ASSERT(!elem_find(&psema->waiters, &running_thread()->general_tag));
    if (elem_find(&psema->waiters, &running_thread()->general_tag)) {
      PANIC("sema_down: thread blocked has been in waiters_list\n");
    }
    list_append(&psema->waiters, &running_thread()->general_tag);
    thread_block(TASK_BLOCKED);
  }
  /* 若 value 为 1 或被唤醒后成功获得锁,会执行下面的代码,也就是获得了锁*/
  psema->value--;
  ASSERT(psema->value == 0);
  // 恢复之前状态
  intr_set_status(old_status);
}

/*信号量up操作*/
void sema_up(struct semaphore* psema) {
  /*关中断，保证原子操作*/
  enum intr_status old_status = intr_disable();  // 关闭中断
  ASSERT(psema->value == 0);
  if (!list_empty(&psema->waiters)) {
    // 唤醒一个等待的线程
    struct task_struct* thread_blocked =
        elem2entry(struct task_struct, general_tag, list_pop(&psema->waiters));
    thread_unblock(thread_blocked);
  }
  psema->value++;
  ASSERT(psema->value == 1);
  // 恢复之前的中断状态
  intr_set_status(old_status);
}

/*获取锁plock*/
void lock_acquire(struct lock* plock) {
  if (plock->holder != running_thread()) {
    sema_down(&plock->semaphore);  // 对信号量 P 操作,原子操作
    plock->holder = running_thread();
    ASSERT(plock->holder_repeat_nr == 0);
    plock->holder_repeat_nr = 1;
  } else {  // 已经加锁，未释放再次加锁
    plock->holder_repeat_nr++;
  }
}

/*释放锁plock*/
void lock_release(struct lock* plock) {
  ASSERT(plock->holder == running_thread());  // 取保自己是锁的持有者
  if (plock->holder_repeat_nr > 1) {
    plock->holder_repeat_nr--;
    return;
  }
  ASSERT(plock->holder_repeat_nr == 1);
  plock->holder = NULL;
  plock->holder_repeat_nr = 0;
  sema_up(&plock->semaphore);  // 信号量的 V 操作,也是原子操作
}