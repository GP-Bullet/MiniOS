%include"boot.inc"
  section loader vstart=LOADER_BASE_ADDR
  
;构建gdt及其内部描述符
  GDT_BASE:  dd 0x00000000  ;第0个段描述符没有用
             dd 0x00000000  
  CODE_DESC: dd 0x0000FFFF  ;代码段描述符
             dd DESC_CODE_HIGH4

  DATA_STACK_DESC: dd 0x0000FFFF ;数据段和桟段描述符
                   dd DESC_DATA_HIGH4
  VIDEO_DESC:  dd 0x80000007   ;显存段描述符 显存文本模式(0xb8000~0xbffff) 0xbffff-0xb8000=0x7fff/4k=7，limit只需要7 
               dd DESC_VIDEO_HIGH4


  GDT_SIZE  equ $-GDT_BASE  ;GDT大小
  GDT_LIMIT equ GDT_SIZE - 1 ;GDT界限(大小-1)
  times 60 dq 0  ;;此处预留60个描述符空位
  
  ;初始化选择子
  SELECTOR_CODE  equ (0x0001<<3) + TI_GDT +RPL0
  SELECTOR_DATA  equ (0x0002<<3) + TI_GDT +RPL0
  SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT +RPL0

  total_mem_bytes dd 0 ;用来保存内存容量，以字节为单位(计算可得知此处地址为0xb03)
  
  ;初始化GDT_ptr指针(存放在GDTR寄存器)
  ;低16位存放GDT界限，高32位存放起始地址
  gdt_ptr dw GDT_LIMIT
          dd GDT_BASE

  ards_buf times 244 db 0;存放ARDS
  ards_nr dw 0 ;ARDS的数量
  
loader_start:
;---------------进行内存检查---------------
 xor ebx,ebx ;ebx置0
 mov edx, 0x534d4150 ;('SMAP')
 mov di, ards_buf  ;ards结构缓冲区
.e820_mem_get_loop:
  mov eax,0x0000e820 ;0x15的子功能号 ，获取内存容量
  mov ecx,20  ;ARDS 地址范围描述符结构大小是 20 字节
  int 0x15
  jc  .e820_failed_so_try_e801 ;若 cf 位为 1 则有错误发生,尝试 0xe801 子功能
  add di,cx  ;移动执行下一片存放ARDS的缓存
  inc word [ards_nr] ;指定的内存地址中的16位值进行加一操作
  cmp ebx,0  ;若 ebx 为 0 且 cf 不为 1,这说明 ards 全部返回,当前已是最后一个
  jnz .e820_mem_get_loop ;如果不是最后一个，循环继续

  mov cx,[ards_nr] ;先从内存获取ARDS的个数(循环因子，loop遍历次数)
  mov ebx,ards_buf
  xor edx,edx  ;清0后面用来存放最大的内存容量
.find_max_mem_area:
  ;无需判断 type 是否为 1,最大的内存块一定是可被使用的
  ;base_add_low+length_low
  mov eax,[ebx]  ;base_add_low
  add eax,[ebx+8] ;length_low
  add ebx,20  ;移动到下一个ARDS
  cmp edx,eax ;如果edx的值大于或等于eax的值，则标志位寄存器中的ZF（零标志位）会被清除
  jge .next_ards ;如果标志位寄存器中的ZF被清除则跳转
  mov edx,eax ;更新最大值
.next_ards:
  loop .find_max_mem_area
  jmp .mem_get_ok

.e820_failed_so_try_e801:
  mov ax,0xe801
  int 0x15
  jc .e801_failed_so_try88;若当前 e801 方法失败,就尝试 0x88 方法
  ;1.先算出低15MB
  mov cx,0x400 ;1024 单位换算需要用到
  ;因为此处是16位乘法，所以高16为放在dx,低16放在ax,下面是为了获取完整乘积
  mul cx  ;单位换算。ax*cx
  shl edx,16 ;左移16位
  and eax,0x0000FFFF;只保留底16位
  or edx,eax
  add edx,0x100000 ;是获取到的内存总比实际大小少 1MB,故在此“补偿”
  mov esi,edx ;备份

  ;2. 再将 16MB 以上的内存转换为 byte 为单位，寄存器 bx 和 dx 中是以 64KB 为单位的内存数量
  xor eax,eax ;置零
  mov ax,bx
  mov ecx,0x10000 ;1024*64
  mul ecx 
  ;因为此方法只能测出4GB内存，32位寄存器可以存下，所以乘积只有低32位在eax
  add esi,eax ;相加
  mov edx,esi
  jmp .mem_get_ok

.e801_failed_so_try88:
   ;int 15后,ax存入的是以KB为单位的内存容量
  mov ah,0x88
  int 0x15
  jc .error_hlt
  and eax,0x0000FFFF ;只保留低16位
  mov cx, 0x400
  mul cx
  shl edx,16
  or edx,eax
  add edx,0x100000  ;0x88子功能只会返回 1MB 以上的内存,故实际内存大小要加上 1MB

