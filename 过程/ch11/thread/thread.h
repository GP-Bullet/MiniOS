#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "stdint.h"
#include "list.h"
#include "memory.h"

// 自定义通用函数类型, 在线程函数中作为形参类型
typedef void thread_func(void*);

// 进程或线程状态
enum task_status {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

// 中断栈 intr_stack
// 用于中断发生时保护程序(线程或进程)的上下文环境:
// 进程或线程中断后，会按照此结构压入上下文寄存器
// 线程进入中断后, 位于 kernel.S 中的中断代码会通过此栈来保存上下文
struct intr_stack {
    uint32_t vec_no; // kernel.S 宏 VECTOR 中 %1 压入的中断号
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;	 // 虽然pushad把esp也压入,但esp是不断变化的,所以会被popad忽略
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    // 以下由 cpu 从低特权级进入高特权级时压入
    uint32_t err_code;		 // err_code会被压入在eip之后
    void (*eip) (void);
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

// 线程栈 thread_stack
// 用在 switch_to 时保存线程环境
struct thread_stack {
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    // 线程第一次执行时, eip 指向待调用的函数 kernel_thread
    // 其他时候, eip 是指向 switch_to 的返回地址
    void (*eip) (thread_func* func, void* func_arg);

    // 以下仅供第一次被调度上 cpu 时使用
    // unused_ret 只为占位置充数为返回地址
    void (*unused_retaddr);
    thread_func* function; 		// 由 kernel_thread 所调用的函数名
    void* func_arg; 			// 由 kernel_thread 所调用的函数所需的参数
};


// 进程或线程的 PCB
struct task_struct {
    uint32_t* self_kstack;         // 各内核线程都用自己的内核栈
    enum task_status status;
    char name[16];
    uint8_t priority;              // 线程优先级
    uint8_t ticks;                 // 每次在处理器上执行的时间嘀嗒数
    uint32_t elapsed_ticks;        // 此任务上 cpu 运行后至今占用了多少嘀嗒数

    struct list_elem general_tag;   // 用于线程在一般队列中的结点
    struct list_elem all_list_tag;  // 用于线程在 thread_all_list 中的结点

    uint32_t* pgdir;                // 进程自己页表的虚拟地址
    struct virtual_addr userprog_vaddr; // 用户进程的虚拟地址
    uint32_t stack_magic;           // 栈的边界标记, 用于检测栈的溢出
};
extern struct list thread_ready_list;
extern struct list thread_all_list;

void thread_create(struct task_struct* pthread, thread_func function, void* func_arg);
void init_thread(struct task_struct* pthread, char* name, int prio);
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg);
struct task_struct* running_thread(void);
void schedule(void);
void thread_init(void);
#endif
