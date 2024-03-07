%include "boot.inc"
;主引导程序 

;------------------------------------------------------------
SECTION MBR vstart=0x7c00         
   mov ax,cs         ;BIOS 是通过 jmp 0：0x7c00 跳转到 MBR，此时cs为0
   mov ds,ax
   mov es,ax
   mov ss,ax
   mov fs,ax
   mov sp,0x7c00       ;0x7c00 以下暂时是安全的区域，就把它当作栈来用。源于dos1.0的32kb内存
   mov ax,0xb800
   mov gs,ax

;清屏
   mov ax,0x0600
   mov bx,0x0700  ;上卷行的属性
   mov cx,0         
   mov dx,0x184f   ;vga文本模式一行80字符，共25行
   int 10h
;显示内容
   mov byte [gs:0x00],'1'
   mov byte [gs:0x01],0xA4

   mov byte [gs:0x02],' '
   mov byte [gs:0x03],0xA4

   mov byte [gs:0x04],'M'
   mov byte [gs:0x05],0xA4

   mov byte [gs:0x06],'B'
   mov byte [gs:0x07],0xA4

   mov byte [gs:0x08],'R'
   mov byte [gs:0x09],0xA4 


   

;将一个扇区加载到内存指定位置并跳转
   mov eax,LOADER_START_SECTOR
   mov ebx,LOADER_BASE_ADDR
   mov ecx,4                           ;loader.S已经超过了一个扇区
   call rd_disk_m_16

   jmp LOADER_BASE_ADDR



;512字节不够用，需要将loader读到内存中，由loader来加载内核

;读磁盘的n个扇区函数    
rd_disk_m_16:
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
   mov [bx],ax
   add bx,2
   loop continue_read

   ret

   

  times 510-($-$$) db 0
   db 0x55,0xaa
