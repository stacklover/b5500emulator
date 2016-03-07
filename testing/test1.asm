# Test operators
# assemble a small program
	.ORG	020
	LITC	0222
	LITC	0444
	LITC	0456
	LND
	LOR
	LNG
	ZPI
	LITC	0111
	LQV
	ZPI
	DEL
	DEL
	ZPI
# now lets go
	.SET	S	01000
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
# next program piece
	.RUN
# verify results
	.VFY	BROF	1
	.VFY	AROF	0
	.VFY	B	0777
	.RUN
	.VFY	S	01001
	.END
