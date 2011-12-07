cmd_drivers/input/misc/gpio_input.o := arm-none-linux-gnueabi-gcc -Wp,-MD,drivers/input/misc/.gpio_input.o.d  -nostdinc -isystem /usr/src/dropad/arm-2009q3/bin/../lib/gcc/arm-none-linux-gnueabi/4.4.1/include -I/usr/src/dell/abc/dsc-team-kernel-project/arch/arm/include -Iinclude  -include include/generated/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-msm/include -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -Werror -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -fno-delete-null-pointer-checks -Os -marm -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables -D__LINUX_ARM_ARCH__=7 -march=armv7-a -msoft-float -Uarm -Wframe-larger-than=3072 -fno-stack-protector -fomit-frame-pointer -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fconserve-stack   -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(gpio_input)"  -D"KBUILD_MODNAME=KBUILD_STR(gpio_input)" -D"DEBUG_HASH=60" -D"DEBUG_HASH2=33" -c -o drivers/input/misc/gpio_input.o drivers/input/misc/gpio_input.c

deps_drivers/input/misc/gpio_input.o := \
  drivers/input/misc/gpio_input.c \

drivers/input/misc/gpio_input.o: $(deps_drivers/input/misc/gpio_input.o)

$(deps_drivers/input/misc/gpio_input.o):
