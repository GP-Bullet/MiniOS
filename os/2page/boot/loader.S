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

;创建页目录表和页表

    mov eax,KERNEL_START_SECTOR      ;9号扇区
    mov ebx,KERNEL_BIN_BASE_ADDR       ;0x70000
    mov ebx,200                         ;读两百个
    call rd_disk_m_32

;启用分页分三步走
    call set_pagedirtable_pagetable

    mov ebx,[gdt_ptr+2]
    or dword [ebx + 0x18 + 4],0xc0000000        ;视频段是第3个段描述符,每个描述符是8字节,故0x18 = 24，然后+4，是取出了视频段段描述符的高4字节。然后or操作，段基址最高位+c

    add dword [gdt_ptr+2],0xc0000000 
    add esp, 0xc0000000            ; 将栈指针同样映射到内核地址
    mov eax,PAGE_DIRTABLE
    mov cr3,eax   
                        ; 把页目录地址赋给cr3   (物理地址)
    ;打开cr0的pg位（第 31 位）
    mov eax,cr0
    or eax,0x80000000
    mov cr0,eax

;打开分页后，用gdt的新地址重新加载
    lgdt[gdt_ptr]

enter_kernel:
    call kernel_init
    mov esp,0xc009f000
    jmp KERNEL_ENTRY_POINT

kernel_init:
    xor eax,eax                 ;清空eax
    xor ebx,ebx                 ;清空ebx, ebx记录程序头表地址
    xor ecx,ecx                 ;清空ecx, cx记录程序头表中的program header数量
    xor edx,edx                 ;清空edx, dx 记录program header尺寸

    mov dx,[KERNEL_BIN_BASE_ADDR + 42]          ; 偏移文件42字节处的属性是e_phentsize,表示program header table中每个program header大小
    mov ebx,[KERNEL_BIN_BASE_ADDR +28]          ; 偏移文件开始部分28字节的地方是e_phoff,表示program header table的偏移，ebx中是第1 个program header在文件中的偏移量
                                                ; 其实该值是0x34,不过还是谨慎一点，这里来读取实际值
    add ebx, KERNEL_BIN_BASE_ADDR                       ; 现在ebx中存着第一个program header的内存地址
    mov cx, [KERNEL_BIN_BASE_ADDR + 44]                 ; 偏移文件开始部分44字节的地方是e_phnum,表示有几个program header
.each_segment:
    cmp byte [ebx + 0], PT_NULL		                    ; 若p_type等于 PT_NULL,说明此program header未使用。
    je .PTNULL

                                                        ;为函数memcpy压入参数,参数是从右往左依然压入.函数原型类似于 memcpy(dst,src,size)
    push dword [ebx + 16]		                        ; program header中偏移16字节的地方是p_filesz,压入函数memcpy的第三个参数:size
    mov eax, [ebx + 4]			                        ; 距程序头偏移量为4字节的位置是p_offset，该值是本program header 所表示的段相对于文件的偏移
    add eax, KERNEL_BIN_BASE_ADDR	                    ; 加上kernel.bin被加载到的物理地址,eax为该段的物理地址
    push eax				                            ; 压入函数memcpy的第二个参数:源地址
    push dword [ebx + 8]			                    ; 压入函数memcpy的第一个参数:目的地址,偏移程序头8字节的位置是p_vaddr，这就是目的地址
    call mem_cpy				                        ; 调用mem_cpy完成段复制
    add esp,12				                            ; 清理栈中压入的三个参数
.PTNULL:
   add ebx, edx				                            ; edx为program header大小,即e_phentsize,在此ebx指向下一个program header 
   loop .each_segment
   ret


     ;----------  逐字节拷贝 mem_cpy(dst,src,size) 输入:栈中三个参数(dst,src,size)
mem_cpy:
    cld 
    push ebp
    mov ebp,esp
    push ecx
    mov edi,[ebp + 8]                   ;dst    +8是因为零一个mem_cpy的返回地址和一个push ebp
    mov esi, [ebp +12]                  ;src 
    mov ecx, [ebp + 16]                 ;size
    rep movsb                           ;DS：[E]SI指向的地址处的 1、2 或 4 个字节搬到 ES：[E]DI 指向的地址处，16 位环境下源地址指针用 SI 寄存器，目的地址指针用 DI 寄存器，32 位环境下源地址则用 ESI，目的地址则用 EDI。

    pop ecx
    pop ebp
    ret



set_pagedirtable_pagetable:

;;;;创建页目录表和页表
    mov ecx,4096                    ;1M开始的4KB清零
    mov esi,0
.loop_set:               
    mov byte [PAGE_DIRTABLE + esi],0
    inc esi
    loop .loop_set

    

.init_PDE:            ;------初始化页目录表，让0号项与768号指向同一个页表，该页表管理从0开始4M的空间
    mov eax,PAGE_DIRTABLE                ;一个页表可以对应4MB空间
    add eax,0x1000
    mov ebx,eax
    or eax,PG_US_U | PG_RW_W | PG_P
    mov [PAGE_DIRTABLE + 0x0],eax        ;需要将页目录表0号和768号项写入第一个页表的位置(0x101000)及属性
    mov [PAGE_DIRTABLE + 0xc00],eax      ; 页目录项的属性RW和P位为1,US为1,表示用户属性,所有特权级别都可以访问.

    sub eax, 0x1000                             ; 使最后一个目录项指向 页目录表本身 的地址，为的是将来动态操作页表做准备(4092=4096-4)
    mov [PAGE_DIRTABLE + 4092], eax	        ;属性包含PG_US_U是为了将来init进程（运行在用户空间）访问这个页目录表项


    ; -----------初始化第一个页表，因为我们的操作系统不会超过1M，所以只用初始化256项
    ;此页表是页目录中的第 0个页目录项对应的页表，它用来分配物理地址范围 0～0x3fffff 之间的物理页，
    ;这也就是虚拟地址 0x0～0x3fffff和虚拟地址 0xc0000000～0xc03fffff 对应的物理页。其物理地址是连续分配的，即在低端 1MB 内存中，虚拟地址等于物理地址。
    mov ecx,256
    mov esi,0
    mov edx,PG_US_U | PG_RW_W | PG_P
.create_pte:
    mov [ebx+esi*4],edx
    add edx,4096
    inc esi
    loop .create_pte


 ; ---初始化页目录表769号-1022号项，769号项指向第二个页表的地址（此页表紧挨着上面的第一个页表），770号指向第三个，以此类推
    mov eax,PAGE_DIRTABLE
    add eax,0x2000                  ;第二个页表的位置
    or eax,PG_US_U | PG_RW_W | PG_P ;
    mov ebx,PAGE_DIRTABLE
    mov ecx,254
    mov esi,769
.create_kernel_pde:
    mov [ebx+esi*4],eax
    inc esi
    add eax,0x1000
    loop .create_kernel_pde  ; 循环设定254个页目录表项
    ret



;读磁盘的n个扇区函数    
rd_disk_m_32:
   				    ; eax=LBA扇区号，逻辑块地址（Logical Block Address），LBA28格式
				       ; ebx=将数据写入的内存地址
				       ; ecx=读入的扇区数
   mov esi,eax  ;可以使用32位
   mov di,cx

;1,要读取的扇区数
   mov dx,0x1f2
   mov al,cl
   out dx,al  ;输出

;恢复eax
   mov eax,esi
   
;2,将LBA地址存放0x1f3~0x1f6  0x1f6为device


;0~7
   mov dx,0x1f3
   out dx,al
;8~15
   shr eax,8
   mov dx,0x1f4
   out dx,al
;16~23
   shr eax,8
   mov dx,0x1f5   
;LBA28即0～27，  
   shr eax,8
   and al,0x0f  ;保留24～27位
   or  al,0xe0   ;高4位为1110,lba模式
   mov dx,0x1f6 
   out dx,al
;3.向0x1f7写入读状态命令，0x20
   mov dx,0x1f7
   mov al,0x20
   out dx,al
;4,检测硬盘状态
not_ready:
   ;nop 表示空操作，即什么也不做，只是为了增加延迟，相当于 sleep 了一小下，目的是减少打扰硬盘的工作。
   ;0x1f7端口，对同一端口在读写两种操作时有不同的用途，在读硬盘时，此端口中的值是硬盘的工作状态。
   nop
   in al,dx
   and al,0x88
   cmp al,0x08 ;修改标志位  
   jnz not_ready  ;不为零则说明忙，执行跳转

;5,准备好，则从0x1f0读扇区数据
   mov ax,di
   mov dx,256  ;di为要读取的扇区数，一个扇区有512字节，每次读入一个字，共需di*512/2次，所以di*256
   mul dx
   mov cx,ax

   mov dx,0x1f0
continue_read:
   in ax,dx             
   mov [ebx],ax         ;与rd_disk_m_16相比，就是把bx改成了ebx
   add ebx,2
   loop continue_read

   ret