.mem_get_ok:
  mov [total_mem_bytes], edx

;---------------打开保护模式---------------
 ;准备打开保护模式
 ;1 打开 A20(将端口0x92的第1位置1)
 in al,0x92
 or al ,0000_0010B
 out 0x92,al

 ;2 加载 gdt
 lgdt [gdt_ptr]

 ;3 将 cr0 的 pe 位置 1(CR0寄存器的PE位置1)
 mov eax,cr0
 or eax ,0x00000001
 mov cr0,eax
  
 ;利用jmp清空流水线
 jmp dword SELECTOR_CODE:p_mode_start

.error_hlt:		      ;出错则挂起
   hlt

[bits 32] ;开启32位
p_mode_start:
 ;初始化寄存器
  mov ax,SELECTOR_DATA
  mov ds,ax
  mov es,ax
  mov ss,ax
  mov esp,LOADER_STACK_TOP
  mov ax,SELECTOR_VIDEO
  mov gs,ax

; -------------------------   加载kernel  ----------------------
   mov eax, KERNEL_START_SECTOR        ; kernel.bin所在的扇区号
   mov ebx, KERNEL_BIN_BASE_ADDR       ; 从磁盘读出后，写入到ebx指定的地址
   mov ecx, 200			       ; 读入的扇区数

   call rd_disk_m_32

  ;创建页目录及页表并初始化页内存位图
  call setup_page
  ;要将描述符表地址及偏移量写入内存 gdt_ptr,一会儿用新地址重新加载
  sgdt [gdt_ptr] ;存储到原来gdt所有的位置


  ;将 gdt 描述符中视频段描述符中的段基址+0xc0000000
  mov ebx,[gdt_ptr+2]  ;GDT_BASE
  or dword [ebx+0x18+4],0xc0000000  ;+0x18 指到VIDEO_DESC +4
  ;视频段是第 3 个段描述符,每个描述符是 8 字节,故 0x18
  ;段描述符的高 4 字节的最高位是段基址的第 31~24 位

  ;将 gdt 的基址加上 0xc0000000 使其成为内核所在的高地址
  add dword [gdt_ptr+2],0xc0000000 ;;GDT_BASE+0xc0000000
  add esp,0xc0000000  ;将栈指针同样映射到内核地址

  ;把页目录地址赋给 cr3
  mov eax, PAGE_DIR_TABLE_POS
  mov cr3, eax

  ;打开cr0的pg位(第 31 位)
  mov eax, cr0
  or eax, 0x80000000
  mov cr0, eax

  ;在开启分页后,用 gdt 新的地址重新加载
  lgdt [gdt_ptr]
    jmp SELECTOR_CODE:enter_kernel	  ;强制刷新流水线,更新gdt
enter_kernel:
  call kernel_init
  mov esp, 0xc009f000
  jmp KERNEL_ENTRY_POINT 

;-----------------   将kernel.bin中的segment拷贝到编译的地址   -----------
kernel_init:
   xor eax, eax
   xor ebx, ebx		;ebx记录程序头表地址
   xor ecx, ecx		;cx记录程序头表中的program header数量
   xor edx, edx		;dx 记录program header尺寸,即e_phentsize

   mov dx, [KERNEL_BIN_BASE_ADDR + 42]	  ; 偏移文件42字节处的属性是e_phentsize,表示program header大小
   mov ebx, [KERNEL_BIN_BASE_ADDR + 28]   ; 偏移文件开始部分28字节的地方是e_phoff,表示第1 个program header在文件中的偏移量
					  ; 其实该值是0x34,不过还是谨慎一点，这里来读取实际值
   add ebx, KERNEL_BIN_BASE_ADDR
   mov cx, [KERNEL_BIN_BASE_ADDR + 44]    ; 偏移文件开始部分44字节的地方是e_phnum,表示有几个program header
.each_segment:
   cmp byte [ebx + 0], PT_NULL		  ; 若p_type等于 PT_NULL,说明此program header未使用。
   je .PTNULL

   ;为函数memcpy压入参数,参数是从右往左依然压入.函数原型类似于 memcpy(dst,src,size)
   push dword [ebx + 16]		  ; program header中偏移16字节的地方是p_filesz,压入函数memcpy的第三个参数:size
   mov eax, [ebx + 4]			  ; 距程序头偏移量为4字节的位置是p_offset
   add eax, KERNEL_BIN_BASE_ADDR	  ; 加上kernel.bin被加载到的物理地址,eax为该段的物理地址
   push eax				  ; 压入函数memcpy的第二个参数:源地址
   push dword [ebx + 8]			  ; 压入函数memcpy的第一个参数:目的地址,偏移程序头8字节的位置是p_vaddr，这就是目的地址
   call mem_cpy				  ; 调用mem_cpy完成段复制
   add esp,12				  ; 清理栈中压入的三个参数
.PTNULL:
   add ebx, edx				  ; edx为program header大小,即e_phentsize,在此ebx指向下一个program header 
   loop .each_segment
   ret   
  
