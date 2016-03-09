# build PRT @1000
	.ORG	01000
# nothing here
	.ORG	01020
	.WORD	0
	.WORD	0	# temp storage
# PRT(22) = *LIST, LABEL, OR SEGMENT DESCRIPTOR*
# PRT(22) = SEGMENT DESCRIPTOR, TYPE = 3, RELATIVE ADDRESS = 0000, SEGMENT NUMBER = 0001.
	.WORD	07500000000001100	# main program
# PRT(23) = *OUTER BLOCK DESCRIPTOR*
	.WORD	0
# PRT(24) = *SEGMENT DESCRIPTOR*
# PRT(24) = SEGMENT DESCRIPTOR, TYPE = 1, RELATIVE ADDRESS = 0044, SEGMENT NUMBER = 0002.
	.WORD	07500000000001211	# main program, continuation point
# REAL RESULT;
# PRT(25) = RESULT
	.WORD	0
# REAL A, B, C;
# PRT(26) = A
	.WORD	00123456701234567
# PRT(27) = B
	.WORD	0
# PRT(30) = C
	.WORD	03333117777224444
# PRT(31) = CALLEE
# PRT(31) = SEGMENT DESCRIPTOR, TYPE = 1, RELATIVE ADDRESS = 0024, SEGMENT NUMBER = 0003.
	.WORD	07500000000001305
# PRT(32) = *LIST, LABEL, OR SEGMENT DESCRIPTOR*
# PRT(32) = SEGMENT DESCRIPTOR, TYPE = 0, RELATIVE ADDRESS = 0004, SEGMENT NUMBER = 0002.
	.WORD	07400000000001201
# PRT(33) = *LIST, LABEL, OR SEGMENT DESCRIPTOR*
# PRT(33) = SEGMENT DESCRIPTOR, TYPE = 0, RELATIVE ADDRESS = 0024, SEGMENT NUMBER = 0002.
	.WORD	07400000000001205

# start of task (segment 1)
	.ORG	01100
	LITC	0	# what for?
	MKS		# prepare to call main
	OPDC	024	# PRT(24)
# spurious in listing: BFW
	LITC	5
	COM
	ZPI

# start of main program (segment 2)
	.ORG	01200
# RESULT:= CALLEE(A+C, A<C, 302);
	MKS		# prepare to call CALLEE
# branch around code for first expression parameter
	LITC  010
	BFW
	NOP		# word alignment
# now code for first expression parameter @1104
	OPDC	026	# A
	OPDC	030	# C
	LND		# should be ADD, but we don't have that yet
	LITC	021	# temp storage
	STD
	DESC	021	# pointer to that storage
	RTS
# load address of first expression parameter code
	LITC	032	# descriptor address
	LOD		# load it (F field is not set)
	DESC	01000	# dummy to get the F register into the C field
	CTF		# move it into descriptor
# branch around code for second expression parameter
	LITC	012
	BFW
	NOP		# word alignment
	NOP		# word alignment
	NOP		# word alignment
# now code for second expression parameter @1124
	OPDC	026	# A
	OPDC	030	# C
	LOR		# should be LSS, but we don't have that yet
#	LITC	0	# this EXTRA LITC is somewhat odd, its overwritten by the next
	LITC	021	# temp storage
	STD
	DESC	021	# pointer to that storage
	RTS
# load address of second expression parameter code
	LITC	033	# descriptor address
	LOD		# load it (F field is not set)
	DESC	01000	# dummy to get the F register into the C field
	CTF		# move it into descriptor
# load simple third parameter
	LITC	0456
# now call CALLEE
	OPDC	031
# store into RESULT
	LITC	025	# RESULT
	STD
# END. program
	XIT
	LITC	0
	LITC	011
	LBU

# start of callee (segment 3)
# REAL PROCEDURE CALLEE(A, B, C);
	.ORG	01300
# REAL A;
# BOOLEAN B;
# INTEGER C;
# BEGIN
# REAL U, V, W, X, Y, Z;
# STACK(F-3) = A
# STACK(F-2) = B
# STACK(F-1) = C
# STACK(F+0) = RCW
# STACK(F+1) = ?
# STACK(F+2) = result
# STACK(F+3) = U
# STACK(F+4) = V
# STACK(F+5) = W
# STACK(F+6) = X
# STACK(F+7) = Y
# STACK(F+10) = Z
# IF B THEN
	OPDC	F-2	# B
# branch around the true part if false
	LITC	7
	BFC
# true part
# CALLEE:= A*C
	OPDC	F-3	# A
	OPDC	F-1	# C
	LND		# should be MUL
	LITC	F+2	# result
	STD
# branch around the false part
	LITC	5
	BFW
# ELSE
# CALLEE:= A-C;
	OPDC	F-3	# A
	OPDC	F-1	# C
	LOR		# should be SUB
	LITC	F+2	# result
	STD
# END CALLEE;
	OPDC	F+2	# result
	RTN
	NOP		# word alignment
	NOP		# word alignment
	NOP		# word alignment
# the real function entry point @offset 24
	LITC	0	# space for ?
	LITC	0	# space for result
	LITC	0	# space for U
	LITC	0	# space for V
	LITC	0	# space for W
	LITC	0	# space for X
	LITC	0	# space for Y
	LITC	0	# space for Z
	LITC	0	# space for ? probably ensure space for Z is on the real stack
# branch to begin (7 words)
	LITC	7
	LBC

# now lets go
	.SET	R	010	# 00
	.SET	S	0100
	.SET	F	012345
	.ORG	01100
	.RUN
	.END
