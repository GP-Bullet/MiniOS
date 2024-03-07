if [[ ! -d "../lib" || ! -d "../build" ]];then
    echo "dependent dir don't exist!"
    cwd=${pwd}
    cwd=${cwd##*/}
    cwd=${cwd%/}
    if [[ $cwd != "command" ]];then
        echo -e "you'd better in command dir\n"
    fi
    exit
fi

BIN="prog_no_arg"
CFLAGS="-Wall -m32 -c -fno-builtin -fno-stack-protector -W -Wmissing-prototypes -Wno-unused-parameter"
LIB="../lib/"
OBJS="../build/string.o ../build/syscall.o ../build/stdio.o ../build/assert.o"
DD_IN=$BIN
DD_OUT="../hd60M.img"

gcc $CFLAGS -I $LIB -o $BIN".o" $BIN".c"
#ld -melf_i386 -e main $BIN".o" $OBJS -o $BIN
#strip --remove-section=.note.gnu.property $BIN
SEC_CNT=$(ls -l $BIN | awk '{printf("%d", ($5+511)/512)}')

if [[ -f $BIN ]];then
    dd if=./$DD_IN of=$DD_OUT bs=512 count=$SEC_CNT seek=300 conv=notrunc
fi
