# single precision tests

# add/sub tests
	.ORG	01400
	NOP
	ADD
	ZPI
# now lets go
	.SET	isP1	1
	.SET	R	020	# 00
	.SET	S	01000
	.SET	F	012345
	.SET	NCSF	1

	.SET	AROF	1
	.SET	BROF	1
	.SET	A	0007777777777777
	.SET	B	0007777777777777
	.ORG	01400
	.RUN
	.VFY	B	0012000000000000

	.SET	AROF	1
	.SET	BROF	1
	.SET	A	0007777777777777
	.SET	B	0000000000000001
	.ORG	01400
	.RUN
	.VFY	B	0011000000000000

	.SET	AROF	1
	.SET	BROF	1
	.SET	A	0007777777777777
	.SET	B	0777777777777777
	.ORG	01400
	.RUN
	.VFY	B	0777777777777777

	.SET	AROF	1
	.SET	BROF	1
	.SET	A	0007777777777777
	.SET	B	0147777777777777
	.ORG	01400
	.RUN
	.VFY	B	0151000000000001

	.SET	AROF	1
	.SET	BROF	1
	.SET	A	0007777777777777
	.SET	B	0157777777777777
	.ORG	01400
	.RUN
	.VFY	B	0157777777777777


	.END
