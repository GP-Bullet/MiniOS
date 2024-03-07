#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H
#include "thread.h"
#include "list.h"
#include "stdint.h"

struct semaphore {
    uint8_t value;      
    struct list waiters;         //管理所有阻塞的线程
};

struct lock {
    struct task_struct* holder;   //记录谁把信号量申请走了
    struct semaphore semaphore;   //锁管理信号量
    uint32_t holder_repeat_nr;    //有时候线程拿到了信号量，但是线程内部不止一次使用该信号量对应公共资源，就会不止一次申请锁
};
//内外层函数在释放锁时就会对一个锁释放多次，所以必须要记录重复申请的次数
#endif 