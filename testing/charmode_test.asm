# %% RETRO-B5500 EMULATOR CHARACTER MODE TESTS
# % 2013-01-26 P.KIMPEL
# % ORIGINAL VERSION

# BEGIN

#PRT(201) = *SEGMENT DESCRIPTOR*
	.ORG	0201
	.WORD	0

# INTEGER I;
#PRT(202) = I
	.ORG	0202
	.WORD	0

# REAL R;
#PRT(203) = R
	.ORG	0203
	.WORD	0

# ARRAY S[16]:=
# "01234567","89ABCDEF","GHIJKLMN","OPQRSTUV","WXYZ +-/",
# "NOW IS T","HE TIME ","FOR ALL ","GOOD MEN"," TO COME",
# " TO THE ","AID OF T","HEIR PAR","TY.     ","1234567Q","12345678";
#PRT(204) = S
	.ORG	0204
	.WORD	05000200000001000	# array S at @1000 for 16
	.ORG	01000
	.WORD	"01234567"
	.WORD	"89ABCDEF"
	.WORD	"GHIJKLMN"
	.WORD	"OPQRSTUV"
	.WORD	"WXYZ +-/"
	.WORD	"NOW IS T"
	.WORD	"HE TIME "
	.WORD	"FOR ALL "
	.WORD	"GOOD MEN"
	.WORD	" TO COME"
	.WORD	" TO THE "
	.WORD	"AID OF T"
	.WORD	"HEIR PAR"
	.WORD	"TY.     "
	.WORD	"1234567Q"
	.WORD	"12345678"

# ARRAY D[16];
#PRT(205) = D
	.ORG	0205
	.WORD	05000200000001020	# array D at @1020 for 16
	.ORG	01020
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0

# LABEL ENTRY, START;

# ENTRY:@20: GO TO START;
	.ORG	020
	LITC	0
	OPDC	0211

# START:*:
	.ORG	020

# D[0]:= 76543210;
	LITC	0166
	LFU

	.ORG	0206
	LITC	0	# index 0
	DESC	0205	# addr of array element D[index]
	OPDC	0265	# addr of constant 76543210
	NOP
	XCH		# make address be top of stack
	STD		# store destructive

# STREAM(R:=-D[0] : COUNT:=0, S:=S, D:=D);
	NOP
	LITC	0	# index 0
	OPDC	0205	# load value at D[index]
	CHS		# invert sign (R is MKS-1 or RCW-5)
	MKS		# mark stack (is MKS+0 or RCW-4)
	LITC	0	# constant 0 (COUNT is MKS+1 or RCW-3)
	LITC	0204	# address of descriptor of S
	LOD		# load descriptor (S is MKS+2 or RCW-2)
	LITC	0205	# address of descriptor of D
	LOD		# load descriptor (D is MKS+3 or RCW-1)
	CMN		# enter character mode (is RCW+0)
# BEGIN
# SI:= S;
	RSA	2	# S at RCW-2
# SI:= SI+51;
	SFS	51	# source pointer +51
# DI:= DI+6;
	SFD	6	# destination pointer +6
# DS:= 5 CHR;
	TRS	5	# copy 5 chars
# SI:= LOC R;
	SES	5	# R at RCW-5
# DI:= D;
	RDA	1	# D at RCW-1
# DI:= DI+17;
	SFD	17	# destination pointer +17
# DS:= 8 DEC;
	OCV	8	# convert binary to 8 numeric
# SI:= D;
	RSA	1	# D at RCW-1
# 2(SI:= SI+7);
	BNS	2	# loop 2x
	SFS	7	# source pointer +7
	ENS		# end loop
# SI:= SI+3;
	SFS	3	# source pointer +3
# DI:= LOC R;
	SED	5	# R at RCW-5
# DS:= 8 OCT;
	ICV	8	# convert 8 numeric to binary
# DI:= D;
	RDA	1	# D at RCW-1
# DI:= DI+9;
	SFD	9	# destination pointer +9
# COUNT:= DI;
	SDA	3	# store destination address in RCW-3 (COUNT)
# DS:= 9 LIT "ADGJMPSV7";
	TRP	9	# copy 9 chars from code stream
	.SYLL	00021
	.SYLL	02427
	.SYLL	04144
	.SYLL	04762
	.SYLL	06507
# SI:= COUNT;
	RSA	3	# recall source address from RCW-3 (COUNT)
