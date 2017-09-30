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
# 2017-09-30  R.Meyer
#   overhaul of file names
#**********************************************************************/

ALL =		processor_panel.exe \
		asm-n-run.exe \
		sptest.exe \
		emulator.exe

OBJTEST	=	test.o \
		sp_compare.o \
		sp_add.o \
		pdp_text.o init_shares.o \
		execute_cm.o execute_wm.o \
		stack.o memory.o jump_call.o \
		irq_io.o single.o double.o \
		misc_cpu.o bitfield.o stream.o \
		instr_table.o

OBJPANEL =	processor_panel.o pdp_text.o init_shares.o

OBJEMULATOR =	emulator.o init_shares.o \
		execute_cm.o execute_wm.o \
		stack.o memory.o jump_call.o \
		irq_io.o single.o double.o \
		misc_cpu.o bitfield.o stream.o \
		translatetables.o	\
		instr_table.o

OBJASMNRUN =	asm-n-run.o \
		pdp_text.o init_shares.o \
		execute_cm.o execute_wm.o \
		stack.o memory.o jump_call.o \
		irq_io.o single.o double.o \
		misc_cpu.o bitfield.o stream.o \
		translatetables.o	\
		instr_table.o

OBJSPTEST =	sptest.o \
		pdp_text.o init_shares.o \
		execute_cm.o execute_wm.o \
		stack.o memory.o jump_call.o \
		irq_io.o single.o double.o \
		misc_cpu.o bitfield.o stream.o \
		translatetables.o	\
		instr_table.o

INC =		common.h

CFLAGS		= -D_LARGEFILE64_SOURCE	-D_FILE_OFFSET_BITS=64 -pipe -Os \
		  -D_THREAD_SAFE -D_REENTRANT -Wall

LFLAGS		= #-lfltk_images -lfltk	-lpng -lz -ljpeg  -lXext -lXft \
		  #-lfontconfig	-lXinerama -lpthread -ldl -lm -lX11

all: $(ALL)

test.exe:	 $(OBJTEST) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o test.exe $(OBJTEST)

processor_panel.exe:	 $(OBJPANEL) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o processor_panel.exe $(OBJPANEL)

emulator.exe:	 $(OBJEMULATOR) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o emulator.exe $(OBJEMULATOR)

asm-n-run.exe:	$(OBJASMNRUN) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o asm-n-run.exe $(OBJASMNRUN)

sptest.exe:  $(OBJSPTEST)	Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o sptest.exe $(OBJSPTEST)

%.o: %.c $(INC)	Makefile
	@echo "*** Compiling $<..."
	$(CXX) $(CFLAGS) -c $< -o $@

