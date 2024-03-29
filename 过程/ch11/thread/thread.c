#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "interrupt.h"
#include "debug.h"
#include "print.h"
#include "process.h"

#define PG_SIZE 4096

struct task_struct* main_thread; // 主线程 PCB
struct list thread_ready_list; // 就绪队列
struct list thread_all_list; // 所有任务队列
static struct list_elem* thread_tag; // 用于保存队列中的线程结点

extern void switch_to(struct task_struct* cur, struct task_struct* next);

// 获取当前线程 PCB 指针
struct task_struct* running_thread(void) {
    uint32_t esp;
    asm("mov %%esp, %0" : "=g" (esp));
    // 取 esp 整数部分, 即 PCB 起始地址
    return (struct task_struct*)(esp & 0xfffff000);
}

// 由 kernel_thread 去执行 function(func_arg)
static void kernel_thread(thread_func* function, void* func_arg) {
    // 执行 function 前需要开中断,
    // 避免后面的时钟中断被屏蔽, 而无法调度其它线程
    intr_enable();
    //while(1);   
    function(func_arg);
}

// 初始化线程栈 thread_stack，将待执行的函数和参数放到 thread_stack 中相应的位置
void thread_create(struct task_struct* pthread, //待创建的线程指针 
                   thread_func function,        //线程函数
                   void* func_arg) {            //线程参数
    // 先预留中断使用栈的空间
    pthread->self_kstack -= sizeof(struct intr_stack);

    // 在留出线程栈空间
    pthread->self_kstack -= sizeof(struct thread_stack);
    struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
    kthread_stack->eip = kernel_thread;
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;
    kthread_stack->ebp = 0;
    kthread_stack->ebx = 0;
    kthread_stack->esi = 0;
    kthread_stack->edi = 0;
}

// 初始化线程基本信息
// 待初始化线程指针（PCB），线程名称，线程优先级
void init_thread(struct task_struct* pthread, char* name, int prio) {
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name);

    if(pthread == main_thread) {
        // 把 main 函数也封装成一个线程, main 函数是一直运行的
        pthread->status = TASK_RUNNING;
    } else {
        pthread->status = TASK_READY;
    }
    // self_kstack 是线程自己在内核态下使用的栈顶地址
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->priority = prio;
    //pthread->priority = prio;
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;
    pthread->stack_magic = 0x19870916; // 自定义魔数
}

// 线程所执行的函数是 function(func_arg)
struct task_struct* thread_start(char* name,            //线程名
                                 int prio,              //优先级
                                 thread_func function,  //要执行的函数
                                 void* func_arg)        //函数的参数
{
    // PCB 都位于内核空间, 包括用户进程的 PCB 也是在内核空间
    struct task_struct* thread = get_kernel_pages(1);   //申请一页内核空间存放PCB

    init_thread(thread, name, prio);                    //初始化线程
    thread_create(thread, function, func_arg);          //创建线程

    // 确保之前不在队列中
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    // 加入就绪线程队列
    list_append(&thread_ready_list, &thread->general_tag);
    // 确保之前不在队列中
    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    // 加入全部线程队列
    list_append(&thread_all_list, &thread->all_list_tag);

    return thread;
}

// 将 main 函数封装为主线程
static void make_main_thread(void) {
    main_thread = running_thread();
    init_thread(main_thread, "main", 31);

    ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
    list_append(&thread_all_list, &main_thread->all_list_tag);
}

// 实现线程调度
void schedule(void) {
    ASSERT(intr_get_status() == INTR_OFF);

    struct task_struct* cur = running_thread();
    if(cur->status == TASK_RUNNING) {
        // 若此线程只是 CPU 时间片到了, 将其加入到就绪队尾
        ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
        list_append(&thread_ready_list, &cur->general_tag);
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
    } else {
        // 若此线程阻塞, 不需要将其加入队列
    }

    ASSERT(!list_empty(&thread_ready_list));
    thread_tag = NULL;
    thread_tag = list_pop(&thread_ready_list);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    next->status = TASK_RUNNING;

    process_activate(next);
    //while(1);
    switch_to(cur, next);
}

// 初始化线程环境
void thread_init(void) {
    put_str("thread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    // 将当前 main 函数创建为线程
    make_main_thread();
    put_str("thread_init done\n");
}


// 当前线程将自己阻塞, 标志其状态为 stat
void thread_block(enum task_status stat) {
    ASSERT(stat == TASK_BLOCKED || 
           stat == TASK_WAITING || 
           stat == TASK_HANGING);
    enum intr_status old_status = intr_disable();
    struct task_struct* cur_thread = running_thread();
    cur_thread->status = stat;
    schedule(); // 将当前线程换下处理器
    intr_set_status(old_status);
}

// 将线程解除阻塞
void thread_unblock(struct task_struct* pthread) {
    enum intr_status old_status = intr_disable();
    ASSERT(pthread->status == TASK_BLOCKED || 
           pthread->status == TASK_WAITING || 
           pthread->status == TASK_HANGING);
    ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
    // 放在就绪队列最前面, 使其尽快得到调度
    list_push(&thread_ready_list, &pthread->general_tag);
    pthread->status = TASK_READY;
    intr_set_status(old_status);
}