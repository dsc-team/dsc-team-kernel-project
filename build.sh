#!/bin/sh



export PATH=/usr/src/dropad/4.4.1/bin/:$PATH
#export PATH=/usr/src/dell/android-ndk-r7/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86/bin/:$PATH

make clean

ST1=`date`;
echo $ST1;

#ARCH=arm CROSS_COMPILE=arm-linux-androideabi- make -j 2
ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabi- make -j 2

ET1=`date`;
echo build complete!!!
echo $ST1
echo $ET1;


