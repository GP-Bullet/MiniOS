    %include"boot.inc"
section loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR
    jmp loader_start

;构建gdt和内部描述符

    GDT_BASE:dd 0x00000000
            dd 0x00000000
    CODE_DESC: dd 0x0000FFFF
            dd DESC_CODE_HIGH4
    DATA_DESC: dd 0x0000FFFF
            dd DESC_DATA_HIGH4
    VEDIO_DESC:dd 0x8000_0007           ;limit=(0xbffff-0xb8000)/4k=0x7
            dd DESC_VIDEO_HIGH4

    GDT_SIZE  equ $ - GDT_BASE        ;起始地址  
    GDT_LIMIT equ GDT_SIZE - 1
    times 60 dq 0        ;dq 四字是8字节 
    SELECTOR_CODE equ (0x0001<<3) +TI_GDT +RPL0            ;为什么都是RPL0
    SELECTOR_DATA equ (0x0002<<3) +TI_GDT +RPL0
    SELECTOR_VIDEO equ (0x0003<<3) +TI_GDT +RPL0

total_mem_bytes dd 0	; total_mem_bytes用于保存内存容量,由于此处+  jmp loader_start 3字节，所以此处用xp 0xb03

;定义gdt的指针（地址），前2字节是gdt界限，后4字节是gdt起始地址
    gdt_ptr dw GDT_LIMIT
        dd GDT_BASE
        
;;;;
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



   

;三步进入保护模式
;1.打开 A20Gate ，将端口 0x92 的第 1 位置 1 就可以了 
   in al,0x92
   or al,0000_0010b
   out 0x92,al
;2.加载GDT
   lgdt [gdt_ptr]
;3. cr0寄存器位置1 ，E 为 0 表示在实模式下运行，PE 为 1 表示在保护模式下运行。

  mov eax,cr0
  or eax,0x0000_0001
  mov cr0,eax

;刷新流水线
;由于在实模式下时，指令按照 16 位指令格式来译码，第 78～82 行既有 16 位指令，又有 32
;位指令，所以流水线把 32 位指令按照 16 位译码就会出错。解决这问题的方法就是用无跳转指令清空流水线。

   jmp dword SELECTOR_CODE:protect_start

[bits 32]
protect_start:
   mov ax,SELECTOR_DATA
   mov ds,ax
   mov es,ax
   mov ss,ax
   mov esp,LOADER_STACK_TOP
   mov ax,SELECTOR_VIDEO
   mov gs,ax

   mov byte [gs:160],'P'
   jmp $
   