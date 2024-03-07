%include "boot.inc"
;主引导程序
SECTION MBR vstart=0x7c00  ;指定段地址(引导程序的起始地址)
   ;下面几行是使用cs寄存器初始化其他段寄存器，此时cs寄存器为0, 因为jmp 0: 0x7c00
   mov ax,cs      
   mov ds,ax
   mov es,ax
   mov ss,ax
   mov fs,ax                
   ;下面两行保存显卡地址(0xb800)到gs，ax起跳板作用，因为是段首地址所以要左移一位
   mov ax,0xb800
   mov gs,ax
   mov sp,0x7c00      ;初始化栈指针     

 ;功能号(上卷屏幕) AH = 0x06 ,上卷行数 AL=0(0表示全部)（ax=ah+al）
   mov ax,600h       
 ;BH上卷行属性
   mov bx,700h
 ;(CL,CH)左上角位置         
   mov cx,0
 ;(DL,DH)右上角位置发
   mov dx,184fh 
 ;触发中断，执行功能(无返回值)
   int 10h    

  ;直接利用显卡地址打印"  HELLO YR  "
  ; A 表示绿色背景闪烁,4 表示前景色为红色
  mov byte [gs:0x00],' '
  mov byte [gs:0x01],0xA4
  mov byte [gs:0x02],' '
  mov byte [gs:0x03],0xA4
  mov byte [gs:0x04],'1'
  mov byte [gs:0x05],0xA4
  mov byte [gs:0x06],' '
  mov byte [gs:0x07],0xA4
  mov byte [gs:0x08],'M'
  mov byte [gs:0x09],0xA4
  mov byte [gs:0x0A],'B'
  mov byte [gs:0x0B],0xA4
  mov byte [gs:0x0C],'R'
  mov byte [gs:0x0D],0xA4
  mov byte [gs:0x0E],' '
  mov byte [gs:0x0F],0xA4
  mov byte [gs:0x10],' '
  mov byte [gs:0x11],0xA4


  mov eax, LOADER_START_SECTOR  ;起始扇区LBA地址
  mov bx , LOADER_BASE_ADDR     ;写入的内存地址
  mov cx , 5                    ;待读入的扇区数
  call  rd_disk_m_16            ;调用函数，读取

  jmp LOADER_BASE_ADDR +0x300


  ; 函数的读取磁盘n个扇区（参数如下）
  ; eax=LBA 扇区号
  ; bx=将数据写入的内存地址
  ; cx=读入的扇区数
rd_disk_m_16:
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
  mov [bx],ax
  add bx,2
  loop .go_on_read
  ret
 ;($-$$)指的是段首到该行位置的偏移量 ，这一行代码的目的是将510字节中剩余的用0填满
  times 510-($-$$) db 0
 ;使用魔术标记该扇区为引导扇区
   db 0x55,0xaa