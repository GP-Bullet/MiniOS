#ifndef DEVICE_IOQUEUE
#define DEVICE_IOQUEUE
#include "stdint.h"
#include "sync.h"
#include "thread.h"

#define bufsize 2048

/*环形队列*/
struct ioqueue {
  struct lock lock;
  struct task_struct* producer;
  struct task_struct* consumer;

  char buf[bufsize];  // 缓冲区
  int32_t head;       // 队头
  int32_t tail;       // 队尾
};

void ioqueue_init(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue* ioq);
bool ioq_empty(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char byte);
uint32_t ioq_length(struct ioqueue* ioq);
#endif /* DEVICE_IOQUEUE */
