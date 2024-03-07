#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
#include "console.h"

void k_thread_a(void*);
void k_thread_b(void*); 
int main(){
	put_str("\nI am kernel\n");
	init_all();
	//asm volatile("sti");	//开启中断
	
	//void* addr = get_kernel_pages(5);
	//put_str("\n get_kernel_page start vaddr is:");
	//put_int((uint32_t)addr);
	//put_str("\n");

	//thread_start("k_thread_a",31,k_thread_a,"ArgA ");
	//thread_start("k_thread_b",20,k_thread_b,"ArgB ");
    intr_enable(); // 打开中断, 使时钟中断起作用


	while(1);//{
	//	console_put_str("Main ");
	//}
	return 0;
}

void k_thread_a(void* arg){
	char* para = arg;
	while(1){
		console_put_str(para);
	}
}

void k_thread_b(void* arg) {
    char* para = arg;
    while(1) {
        console_put_str(para);
    }
}