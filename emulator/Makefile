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

ALL =		$(ODIR)/emulator2.exe \
		$(ODIR)/processor_panel.exe \
		#$(ODIR)/emulator.exe \
		#$(ODIR)/asm-n-run.exe \
		#$(ODIR)/sptest.exe \

OBJTEST	=	$(ODIR)/test.o \
		$(ODIR)/sp_compare.o \
		$(ODIR)/sp_add.o \
		$(ODIR)/pdp_text.o $(ODIR)/init_shares.o \
		$(ODIR)/execute_cm.o $(ODIR)/execute_wm.o \
		$(ODIR)/stack.o $(ODIR)/memory.o $(ODIR)/jump_call.o \
		$(ODIR)/single.o $(ODIR)/double.o \
		$(ODIR)/misc_cpu.o $(ODIR)/bitfield.o $(ODIR)/stream.o \
		$(ODIR)/instr_table.o

OBJPANEL =	$(ODIR)/processor_panel.o $(ODIR)/pdp_text.o $(ODIR)/init_shares.o

OBJEMULATOR =	$(ODIR)/emulator2.o $(ODIR)/init_shares.o \
		$(ODIR)/execute_cm.o $(ODIR)/execute_wm.o \
		$(ODIR)/stack.o $(ODIR)/memory.o $(ODIR)/jump_call.o \
		$(ODIR)/single.o $(ODIR)/double.o \
		$(ODIR)/misc_cpu.o $(ODIR)/bitfield.o $(ODIR)/stream.o \
		$(ODIR)/cc2.o \
		$(ODIR)/dev_spo.o \
		$(ODIR)/dev_cr.o \
		$(ODIR)/dev_mt.o \
		$(ODIR)/dev_lp.o \
		$(ODIR)/dev_dk.o \
		$(ODIR)/translatetables.o \
		$(ODIR)/instr_table.o

OBJEMULATOR2 =	$(ODIR)/emulator2.o $(ODIR)/init_shares.o \
		$(ODIR)/b5500_cpu.o \
		$(ODIR)/cc2.o \
		$(ODIR)/dev_spo.o \
		$(ODIR)/dev_cr.o \
		$(ODIR)/dev_mt.o \
		$(ODIR)/dev_lp.o \
		$(ODIR)/dev_dk.o \
		$(ODIR)/translatetables.o \
		$(ODIR)/instr_table.o
ifeq ($(USECAN),1)
OBJEMULATOR2 +=	$(ODIR)/canlib.o
endif

OBJASMNRUN =	$(ODIR)/asm-n-run.o \
		$(ODIR)/pdp_text.o $(ODIR)/init_shares.o \
		$(ODIR)/execute_cm.o $(ODIR)/execute_wm.o \
		$(ODIR)/stack.o $(ODIR)/memory.o $(ODIR)/jump_call.o \
		$(ODIR)/single.o $(ODIR)/double.o \
		$(ODIR)/misc_cpu.o $(ODIR)/bitfield.o $(ODIR)/stream.o \
		$(ODIR)/translatetables.o \
		$(ODIR)/instr_table.o

OBJSPTEST =	$(ODIR)/sptest.o \
		$(ODIR)/pdp_text.o $(ODIR)/init_shares.o \
		$(ODIR)/execute_cm.o $(ODIR)/execute_wm.o \
		$(ODIR)/stack.o $(ODIR)/memory.o $(ODIR)/jump_call.o \
		$(ODIR)/single.o $(ODIR)/double.o \
		$(ODIR)/misc_cpu.o $(ODIR)/bitfield.o $(ODIR)/stream.o \
		$(ODIR)/translatetables.o \
		$(ODIR)/instr_table.o

INC =		common.h b5500_defs.h canlib.h

CFLAGS		= -D_LARGEFILE64_SOURCE	-D_FILE_OFFSET_BITS=64 -pipe -Os \
		  -D_THREAD_SAFE -D_REENTRANT -DNOSIMH -Wall
ifeq ($(USECAN),1)
CFLAGS		+= -DUSECAN=1
endif

LFLAGS		= -lrt -lpthread #-lfltk_images -lfltk	-lpng -lz -ljpeg  -lXext -lXft \
		  #-lfontconfig	-lXinerama -ldl -lm -lX11

all: $(ALL)

$(ODIR)/test.exe:	 $(OBJTEST) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o $(ODIR)/test.exe $(OBJTEST)

$(ODIR)/processor_panel.exe:	 $(OBJPANEL) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o $(ODIR)/processor_panel.exe $(OBJPANEL)

$(ODIR)/emulator.exe:	 $(OBJEMULATOR) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o $(ODIR)/emulator.exe $(OBJEMULATOR)

$(ODIR)/emulator2.exe:	 $(OBJEMULATOR2) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o $(ODIR)/emulator2.exe $(OBJEMULATOR2)

$(ODIR)/asm-n-run.exe:	$(OBJASMNRUN) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o $(ODIR)/asm-n-run.exe $(OBJASMNRUN)

$(ODIR)/sptest.exe:  $(OBJSPTEST)	Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o $(ODIR)/sptest.exe $(OBJSPTEST)

$(ODIR)/%.o: %.c $(INC)	Makefile
	@echo "*** Compiling $<..."
	$(CXX) $(CFLAGS) -c $< -o $@

