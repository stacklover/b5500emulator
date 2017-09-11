# Loader
# 20: 14M90+KI = 0104
# 21: $|#0|00M
# 22: M)290+JI
# 23: "000000D
# 24: 4*342L05
# 25: 000V10JI = 0000 00
# 26: 18JI0)0)
# 27: 000S0QKI = 0000 00
# 30: 000W0HKI = 0000 00
# 31: 000,08KI = 0000 00
# 32: 000]=/1V = 0000 00
# 33: 082I08JI = 0010 02
# 34: 0417+E0M
# 35: 16+E[)|/
# 36: EV+EBV+E
# 37: }V+E?V*)
# 40: */}V000M
# 41: 16+E:)B/
# 42: 1VB),/4V
# 43: 4J1>50JI

	.ORG	020
# Entry	point for the hardware load -- Literal Call: push a literal @21
# into the stack (A register) as the absolute address of the card read
# descriptor
	LITC	021
# Initiate the I/O by storing the A register at	address	@10, signalling
# Central Control, and setting A to empty. The I/O Unit	picks up the
# descriptor from that stored address and initiates the	I/O asynchronously
	IIO
# Branch forward 4 syllables to	22:0
	LITC	4
	BFW
# Card read I/O	descriptor: card reader	#1 (CRA), 10 words (80 characters),
# alpha	mode, address @44. Note	that while the hardware	load reads one binary
# card,	the ESPOL Loader reads cards in	alpha mode, i.e., the way they would
# normally be encoded by a keypunch machine.
	.WORD	05240120040000044
# Dial A to bit	28 (numbered 20	in the Handbook	word diagrams)
	DIA	044
# Interrogate interrupts and branch to the vector address for the
# highest-priority one outstanding, if any. If no interrupts are outstanding,
# fall through to the next syllable.
	ITI
# Branch back to 22:0 (loop until interrupt occurs)
	LITC	4
	BBW
# Character mode program descriptor for	the subroutine (starting in the	next
# syllable) that will be called	from 43:1, present in memory, character	mode,
# F=0, C=@24
	.WORD	0
# Entry	point of character mode	subroutine: recall source address at F-4
# (four	words below the	Return Control Word, or	RCW, and in this case,
# three	words below the	Mark Stack Control Word, or MSCW) into the M and
# G registers
	RSA	4
# Recall destination address at	F-3 (two words below the MSCW) into S and K
	RDA	3
# Call the repeat field	(transfer count) at F-2	(one word below	the MSCW)
	CRF	2
# Transfer the designated number of words from source to destination
# addresses
	TRW	0
# Exit character mode, in this case returning to 43:2
	EXC
# Transfer zero	bits from A to B (effectively a	no-op, except that it sets A
# empty) [I don't think	this and the next two instructions are normally
# executed, but	@24 is the interrupt vector location for Keyboard Request
# from the SPO,	and @23	is the location	for I/O	Busy, so the branch at 25:3
# is probably just a trap for spurious interrupts. Undefined opcodes are
# no-ops on the	B5500, so it's likely that a vector to one of the words
# above	would eventually fall through to the next syllable at 25:2]
	TRB	0
# Branch back to 22:0 to continue looping for interrupts
	LITC	020
	BBW
# Push branch offset [Don't know of an interrupt that vectors here, but	this
# is also probably a trap for an unexpected vector]
	LITC	022
# Branch to 22:0 to continue looping for interrupts
	BBW
# (traditionally used as a no-op)
	DIA	0
	DIA	0
# Interrupt vector location for	I/O Unit #1 finished: push a zero
	LITC	0
# Operand call:	Push a copy of the result descriptor (RD) for I/O Unit #1
# at R+@14 = @14 absolute. The R register is still zero	after being
# initialized by the Load button
	OPDC	014
# Branch to 32:2 to handle interrupt
	LITC	012
	BFW
# Interrupt vector location for	I/O Unit #2 finished: push a zero
	LITC	0
