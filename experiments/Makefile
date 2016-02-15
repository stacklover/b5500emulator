#***********************************************************************
# b5500emulator
#***********************************************************************
# Copyright (c) 2016, Reinhard Meyer, DL5UY
# Licensed under the MIT License,
#       see LICENSE
# based on work by Nigel Williams and Paul Kimpel
# see: https://github.com/pkimpel/retro-b5500
#***********************************************************************
# single precision compare
#***********************************************************************
# 2016-02-13  R.Meyer
#   from thin air.
#**********************************************************************/

all:	b5500_test.exe

b5500_test.exe: b5500_test.c b5500_sp_compare.c b5500_sp_add.c b5500_common.h Makefile
	gcc -Wall -o b5500_test.exe b5500_test.c b5500_sp_compare.c b5500_sp_add.c 

