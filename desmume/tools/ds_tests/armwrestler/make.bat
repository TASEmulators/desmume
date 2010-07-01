arm-eabi-as -march=armv5te -mthumb -EL -o armwrestler9.o armwrestler9.asm
arm-eabi-ld -Ttext 0x02004000  -EL -e main armwrestler9.o 
arm-eabi-objcopy -O binary a.out armwrestler.arm9
arm-eabi-as -mcpu=arm7tdmi -EL -o armwrestler-arm7.o armwrestler-arm7.asm
arm-eabi-ld -Ttext 0x3800000  -EL -e arm7_main armwrestler-arm7.o 
arm-eabi-objcopy -O binary a.out armwrestler.arm7
ndstool -c armwrestler.nds -r9 0x2004000 -e9 0x2004000 -r7 0x3800000 -e7 0x3800000 -9 armwrestler.arm9 -7 armwrestler.arm7
del *.o
del a.out
pause