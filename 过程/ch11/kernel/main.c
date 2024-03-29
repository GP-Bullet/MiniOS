#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "process.h"

void k_thread_a(void*);
void k_thread_b(void*); 
void u_prog_a(void);
void u_prog_b(void);
int test_var_a = 0;
int test_var_b = 0;


int main(){
	put_str("\nI am kernel\n");
	init_all();
	//asm volatile("sti");	//开启中断

	thread_start("k_thread_a",31,k_thread_a,"A_ ");
	//thread_start("k_thread_b",31,k_thread_b,"B_ ");
    process_execute(u_prog_a, "u_prog_a");
	//process_execute(u_prog_b, "user_prog_b");
    intr_enable(); // 打开中断, 使时钟中断起作用
	while(1);
	return 0;
}


void k_thread_a(void* arg){
	char* para = arg;
	while(1){
        console_put_str(" v_a:0x");
        console_put_int(test_var_a);
        console_put_str("\n");
	}
}

void k_thread_b(void* arg) {
    char* para = arg;
    while(1) {
        //console_put_str(" v_b:0x");
        //console_put_int(test_var_b);
    }
}

// 测试用户进程
void u_prog_a(void) {
    while(1) {
        test_var_a++;
        //put_str("yes\n");
    }
}

// 测试用户进程
void u_prog_b(void) {
    while(1) {
        test_var_b++;
    }
}