;----------  逐字节拷贝 mem_cpy(dst,src,size) ------------
;输入:栈中三个参数(dst,src,size)
;输出:无
;---------------------------------------------------------
mem_cpy:		      
   cld
   push ebp
   mov ebp, esp
   push ecx		   ; rep指令用到了ecx，但ecx对于外层段的循环还有用，故先入栈备份
   mov edi, [ebp + 8]	   ; dst
   mov esi, [ebp + 12]	   ; src
   mov ecx, [ebp + 16]	   ; size
   rep movsb		   ; 逐字节拷贝

   ;恢复环境
   pop ecx		
   pop ebp
   ret



rd_disk_m_32:
  mov esi,eax ;备份eax，因为后面会用到ax寄存器
  mov di,cx   ;备份cx到di
  ;读写磁盘
  ;第一步：设置读取的扇区数
  mov dx,0x1f2 ;存放端口到0x1f2对应Sector count 寄存器用来指定待读取或待写入的扇区数
  mov al,cl    ;将读取的扇区数存放如al
  out dx,al    ;向端口写数据
  mov eax,esi  ;恢复ax

  ;第二步写入地址
  ;LBA 地址 7~0 位写入端口 0x1f3
  mov dx,0x1f3
  out dx,al

  ;LBA 地址 15~8 位写入端口 0x1f4
  mov cl,8
  shr eax,cl ;右移动8位将15~8位移到al
  mov dx,0x1f4
  out dx,al

  ;LBA 地址 23~16 位写入端口 0x1f5
  shr eax,cl ;再右移动8位
  mov dx,0x1f5
  out dx,al

  shr eax,cl    ;在右移动8位
  and al,0x0f   ;与1111(只保留低四位)
  or al,0xe0    ;或11100000 lba 第 24~27 位（在高4位或上1110,表示 lba(扇区从0开始编号) 模式,主盘）
  mov dx,0x1f6  
  out dx,al

  ;第三步写入命令
  mov dx ,0x1f7
  mov al ,0x20
  out dx ,al

  ;第四位：检测硬盘状态
.not_ready:
  nop ;空指令，用于debug
  in al,dx  ;端口也是0x1F7
  and al,0x88 ;与1000 1000 ，只保留第三位和第七位
  ;第三位为1表示银盘准备好传输数据,第七位表示硬盘忙
  cmp al ,0x08 ;进行比较判断硬盘是否准备好，如果相等表示(硬盘忙)没有准备好
  jnz .not_ready ;没有准备好，继续等

  ;第五步：从0x1f0读取数据
 
  ;di 为要读取的扇区数,一个扇区有 512 字节,每次读入一个字共需 di*512/2 次,所以 di*256
  ;下面四行计算读取次数，并存放在ax当中，
  mov ax,di   ;此时di寄存器存放的是的读取扇区个数
  mov dx,256  
  mul dx ;dx*ax存放在ax中
  mov cx,ax

  mov dx,0x1f0
.go_on_read:
  in ax,dx ;进行读取
  mov [ebx],ax
  add ebx,2
  loop .go_on_read
  ret


  ;-------------   创建页目录及页表   ---------------
setup_page:
;先把页目录占用的空间逐字节清0
  mov ecx,4096
  mov esi,0
.clear_page_dir:
  mov byte [PAGE_DIR_TABLE_POS+esi] ,0
  inc esi
  loop .clear_page_dir

;开始创建也目录项(PDE)
.creat_pde: 
  mov eax,PAGE_DIR_TABLE_POS
  add eax,0x1000  ;此时 eax 为第一个页表的位置及属性
  mov ebx,eax     ;此处为 ebx 赋值,是为.create_pte 做准备,ebx 为基址

  or eax, PG_US_U | PG_RW_W | PG_P
  mov [PAGE_DIR_TABLE_POS+0x0] ,eax ;第一个目录项指向第一个页表项(保证页表开启后的顺利切换)
  mov [PAGE_DIR_TABLE_POS+0xc00],eax;第768个目录项指向第一个页表项
  
  sub eax,0x1000
  mov [PAGE_DIR_TABLE_POS+4092],eax ;使最后一个目录项指向页目录表自己的地址

;下面创建页表项(PTE)
  mov ecx,256   ;1M低端内存 / 每页大小4k =256
  mov esi,0
  mov edx,PG_US_U | PG_RW_W | PG_P
.clear_page_pte:
  mov [ebx+esi*4],edx
  add edx,4096
  inc esi
  loop .clear_page_pte

;创建内核其他页表的 PDE
  mov eax, PAGE_DIR_TABLE_POS
  add eax,0x2000 ;此时为第二个页表的位置
  or eax,PG_US_U | PG_RW_W | PG_P
  mov ebx,PAGE_DIR_TABLE_POS
  mov ecx, 254  ; 范围为第 769~1022 的所有目录项数量
  mov esi, 769
.creat_kernel_pde:
  mov [ebx+esi*4],eax
  inc esi
  add eax,0x1000
  loop .creat_kernel_pde
  ret
