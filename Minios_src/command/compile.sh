####  此脚本应该在command目录下执行

if [[ ! -d "../lib" ]]; then
    echo "dependent dir don\`t exist!"
    cwd=$(pwd)
    cwd=${cwd##*/}
    cwd=${cwd%/}
    if [[ $cwd != "command" ]]; then
        echo -e "you\`d better in command dir\n"
    fi
    exit
fi

BIN="cat"
CFLAGS="-Wall -c -m32 -fno-builtin -W -Wstrict-prototypes \
      -Wmissing-prototypes -Wsystem-headers -fno-stack-protector -mno-sse -g "
LIBS="-I ../lib/ -I ../lib/kernel/ -I ../lib/user/ -I \
 ../kernel/ -I ../device/ -I ../thread/ -I \
 ../userprog/ -I ../fs/ -I ../shell/"
 
OBJS="../lib/string.o ../lib/user/syscall.o \
      ../lib/stdio.o ../lib/user/assert.o"
DD_IN=$BIN
DD_OUT="/home/gty/vscode/os/boot/boot.img"

gcc $CFLAGS $LIBS -o $BIN".o" $BIN".c"
nasm -f elf ./start.asm -o ./start.o
ld -m elf_i386 -T program.ld  $BIN".o" $OBJS start.o -o $BIN
SEC_CNT=$(ls -l $BIN | awk '{printf("%d", ($5+511)/512)}')

if [[ -f $BIN ]]; then
    dd if=./$DD_IN of=$DD_OUT bs=512 \
        count=$SEC_CNT seek=300 conv=notrunc
fi
