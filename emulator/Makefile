#***********************************************************************
# b5500emulator
#***********************************************************************
# Copyright (c)	2016, Reinhard Meyer, DL5UY
# Licensed under the MIT License,
#	see LICENSE
# based	on work	by Nigel Williams and Paul Kimpel
# see: https://github.com/pkimpel/retro-b5500
#***********************************************************************
# single precision compare
#***********************************************************************
# 2016-02-13  R.Meyer
#   from thin air.
# 2017-07-17  R.Meyer
#   changes for	cross compilation
#**********************************************************************/

ALL =		b5500_view.exe \
		b5500_asm.exe \
		b5500_sptest.exe \
		b5500_cardreader.exe

OBJTEST	=	b5500_test.o \
		b5500_sp_compare.o \
		b5500_sp_add.o \
		b5500_pdp_text.o b5500_init_shares.o \
		b5500_execute_cm.o b5500_execute_wm.o \
		b5500_stack.o b5500_memory.o b5500_jump_call.o \
		b5500_irq_io.o b5500_single.o b5500_double.o \
		b5500_misc_cpu.o b5500_bitfield.o b5500_stream.o \
		b5500_instr_table.o

OBJVIEW	=	b5500_view.o b5500_pdp_text.o b5500_init_shares.o

OBJCARDREADER =	b5500_cardreader.o b5500_init_shares.o \
		b5500_execute_cm.o b5500_execute_wm.o \
		b5500_stack.o b5500_memory.o b5500_jump_call.o \
		b5500_irq_io.o b5500_single.o b5500_double.o \
		b5500_misc_cpu.o b5500_bitfield.o b5500_stream.o \
		b5500_translatetables.o	\
		b5500_instr_table.o

OBJASM =	b5500_asm.o \
		b5500_pdp_text.o b5500_init_shares.o \
		b5500_execute_cm.o b5500_execute_wm.o \
		b5500_stack.o b5500_memory.o b5500_jump_call.o \
		b5500_irq_io.o b5500_single.o b5500_double.o \
		b5500_misc_cpu.o b5500_bitfield.o b5500_stream.o \
		b5500_translatetables.o	\
		b5500_instr_table.o

OBJSPTEST =	b5500_sptest.o \
		b5500_pdp_text.o b5500_init_shares.o \
		b5500_execute_cm.o b5500_execute_wm.o \
		b5500_stack.o b5500_memory.o b5500_jump_call.o \
		b5500_irq_io.o b5500_single.o b5500_double.o \
		b5500_misc_cpu.o b5500_bitfield.o b5500_stream.o \
		b5500_translatetables.o	\
		b5500_instr_table.o

INC =		b5500_common.h

CFLAGS		= -D_LARGEFILE64_SOURCE	-D_FILE_OFFSET_BITS=64 -pipe -Os \
		  -D_THREAD_SAFE -D_REENTRANT -Wall

LFLAGS		= #-lfltk_images -lfltk	-lpng -lz -ljpeg  -lXext -lXft \
		  #-lfontconfig	-lXinerama -lpthread -ldl -lm -lX11

all: $(ALL)

b5500_test.exe:	 $(OBJTEST) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o b5500_test.exe $(OBJTEST)

b5500_view.exe:	 $(OBJVIEW) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o b5500_view.exe $(OBJVIEW)

b5500_cardreader.exe:	 $(OBJCARDREADER) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o b5500_cardreader.exe $(OBJCARDREADER)

b5500_asm.exe:	$(OBJASM) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o b5500_asm.exe $(OBJASM)

b5500_sptest.exe:  $(OBJSPTEST)	Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o b5500_sptest.exe $(OBJSPTEST)

%.o: %.c $(INC)	Makefile
	@echo "*** Compiling $<..."
	$(CXX) $(CFLAGS) -c $< -o $@