# DS:= 9 ZON;
	TRZ	9	# copy 9 chars zone bits (BAxxxx) only, keep 8421
# SI:= COUNT;
	RSA	3	# recall source address from RCW-3 (COUNT)
# DS:= 9 NUM;
	TRN	9	# copy 9 chars numeric bits (xx8421) only, set BA=00
# SI:= COUNT;
	RSA	3	# recall source address from RCW-3 (COUNT)
# DI:= COUNT;
	RDA	3	# recall destination address from RCW-3 (COUNT)
# DI:= DI+11;
	SFD	11	# destination pointer +11
# TALLY:= 3;
	SEC	3	# set tally to 3
# COUNT:= TALLY;
	STC	3	# store tally in RCW-3 (COUNT)
# DS:= COUNT WDS;
	CRF	3	# load repeat field from RCW-3 (COUNT)
	TRW	0	# copy xx words
# DI:= D;
	RDA	1	# recall destination address from RCW-1 (D)
# DI:= DI+9;
	SFD	9	# destination pointer +9
# DS:= 9 FILL;
	TBN	9	# copy max 9 blanks while 0
# SI:= S;
	RSA	2	# S at RCW-2
# IF SC > "A" THEN
	TGR	021	# test greater than 021 ('A')
	JFC	1	# jump forward if false
# DS:= SET;
	BIS	1	# set one bit to 1
# IF SC = "A" THEN
	TEG	021	# test greater than or equal 021 ('A')
	JFC	1	# jump forward if false
# DS:= SET;
	BIS	1	# set one bit to 1
# IF SC = "A" THEN
	TEQ	021	# test equal 021 ('A')
	JFC	1	# jump forward if false
# DS:= SET;
	BIS	1	# set one bit to 1
# IF SC = "A" THEN
	TEL	021	# test less than or equal 021 ('A')
	JFC	1	# jump forward if false
# DS:= SET;
	BIS	1	# set one bit to 1
# IF SC < "A" THEN
	TLS	021	# test greater than 021 ('A')
	JFC	1	# jump forward if false
# DS:= SET;
	BIS	1	# set one bit to 1
# IF SC ? "A" THEN
	TNE	021	# test not equal 021 ('A')
	JFC	1	# jump forward if false
# DS:= SET;
	BIS	1	# set one bit to 1
# IF SC = ALPHA THEN
	TAN	021	# test greater than or equal 021 ('A') AND alphanumeric
	JFC	1	# jump forward if false
# DS:= SET;
	BIS	1	# set one bit to 1
# SI:= S;
	RSA	2	# S at RCW-2
# DI:= D;
	RDA	1	# D at RCW-1
# IF 9 SC > DC THEN
	CGR	9	# compare 9 chars for greater
	JFC	1	# jump forward if false
# TALLY:= TALLY+1;
	INC	1	# increment tally by 1
# SI:= S;
	RSA	2	# S at RCW-2
# DI:= D;
	RDA	1	# D at RCW-1
# IF 9 SC = DC THEN
	CEG	9	# compare 9 chars for greater or equal
	JFC	1	# jump forward if false
# TALLY:= TALLY+1;
	INC	1	# increment tally by 1
# SI:= S;
	RSA	2	# S at RCW-2
# DI:= D;
	RDA	1	# D at RCW-1
# IF 9 SC = DC THEN
	CEQ	9	# compare 9 chars for equal
	JFC	1	# jump forward if false
# TALLY:= TALLY+1;
	INC	1	# increment tally by 1
# SI:= S;
	RSA	2	# S at RCW-2
# DI:= D;
	RDA	1	# D at RCW-1
# IF 9 SC = DC THEN
	CEL	9	# compare 9 chars for less or equal
	JFC	1	# jump forward if false
# TALLY:= TALLY+1;
	INC	1	# increment tally by 1
# SI:= S;
	RSA	2	# S at RCW-2
# DI:= D;
	RDA	1	# D at RCW-1
# IF 9 SC < DC THEN
	CLS	9	# compare 9 chars for less
	JFC	1	# jump forward if false
# TALLY:= TALLY+1;
	INC	1	# increment tally by 1
# SI:= S;
	RSA	2	# S at RCW-2
# DI:= D;
	RDA	1	# D at RCW-1
# IF 9 SC ? DC THEN
	CNE	9	# compare 9 chars for not equal
	JFC	1	# jump forward if false
# TALLY:= TALLY+1;
	INC	1	# increment tally by 1
