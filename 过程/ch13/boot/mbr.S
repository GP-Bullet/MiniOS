;主引导程序
;-----------------------------------------------------
%include "boot.inc"
SECTION MBR vstart=0x7c00			;告诉编译器，起始地址是0x7c00
	mov ax,cs				;因为BIOS执行完毕后cs:ip为0x0:0x7c00,所以用cs初始化各寄存器(此时cs=0)
	mov ds,ax				;ds、es、ss、fs不能给立即数初始化，需要用ax寄存器初始化
	mov es,ax
	mov ss,ax
	mov fs,ax
	mov sp,0x7c00				;初始化堆栈指针，因为目前0x7c00以下的内存暂时可用
	mov ax,0xb800				;选择显卡的文本模式
	mov gs,ax				;使用GS段寄存器作为显存段基址

;清屏利用0x06号功能，上卷全部行，则可清屏
;-----------------------------------------------------
;INT 0x10	功能号:0x06		功能描述:上卷窗口
;-----------------------------------------------------
;输入:
;AH	功能号= 0x06
;AL = 上卷行数（如果为0，表示全部）
;BH = 上卷行属性
;(CL,CH) = 窗口左上角的（X,Y）位置
;(DL,DH) = 窗口右下角的（X,Y）位置
;无返回值:

	mov ax,0x600			;上卷行数:全部	功能号:06
	mov bx,0x700			;上卷属性
	mov cx,0			;左上角:(0，0)
	mov dx,0x184f			;右下角:(80,25)
					;VGA文本模式中，一行只能容纳80字节，共25行
					;下标从0开始，所以0x18=24，0x4f=79
	int 0x10			;int 0x10

;输出背景色为蓝色，前景色为红色，并且跳动的字符串“1 MBR”
;--------------------------------------------------------------------------
	mov byte [gs:0x00], '1'
	mov byte [gs:0x01], 0x92

	mov byte [gs:0x02], ' '
	mov byte [gs:0x03], 0x12

	mov byte [gs:0x04], 'M'
	mov byte [gs:0x05], 0x12

	mov byte [gs:0x06], 'B'
	mov byte [gs:0x07], 0x12

	mov byte [gs:0x08], 'R'
	mov byte [gs:0x09], 0x12
;--------------------------------------------------------------------------


	mov eax ,LOADER_START_SECTOR	;起始扇区lba地址
	mov bx  ,LOADER_BASE_ADDR	;写入的地址
	mov cx  ,4			;待读入的扇区数
	call rd_disk_m_16		;以下读取程序的起始部分

	jmp LOADER_BASE_ADDR + 0x300

;--------------------------------------------------------------------------
;功能：读取eax=LBA扇区号
rd_disk_m_16:
	mov esi ,eax	;备份eax
	mov di ,cx	;备份cx

;读写硬盘
;1---设置要读取的扇区数
	mov dx ,0x1f2	;设置端口号，dx用来存储端口号的
	mov al ,cl	
	out dx ,al	;读取的扇区数
	
	mov eax ,esi	;恢复eax
 	

;2---将LBA地址存入0x1f3～0x1f6
	;LBA 7～0位写入端口0x1f3
	mov dx ,0x1f3
	out dx ,al

	;LBA 15~8位写入端口0x1f4
	mov cl ,8
	shr eax ,cl	;逻辑右移8位,将eax的最低8位移掉，让最低8位al的值变成接下来8位
	mov dx ,0x1f4
	out dx ,al

	;LBA 24～16位写入端口0x1f5
	shr eax ,cl
	mov dx ,0x1f5
	out dx ,al

	shr eax ,cl
	and al ,0x0f	;设置lba 24～27位
	or al ,0xe0	;设置7～4位是1110表示LBA模式
	mov dx ,0x1f6
	out dx ,al

;3---向0x1f7端口写入读命令0x20
	mov dx ,0x1f7
	mov al ,0x20
	out dx ,al

;4---检测硬盘状态
.not_ready:
	;同写入命令端口，读取时标示硬盘状态，写入时是命令
	nop
	in al ,dx
	and al ,0x88	;第三位为1表示已经准备好了，第7位为1表示硬盘忙
	cmp al ,0x08
	jnz .not_ready

;5---0x1f0端口读取数据
	mov ax ,di	;要读取的扇区数
	mov dx ,256	;一个扇区512字节，一次读取2字节，需要读取256次
	mul dx		;结果放在ax里
	mov cx ,ax	;要读取的次数
	
	mov dx ,0x1f0
.go_on_read:
	in ax, dx
	mov [bx], ax	;bx是要读取到的内存地址
	add bx, 0x02
	loop .go_on_read	;循环cx次
	ret	
	


	
	times 510-($-$$) db 0
	db 0x55,0xaa
