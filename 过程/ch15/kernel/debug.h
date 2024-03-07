#ifndef __KERNEL_DEBUG_H 
#define __KERNEL_DEBUG_H 
void panic_spin(char* filename, int line, const char* func, const char* condition);

/*__VA_ARGS__
* _ VA_ARGS_ 是预处理器所支持的专用标识符。
* 代表所有与省略号相对应的参数。
* ... 表示定义的宏其参数可变。 */
#define PANIC(...) panic_spin(__FILE__,__LINE__,__func__,__VA_ARGS__)

#ifdef NDEBUG			//不用调试的时候，就删掉
	#define ASSERT(CONITION)((void)0)
#else
#define ASSERT(CONITION) 	\
	if(CONITION){}else{ 	\
	PANIC(#CONITION);}	//#号让宏的参数转换成 字符串 常量

#endif	/*__NDEBUF*/
#endif  /*__KERNEL_DEBUG_H*/
