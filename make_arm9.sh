# make all executeables for the ARM9 based embedded system
#!/bin/sh

# ***** where the binaries are to be found *****
# elftosb is in:
export PATH=$PATH:/opt/freescale/ltib/usr/bin/
# ggc and utilities are in:
# export PATH=$PATH:/opt/freescale/usr/local/gcc-4.4.4-glibc-2.11.1-multilib-1.0/arm-fsl-linux-gnueabi/bin/
export PATH=$PATH:/home/reinhard/buildroot/staging/host/usr/bin/

# ***** cross compiler to use *****
export CROSS_COMPILE=arm-linux-
export CC=arm-linux-gcc
export CXX=arm-linux-g++

cd emulator
make ODIR=arm9

