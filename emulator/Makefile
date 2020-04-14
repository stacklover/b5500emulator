#***********************************************************************
# b5500emulator
#***********************************************************************
# Copyright (c)	2016-2018, Reinhard Meyer, DL5UY
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
# 2020-03-09  R.Meyer
#   added iTELEX functionality
#**********************************************************************/

ALL =		$(ODIR)/emulator2.exe \
		$(ODIR)/processor_panel.exe \
		$(ODIR)/datacom_panel.exe \
		$(ODIR)/b9352.exe \
		$(ODIR)/b9353.exe

OBJPANEL =	$(ODIR)/processor_panel.o \
		$(ODIR)/pdp_text.o \
		$(ODIR)/init_shares.o

OBJDCC =	$(ODIR)/datacom_panel.o

OBJB9352 =	$(ODIR)/b9352.o

OBJB9353 =	$(ODIR)/b9353.o

OBJEMULATOR2 =	$(ODIR)/emulator2.o  \
		$(ODIR)/init_shares.o \
		$(ODIR)/b5500_cpu.o \
		$(ODIR)/cc2.o \
		$(ODIR)/io.o \
		$(ODIR)/dev_spo.o \
		$(ODIR)/dev_cr.o \
		$(ODIR)/dev_cp.o \
		$(ODIR)/dev_mt.o \
		$(ODIR)/dev_lp.o \
		$(ODIR)/dev_dr.o \
		$(ODIR)/dev_dk.o \
		$(ODIR)/dev_dcc.o \
		$(ODIR)/dcc_ld_teletype.o \
		$(ODIR)/dcc_ld_contention.o \
		$(ODIR)/dcc_em_b9352_ansi.o \
		$(ODIR)/dcc_pc_telnet.o \
		$(ODIR)/dcc_pc_itelex.o \
		$(ODIR)/dcc_pc_serial.o \
		$(ODIR)/dcc_pc_canopen.o \
		$(ODIR)/translatetables.o \
		$(ODIR)/instr_table.o \
		$(ODIR)/circbuffer.o \
		$(ODIR)/telnetd.o \
		$(ODIR)/itelexd.o
ifeq ($(USECAN),1)
OBJEMULATOR2 +=	$(ODIR)/canbus.o \
		$(ODIR)/canlib.o
endif

INC =		common.h io.h b5500_defs.h canlib.h dcc.h telnetd.h itelexd.h

CFLAGS		= -D_LARGEFILE64_SOURCE	-D_FILE_OFFSET_BITS=64 -pipe -Os \
		  -D_THREAD_SAFE -D_REENTRANT -DNOSIMH -Wall
ifeq ($(USECAN),1)
CFLAGS		+= -DUSECAN=1
endif

LFLAGS		= -lrt -lpthread #-lfltk_images -lfltk	-lpng -lz -ljpeg  -lXext -lXft \
		  #-lfontconfig	-lXinerama -ldl -lm -lX11

all: $(ALL)

$(ODIR)/processor_panel.exe:	 $(OBJPANEL) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o $(ODIR)/processor_panel.exe $(OBJPANEL)

$(ODIR)/datacom_panel.exe:	 $(OBJDCC) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o $(ODIR)/datacom_panel.exe $(OBJDCC)

$(ODIR)/b9352.exe:	 $(OBJB9352) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o $(ODIR)/b9352.exe $(OBJB9352)

$(ODIR)/b9353.exe:	 $(OBJB9353) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o $(ODIR)/b9353.exe $(OBJB9353)

$(ODIR)/emulator2.exe:	 $(OBJEMULATOR2) Makefile
	@echo "*** Linking $@..."
	$(CXX) $(LFLAGS) -o $(ODIR)/emulator2.exe $(OBJEMULATOR2)

$(ODIR)/%.o: %.c $(INC)	Makefile
	@echo "*** Compiling $<..."
	$(CXX) $(CFLAGS) -c $< -o $@