# Operand call:	Push a copy of the result descriptor (RD) for I/O Unit #2
# at R+@15 = @15 absolute
	OPDC	015
# Branch to 32:2 to handle interrupt
	LITC	6
	BFW
# Interrupt vector location for	I/O Unit #3 finished: push a zero
	LITC	0
# Operand call:	Push a copy of the result descriptor (RD) for I/O Unit #3
# at R+@16 = @16 absolute
	OPDC	016
# Branch to 32:2 to handle interrupt
	LITC	2
	BFW
# Interrupt vector location for	I/O Unit #4 finished: push a zero
	LITC	0
# Operand call:	Push a copy of the result descriptor (RD) for I/O Unit #4
# at R+@17 = @17 absolute into the stack
	OPDC	017
# Common point for handling an I/O finish interrupt -- Dial B to bit 47
# (the low-order bit)
	DIB	075
# Transfer bit 28 from the result descriptor in	A (see the DIA at 22:0)
# to bit 47 in B (which	currently holds	the zero pushed	at each	of the
# interrupt vector entry points) and mark A empty to delete the	RD from
# the stack.
	TRB	1
# Branch Forward Conditional around the	endless	loop below to 34:0 if the
# low-order bit	in B is	0 (false), i.e., if bit	28 (20 in the Handbook)	in
# the RD was false, indicating no read-check error occurred on the card	read.
# Since	we are reading in alpha	mode, false means there	were no	invalid
# punches on the card that couldn't be translated to internal character	codes
	LITC	2
	BFC
# Branch back to 33:2 -- an endless loop. The reason for the loop is that
# there	was no way to halt the processor in control state. We come to 33:2
# if there was a read-check error, so in that case the loader just gives up
# and spins here until someone manually	halts the system.
	LITC	2
	BBW
# To here if a successful card read -- push a 1	as an index for	the DESC
# next.	This is	the beginning of code that builds parameters in	the stack
# for the character-mode procedure that	will be	called at 43:1.
	LITC	1
# Descriptor call: push	a copy of the word at R+@21 (@21 absolute) into	the
# stack. Since that word looks like a present data descriptor with a non-zero
# length field,	things get really interesting. The descriptor must be indexed
# by the value just below it in	the stack (the 1 just pushed there by the
# LITC). The DESC syllable does	that automatically First, the index is
# checked against the length field, but	0 <= 1 < length, so we're okay;
# otherwise we would get an Invalid Index interrupt set	(if we were in
# Normal State,	that is). Next,	the base address in [33:15] of the
# descriptor in	B is indexed by	adding (modulo 15 bits)	the 1 in A to it.
# The length field [8:10] in the word is set to	zero, indicating this copy
# descriptor points to a specific word address instead of an array of words.
# This is effectively an absolute address, and it's left in the	stack as the
# operation's result. Note that	it points to the second	word in	the 10-word
# card read buffer, so I'll refer to it	as BUF[1].
# Note:	This word in the stack becomes the parameter for the source address
# at F-4 used by the character-mode procedure called at	43:1.
	DESC	021
# Duplicate the	indexed	descriptor in the stack.
	DUP
# Push @11 (9) into the	stack as an index for the OPDC,	next
	LITC	011
# Operand call:	push another copy of the word at R+@21 into the	stack. Since
# it still looks like a	present	data descriptor	with a non-zero	length field,
# OPDC indexes it by the 9 just	below it. Instead of leaving the address in
# the stack, though, OPDC fetches the word at the indexed address
# (@44+@11 = @55, the last word	in the 10-word buffer) and replaces the
# descriptor with a copy of that word's	contents (I'll refer to	this value
# as BUF[9]). Thus OPDC	works somewhat like "load" on other machines and
# DESC works somewhat like "load address".
	OPDC	021
# Duplicate the	value of BUF[9]
	DUP
# Dial A to bit	21
	DIA	033
# Dial B to bit	24
	DIB	040
