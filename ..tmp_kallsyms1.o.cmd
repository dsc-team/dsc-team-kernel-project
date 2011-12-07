cmd_.tmp_kallsyms1.o := arm-none-linux-gnueabi-gcc -Wp,-MD,./..tmp_kallsyms1.o.d -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables  -D__LINUX_ARM_ARCH__=7 -march=armv7-a  -include asm/unified.h -msoft-float   -nostdinc -isystem /usr/src/dropad/arm-2009q3/bin/../lib/gcc/arm-none-linux-gnueabi/4.4.1/include -I/usr/src/dell/abc/dsc-team-kernel-project/arch/arm/include -Iinclude  -include include/generated/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-msm/include    -c -o .tmp_kallsyms1.o .tmp_kallsyms1.S

deps_.tmp_kallsyms1.o := \
  .tmp_kallsyms1.S \
  /usr/src/dell/abc/dsc-team-kernel-project/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \
  /usr/src/dell/abc/dsc-team-kernel-project/arch/arm/include/asm/types.h \
  include/asm-generic/int-ll64.h \
  /usr/src/dell/abc/dsc-team-kernel-project/arch/arm/include/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
    $(wildcard include/config/64bit.h) \

.tmp_kallsyms1.o: $(deps_.tmp_kallsyms1.o)

$(deps_.tmp_kallsyms1.o):
