[bits 32]
section .text
global switch_to
switch_to:
    ;栈中此处是返回地址
    push esi
    push edi
    push ebx 
    push ebp 

    mov eax, [esp+20]  ;获取参数cur
    mov [eax], esp     ;保存栈顶指针(task_struct的self_kstack字段)



;------- 以上是备份当前线程的环境,下面是恢复下一个线程的环境 ---------

    mov eax, [esp+24] ;获取参数next
    mov esp, [eax]    ; 获取栈指针

    pop ebp
    pop ebx 
    pop edi 
    pop esi 
    ret    ;会返回到 kernel_thread    