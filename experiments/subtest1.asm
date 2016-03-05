# Test operators

# assemble a small program
	.ORG	020
# space for function result
	LITC	0
# mark stack
	MKS
# push some parameters
	LITC	0222
	LITC	0444
	LITC	0456
# call the subroutine
	DESC	040
# get result into A
	LNG
	LNG
# stop
	ZPI

# program descriptor for function
	.ORG	040
	.WORD	07500000000000050

# function itself
	.ORG	050
# load address for result
	DESC	F-5
# load parameters
	OPDC	F-3
	OPDC	F-2
	OPDC	F-1
	LND
	LOR
	LNG
# store result
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
# stack should now be 1002
	.VFY	S	01002
	.VFY	F	012345
	.END