# 3(IF SB THEN JUMP OUT ELSE SKIP SB);
	BNS	3	# loop 3x
	BIT	1	# test bit in source for 1
	JFC	3	# jump forward if false
	JNS	0	# terminate loop, 0=no jump
	JFW	3	# jump forward 3
	JFW	1	# jump forward 1
	BSS	1	# skip 1 source bit
	ENS		# end loop
# 2(IF SB THEN
	BNS	2	# loop 2x
	BIT	1	# test bit in source for 1
	JFC	2	# jump forward if false
# SKIP SB
	BSS	1	# skip 1 source bit
# ELSE
	JFW	3	# jump forward 3
# BEGIN
# SKIP 2 SB;
	BSS	2	# skip 2 source bits
# JUMP OUT;
	JNS	0	# terminate loop, 0=no jump
	JFW	1	# jump forward 1
# END;
# );
	ENS		# end loop
# SI:= SC;
	TSA		# transfer source address
# DI:= DC;
	TDA		# transfer destination address
# SI:= S;
	RSA	2	# S at RCW-2
# DI:= D;
	RDA	1	# D at RCW-1
# DI:= DI+8;
	SFD	8	# destination pointer +8
# SKIP 40 SB;
	BSS	40	# skip 40 source bits
# SKIP 40 DB;
	BSD	40	# skip 40 destination bits
# TALLY:= 10;
	SEC	10	# set tally = 10
# COUNT:= TALLY;
	STC	3	# store tally in RCW-3 (COUNT)
# COUNT(
	CRF	3	# load repeat field from RCW-3 (COUNT)
	BNS	7
# IF SB THEN DS:= SET ELSE DS:= RESET;
	BIT	1	# test bit in source for 1
	JFC	2	# jump forward 2 if false
	BIS	1	# set one bit to 1
	JFW	1	# jump forward 1
	BIR	1	# set one bit to 0
# SKIP SB;
	BSS	1	# skip 1 source bit
# );
	ENS		# end loop
# DI:= D;
	RDA	1	# D at RCW-1
# DS:= 9 LIT "00000123M";
	TRP	9	# copy 9 chars from code stream
	.SYLL	00000
	.SYLL	00000
	.SYLL	00000
	.SYLL	00102
	.SYLL	00344
# DS:= 9 LIT "765432100";
	TRP	9	# copy 9 chars from code stream
	.SYLL	00007
	.SYLL	00605
	.SYLL	00403
	.SYLL	00201
	.SYLL	00000
# SI:= D;
	RSA	1	# D at RCW-1
# DI:= D;
	RDA	1	# D at RCW-1
# DI:= DI+9;
	SFD	9	# destination pointer +9
# DS:= 9 ADD;
	FAD	9	# field add 9 chars
# DI:= D;
	RDA	1	# D at RCW-1
# DS:= 9 LIT "00000123M";
	TRP	9	# copy 9 chars from code stream
	.SYLL	00000
	.SYLL	00000
	.SYLL	00000
	.SYLL	00102
	.SYLL	00344
# DS:= 9 LIT "765432100";
	TRP	9	# copy 9 chars from code stream
	.SYLL	00007
	.SYLL	00605
	.SYLL	00403
	.SYLL	00201
	.SYLL	00000
# SI:= D;
	RSA	1	# D at RCW-1
# DI:= D;
	RDA	1	# D at RCW-1
# DI:= DI+9;
	SFD	9	# destination pointer +9
# DS:= 9 SUB;
	FSU	9	# field sub 9 chars
# COUNT:= CI;
	SCA	3	# store control address in RCW-3 (count)
# CI:= COUNT; % SINCE RCA INCREMENTS L, THIS SHOULD BE OK
	RCA	3	# load control address from RCW-3 (count)
# TALLY:= COUNT;
	SEC	0	# set tally = 0
	CRF	3	# load repeat field from RCW-3 (COUNT)
	SEC	0	# set tally = 0
# END STREAM;
	CMX		# exit character mode
# P(DEL); % EAT THE WORD LEFT BELOW MSCW BY THE STREAM
	DEL		# delete top of stack
# GO TO START;
	ZPI		# stop
	LITC	056
	LBU
	NOP
	NOP
	NOP

	.ORG	0265	# constant 76543210
	.WORD	76543210
#1324 0000000443772352
# END.

	OPDC	01020
	ZPI
	.ORG	020
	.RUN
	.END
