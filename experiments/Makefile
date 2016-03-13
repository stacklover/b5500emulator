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

OBJTEST =	b5500_test.o \
		b5500_sp_compare.o \
		b5500_sp_add.o \
		b5500_pdp_text.o b5500_init_shares.o \
		b5500_execute_cm.o b5500_execute_wm.o \
		b5500_stack.o b5500_memory.o b5500_jump_call.o \
		b5500_irq_io.o b5500_single.o b5500_double.o \
		b5500_misc_cpu.o b5500_bitfield.o b5500_stream.o \
		b5500_instr_table.o

OBJVIEW =	b5500_view.o b5500_pdp_text.o b5500_init_shares.o

OBJASM =	b5500_asm.o \
		b5500_pdp_text.o b5500_init_shares.o \
		b5500_execute_cm.o b5500_execute_wm.o \
		b5500_stack.o b5500_memory.o b5500_jump_call.o \
		b5500_irq_io.o b5500_single.o b5500_double.o \
		b5500_misc_cpu.o b5500_bitfield.o b5500_stream.o \
		b5500_instr_table.o

OBJSPTEST =	b5500_sptest.o \
		b5500_pdp_text.o b5500_init_shares.o \
		b5500_execute_cm.o b5500_execute_wm.o \
		b5500_stack.o b5500_memory.o b5500_jump_call.o \
		b5500_irq_io.o b5500_single.o b5500_double.o \
		b5500_misc_cpu.o b5500_bitfield.o b5500_stream.o \
		b5500_instr_table.o

INC =		b5500_common.h

all:	b5500_view.exe b5500_asm.exe b5500_sptest.exe

b5500_test.exe:  $(OBJTEST) Makefile
	gcc -Wall -o b5500_test.exe $(OBJTEST) 

b5500_view.exe:  $(OBJVIEW) Makefile
	gcc -Wall -o b5500_view.exe $(OBJVIEW) 

b5500_asm.exe:  $(OBJASM) Makefile
	gcc -Wall -o b5500_asm.exe $(OBJASM) 

b5500_sptest.exe:  $(OBJSPTEST) Makefile
	gcc -Wall -o b5500_sptest.exe $(OBJSPTEST) 

%.o: %.c $(INC) Makefile
	gcc -Wall -c -o $@ $<