# Transfer 21 bits from	A (the dup of BUF[9]) to B (the	original copy of
# BUF[9]) and delete the dup.
	TRB	025
# Duplicate the	word just updated
	DUP
# Transfer 18 bits from	the dup	word to	the updated original word and
# delete the dup. What these next DUP/TRB sequences do is convert 6-bit
# character codes to 3-bit octal digits	to form	an absolute memory address
# in the low-order end of the word. See	below for details.
	TRB	022
# Duplicate the	newly updated word again
	DUP
# Transfer 15 bits from	the dup	word to	the updated original word and
# delete the dup.
	TRB	017
# Duplicate the	newly updated word again
	DUP
# Transfer 12 bits from	the dup	word to	the updated original word and
# delete the dup. This is the last of the octal-to-binary conversion
	TRB	014
# Dial A to bit	33 (start of the address field in the updated copy of BUF[9])
	DIA	053
# Dial B to bit	33 (start of the address field in the dup of the indexed
# descriptor pointing to BUF[1])
	DIB	053
# Transfer the 15-bit address field from the updated copy of BUF[9] to the
# dup of the indexed descriptor	referencing BUF[1] and delete the updated
# copy of BUF[9]. The resulting	descriptor in the top of stack is now
# pointing to a	memory address that was	constructed from fields	in BUF[9].
# Note:	This word in the stack becomes the parameter for the destination
# address at F-3 used by the character-mode procedure.
	TRB	017
# Push a zero into the stack to	begin constructing the third parameter,
# the transfer count.
	LITC	0
# Push another 9 into the stack	to be used as an index
	LITC	011
# Index	the I/O	descriptor again and load a copy of BUF[9] into	the stack
	OPDC	021
# Duplicate the	copy of	BUF[9]
	DUP
# Dial A to bit	11
	DIA	015
# Dial B to bit	14
	DIB	022
# Transfer one bit, i.e., B:=B&A[14:11:1] (or equivalently, B.[14:1]:=A.[11:1]),
# and delete the dup copy of BUF[9]. This is another character-to-octal
# conversion. There is a two-character count of	words on the card in the
# second and third characters of BUF[9]. The value can't be more than eight,
# since	that's the maximum number of words that	fit on a card (see the
# discussion on	ESPOL below), so only the low-order bit	of the high-order
# character can	be significant (8=@10).
	TRB	1
# Dial A to bit	14
	DIA	022
# Dial B to bit	44
	DIB	072
# Transfer 4 bits from bit 14 in A (from the last TRB result) to the
# low-order 4 bits of B	(the zero pushed at 40:2).
# Note:	This word in the stack becomes the the parameter for the transfer
# count	at F-2 used by the character-mode procedure.
	TRB	4
# Construct and	push a Mark Stack Control Word (MSCW) in preparation for
# calling a procedure. On entry	to the procedure, this word will be at F-1.
	MKS
# Construct and	push a Return Control Word (RCW) with a	return address of
# 43:2 and call	the Character Mode procedure using the program descriptor at
# R+@23	(@23 absolute).	On entry to the	procedure, F points to the RCW.
# Note that the	behavior of OPDC is based primarily on the type	of word
# it finds at the relative address, not	anything in the	opcode it self --
# a very B5500 thing to	do. Also notice	that there is no code to save or
# restore register state on entering and exiting the subroutine	-- all of
# that is completely automatic.
#
# That procedure (entry	point at 24:0) transfers a number of words from	a
# source address to a destination address. The count of	words is in the	stack
# just below the MSCW, the destination address below that, and the source
# address below	that. These will be addressed using negative offsets from F,
# which	points to the RCW pushed by the	OPDC procedure-call syllable.
# Typically parameters are pushed into the stack between the MSCW and RCW,
# but character-mode routines are unusual in that they sometimes address the
# stack	directly below their stack frame.
	OPDC	023
# Branch to 20:0 to read another card
	LITC	0120
	BBW
#
	ZPI
	.ORG	020
	.RUN
	.END
