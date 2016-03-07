# Test operators - regular function calls

# assemble a small program
	.ORG	020
# space for function result
	LITC	0
# mark stack
	MKS
# push three parameters
	LITC	0222
	LITC	0444
	LITC	0456
# call function 1
	DESC	040
# get result into A
	LNG
	LNG
# stop
	ZPI

# program descriptor for functions
	.ORG	040
	.WORD	07500000000000050
	.WORD	07500000000000060

# function 1 itself
	.ORG	050
# allocate space for result of function 2
	LITC	0
# prepare to call function 2
	MKS
# note that from now on, F-relative adressing uses (R+7) instead,
# where MKS has stored the previous F register
# load our three parameters as parameters for function 2
	OPDC	F-3
	OPDC	F-2
	OPDC	F-1
# now call function 2
	DESC	041
# now F-relative addressing is with our F register
# negate the result
	LNG
# load address for result
	DESC	F-5
# store result
	STD
# return
	XIT

# function 2 itself
	.ORG	060
# allocate some local storage - will never be used
	LITC	0
	LITC	0
	LITC	0
	LITC	0
	LITC	0
	LITC	0
# load address for result
	DESC	F-5
# load the three parameters
	OPDC	F-3
	OPDC	F-2
	OPDC	F-1
	LND
	LOR
# store result into function 1 stack
	XCH
	STD
# return
	XIT


# now lets go
	.SET	S	01000
	.SET	F	012345
# pretend A and B full, so they get pushed to stack
	.SET	AROF	1
	.SET	BROF	1
	.ORG	020
	.RUN
# verify results
	.VFY	BROF	0
	.VFY	AROF	1
	.VFY	A	03777777777777111
# stack should now be 1002 and F hold the original value
	.VFY	S	01002
	.VFY	F	012345
	.END
