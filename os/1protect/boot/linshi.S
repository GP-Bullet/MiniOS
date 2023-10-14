mov byte [gs:0x00],'2'
mov byte [gs:0x01],0xA4             ; A表示绿色背景闪烁，4表示前景色为红色

mov byte [gs:0x02],' '
mov byte [gs:0x03],0xA4

mov byte [gs:0x04],'L'
mov byte [gs:0x05],0xA4   

mov byte [gs:0x06],'O'
mov byte [gs:0x07],0xA4

mov byte [gs:0x08],'A'
mov byte [gs:0x09],0xA4

mov byte [gs:0x0a],'D'
mov byte [gs:0x0b],0xA4

mov byte [gs:0x0c],'E'
mov byte [gs:0x0d],0xA4

mov byte [gs:0x0e],'R'
mov byte [gs:0x0f],0xA4




























ards_buf times 244 db 0                                 ;人工对齐:total_mem_bytes4字节+gdt_ptr6字节+ards_buf244字节+ards_nr2,共256字节
ards_nr dw 0		                                    ;用于记录ards结构体数量


loader_start:
                                                        ;-------  int 15h eax = 0000E820h ,edx = 534D4150h ('SMAP') 获取内存布局  -------

    xor ebx, ebx		                                ;第一次调用时，ebx值要为0
    mov edx, 0x534d4150	                                ;edx只赋值一次，循环体中不会改变
    mov di, ards_buf	                                ;ards结构缓冲区
.e820_mem_get_loop:	                                    ;循环获取每个ARDS内存范围描述结构
    mov eax, 0x0000e820	                                ;执行int 0x15后,eax值变为0x534d4150,所以每次执行int前都要更新为子功能号。
    mov ecx, 20		                                    ;ARDS地址范围描述符结构大小是20字节
    int 0x15
    add di, cx		                                    ;使di增加20字节指向缓冲区中新的ARDS结构位置
    inc word [ards_nr]	                                ;记录ARDS数量
    cmp ebx, 0		                                    ;若ebx为0且cf不为1,这说明ards全部返回，当前已是最后一个
    jnz .e820_mem_get_loop

                                                        ;在所有ards结构中，找出(base_add_low + length_low)的最大值，即内存的容量。
    mov cx, [ards_nr]	                                ;遍历每一个ARDS结构体,循环次数是ARDS的数量
    mov ebx, ards_buf 
    xor edx, edx		                                ;edx为最大的内存容量,在此先清0
.find_max_mem_area:	                                    ;无须判断type是否为1,最大的内存块一定是可被使用
    mov eax, [ebx]	                                    ;base_add_low
    add eax, [ebx+8]	                                ;length_low
    add ebx, 20		                                    ;指向缓冲区中下一个ARDS结构
    cmp edx, eax		                                ;冒泡排序，找出最大,edx寄存器始终是最大的内存容量
    jge .next_ards
    mov edx, eax		                                ;edx为总内存大小
.next_ards:
    loop .find_max_mem_area

    mov [total_mem_bytes], edx	                        ;将内存换为byte单位后存入total_mem_bytes处。
