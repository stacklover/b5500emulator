/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c)	2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*	see LICENSE
************************************************************************
* instruction table for	assembler/disassembler
************************************************************************
* 2016-02-29  R.Meyer
*   From thin air (based on my Pascal P5 assembler).
* 2017-09-30  R.Meyer
*   overhaul of file names
***********************************************************************/

#include <stdio.h>
#include "common.h"

const INSTRUCTION instruction_table[] =	{
//
// pseudo instructions
//
	{".ORG", 00000,	OP_EXPR, OP_ORG, false},
	{".RUN", 00000,	OP_NONE, OP_RUN, false},
	{".END", 00000,	OP_NONE, OP_END, false},
	{".SET", 00000,	OP_REGVAL, OP_SET, false},
	{".VFY", 00000,	OP_REGVAL, OP_VFY, false},
	{".WORD", 00000, OP_EXPR, OP_WORD, false},
	{".SYLL", 00000, OP_EXPR, OP_SYLL, false},
//
// WORD	mode instructions
//
	{"LITC", 00000,	OP_RELA, OP_TOP10, false},
	{"OPDC", 00002,	OP_RELA, OP_TOP10, false},
	{"DESC", 00003,	OP_RELA, OP_TOP10, false},
// single-precision numerics
	{"ADD",	 00101,	OP_NONE, OP_ASIS, false},
	{"SUB",	 00301,	OP_NONE, OP_ASIS, false},
	{"MUL",	 00401,	OP_NONE, OP_ASIS, false},
	{"DIV",	 01001,	OP_NONE, OP_ASIS, false},
	{"IDV",	 03001,	OP_NONE, OP_ASIS, false},
	{"RDV",	 07001,	OP_NONE, OP_ASIS, false},
// double-precision numerics
	{"DLA",	 00105,	OP_NONE, OP_ASIS, false},
	{"DLS",	 00305,	OP_NONE, OP_ASIS, false},
	{"DLM",	 00405,	OP_NONE, OP_ASIS, false},
	{"DLD",	 01005,	OP_NONE, OP_ASIS, false},
// Control State and communication ops
	{"PRL",	 00111,	OP_NONE, OP_ASIS, false},
	{"ITI",	 00211,	OP_NONE, OP_ASIS, false},
	{"RTR",	 00411,	OP_NONE, OP_ASIS, false},
	{"COM",	 01011,	OP_NONE, OP_ASIS, false},
	{"IOR",	 02111,	OP_NONE, OP_ASIS, false},
	{"HP2",	 02211,	OP_NONE, OP_ASIS, false},
	{"ZPI",	 02411,	OP_NONE, OP_ASIS, false},
	{"SFI",	 03011,	OP_NONE, OP_ASIS, false},
	{"SFT",	 03411,	OP_NONE, OP_ASIS, false},
	{"IP1",	 04111,	OP_NONE, OP_ASIS, false},
	{"IP2",	 04211,	OP_NONE, OP_ASIS, false},
	{"IIO",	 04411,	OP_NONE, OP_ASIS, false},
	{"IFT",	 05111,	OP_NONE, OP_ASIS, false},
// logical (bitmask) ops
	{"LNG",	 00115,	OP_NONE, OP_ASIS, false},
	{"LOR",	 00215,	OP_NONE, OP_ASIS, false},
	{"LND",	 00415,	OP_NONE, OP_ASIS, false},
	{"LQV",	 01015,	OP_NONE, OP_ASIS, false},
	{"MOP",	 02015,	OP_NONE, OP_ASIS, false},
	{"MDS",	 04015,	OP_NONE, OP_ASIS, false},
// load	& store	ops
	{"CID",	 00121,	OP_NONE, OP_ASIS, false},
	{"CIN",	 00221,	OP_NONE, OP_ASIS, false},
	{"STD",	 00421,	OP_NONE, OP_ASIS, false},
	{"SND",	 01021,	OP_NONE, OP_ASIS, false},
	{"LOD",	 02021,	OP_NONE, OP_ASIS, false},
	{"ISD",	 04121,	OP_NONE, OP_ASIS, false},
	{"ISN",	 04221,	OP_NONE, OP_ASIS, false},
// comparison &	misc. stack ops
	{"GEQ",	 00125,	OP_NONE, OP_ASIS, false},
	{"GTR",	 00225,	OP_NONE, OP_ASIS, false},
	{"NEQ",	 00425,	OP_NONE, OP_ASIS, false},
	{"XCH",	 01025,	OP_NONE, OP_ASIS, false},
	{"FTC",	 01425,	OP_NONE, OP_ASIS, false},
	{"DUP",	 02025,	OP_NONE, OP_ASIS, false},
	{"FTF",	 03425,	OP_NONE, OP_ASIS, false},
	{"LEQ",	 04125,	OP_NONE, OP_ASIS, false},
	{"LSS",	 04225,	OP_NONE, OP_ASIS, false},
	{"EQL",	 04425,	OP_NONE, OP_ASIS, false},
	{"CTC",	 05425,	OP_NONE, OP_ASIS, false},
	{"CTF",	 07425,	OP_NONE, OP_ASIS, false},
// branch, sign-bit, interrogate ops
	{"BBC",	 00131,	OP_BRAS, OP_BRAS, false},
	{"BFC",	 00231,	OP_BRAS, OP_BRAS, false},
	{"SSN",	 00431,	OP_NONE, OP_ASIS, false},
	{"CHS",	 01031,	OP_NONE, OP_ASIS, false},
	{"TOP",	 02031,	OP_NONE, OP_ASIS, false},
	{"LBC",	 02131,	OP_BRAW, OP_BRAW, false},
	{"LFC",	 02231,	OP_BRAW, OP_BRAW, false},
	{"TUS",	 02431,	OP_NONE, OP_ASIS, false},
	{"BBW",	 04131,	OP_BRAS, OP_BRAS, false},
	{"BFW",	 04231,	OP_BRAS, OP_BRAS, false},
	{"SSP",	 04431,	OP_NONE, OP_ASIS, false},
	{"LBU",	 06131,	OP_BRAW, OP_BRAW, false},
	{"LFU",	 06231,	OP_BRAW, OP_BRAW, false},
	{"TIO",	 06431,	OP_NONE, OP_ASIS, false},
	{"FBS",	 07031,	OP_NONE, OP_ASIS, false},
// exit	& return ops
	{"BRT",	 00135,	OP_NONE, OP_ASIS, false},
	{"RTN",	 00235,	OP_NONE, OP_ASIS, false},
	{"XIT",	 00435,	OP_NONE, OP_ASIS, false},
	{"RTS",	 01235,	OP_NONE, OP_ASIS, false},
// index, mark stack, etc.
	{"INX",	 00141,	OP_NONE, OP_ASIS, false},
	{"COC",	 00241,	OP_NONE, OP_ASIS, false},
	{"MKS",	 00441,	OP_NONE, OP_ASIS, false},
	{"CDC",	 01241,	OP_NONE, OP_ASIS, false},
	{"SSF",	 02141,	OP_NONE, OP_ASIS, false},
	{"LLL",	 02541,	OP_NONE, OP_ASIS, false},
	{"CMN",	 04441,	OP_NONE, OP_ASIS, false},
// ISO=Variable	Field Isolate op
	{"ISO",	 00045,	OP_EXPR, OP_TOP6, false},
// delete & conditional	branch ops
	{"DEL",	 00051,	OP_NONE, OP_ASIS, false},
	{"CFN",	 00051,	OP_EXPR, OP_TOP4, false},
	{"CBN",	 00151,	OP_EXPR, OP_TOP4, false},
	{"CFD",	 00251,	OP_EXPR, OP_TOP4, false},
	{"CBD",	 00351,	OP_EXPR, OP_TOP4, false},
// NOP & DIA=Dial A ops
	{"NOP",	 00055,	OP_NONE, OP_ASIS, false},
	{"DIA",	 00055,	OP_EXPR, OP_TOP6, false},
// XRT & DIB=Dial B ops
	{"XRT",	 00061,	OP_NONE, OP_ASIS, false},
	{"DIB",	 00061,	OP_EXPR, OP_TOP6, false},
// TRB=Transfer	Bits
	{"TRB",	 00065,	OP_EXPR, OP_TOP6, false},
// FCL=Compare Field Low
	{"FCL",	 00071,	OP_EXPR, OP_TOP6, false},
// FCE=Compare Field Equal
	{"FCE",	 00075,	OP_EXPR, OP_TOP6, false},
//
// CHAR	mode instructions
//
// CMX,	EXC: Exit character mode
	{"EXC",	 00000,	OP_NONE, OP_ASIS, true},
	{"CMX",	 00100,	OP_NONE, OP_ASIS, true},
// BSD=Skip bit	destination
	{"BSD",	 00002,	OP_EXPR, OP_TOP6, true},
// BSS=Skip bit	source
	{"BSS",	 00003,	OP_EXPR, OP_TOP6, true},
// RDA=Recall destination address
	{"RDA",	 00004,	OP_EXPR, OP_TOP6, true},
// TRW=Transfer	words
	{"TRW",	 00005,	OP_EXPR, OP_TOP6, true},
// SED=Set destination address
	{"SED",	 00006,	OP_EXPR, OP_TOP6, true},
// TDA=Transfer	destination address
	{"TDA",	 00007,	OP_NONE, OP_ASIS, true},
// Control State ops
	{"ZPI",	 02411,	OP_NONE, OP_ASIS, true},
	{"SFI",	 03011,	OP_NONE, OP_ASIS, true},
	{"SFT",	 03411,	OP_NONE, OP_ASIS, true},
// TBN=Transfer	blanks for non-numeric
	{"TBN",	 00012,	OP_EXPR, OP_TOP6, true},
// SDA=Store destination address
	{"SDA",	 00014,	OP_EXPR, OP_TOP6, true},
// SSA=Store source address
	{"SSA",	 00015,	OP_EXPR, OP_TOP6, true},
// SFD=Skip forward destination
	{"SFD",	 00016,	OP_EXPR, OP_TOP6, true},
// SRD=Skip reverse destination
	{"SRD",	 00017,	OP_EXPR, OP_TOP6, true},
// SES=Set source address
	{"SES",	 00022,	OP_EXPR, OP_TOP6, true},
// TEQ=Test for	equal
	{"TEQ",	 00024,	OP_EXPR, OP_TOP6, true},
// TNE=Test for	not equal
	{"TNE",	 00025,	OP_EXPR, OP_TOP6, true},
// TEG=Test for	equal or greater
	{"TEG",	 00026,	OP_EXPR, OP_TOP6, true},
// TGR=Test for	greater
	{"TGR",	 00027,	OP_EXPR, OP_TOP6, true},
// SRS=Skip reverse source
	{"SRS",	 00030,	OP_EXPR, OP_TOP6, true},
// SFS=Skip forward source
	{"SFS",	 00031,	OP_EXPR, OP_TOP6, true},
// FSB=Field subtract (aux)
	{"FSUX",  00032, OP_EXPR, OP_TOP6, true},
// FAD=Field add (aux)
	{"FADX",  00033, OP_EXPR, OP_TOP6, true},
// TEL=Test for	equal or less
	{"TEL",	 00034,	OP_EXPR, OP_TOP6, true},
// TLS=Test for	less
	{"TLS",	 00035,	OP_EXPR, OP_TOP6, true},
// TAN=Test for	alphanumeric
	{"TAN",	 00036,	OP_EXPR, OP_TOP6, true},
// BIT=Test bit
	{"BIT",	 00037,	OP_EXPR, OP_TOP6, true},
// INC=Increase	TALLY
	{"INC",	 00040,	OP_EXPR, OP_TOP6, true},
// STC=Store TALLY
	{"STC",	 00041,	OP_EXPR, OP_TOP6, true},
// SEC=Set TALLY
	{"SEC",	 00042,	OP_EXPR, OP_TOP6, true},
// CRF=Call repeat field
	{"CRF",	 00043,	OP_EXPR, OP_TOP6, true},
// JNC=Jump out	of loop	conditional
	{"JNC",	 00044,	OP_EXPR, OP_TOP6, true},
// JFC=Jump forward conditional
	{"JFC",	 00045,	OP_EXPR, OP_TOP6, true},
// JNS=Jump out	of loop
	{"JNS",	 00046,	OP_EXPR, OP_TOP6, true},
// JFW=Jump forward unconditional
	{"JFW",	 00047,	OP_EXPR, OP_TOP6, true},
// RCA=Recall control address
	{"RCA",	 00050,	OP_EXPR, OP_TOP6, true},
// ENS=End loop
	{"ENS",	 00051,	OP_NONE, OP_ASIS, true},
// BNS=Begin loop
	{"BNS",	 00052,	OP_EXPR, OP_TOP6, true},
// RSA=Recall source address
	{"RSA",	 00053,	OP_EXPR, OP_TOP6, true},
// SCA=Store control address
	{"SCA",	 00054,	OP_EXPR, OP_TOP6, true},
// JRC=Jump reverse conditional
	{"JRC",	 00055,	OP_EXPR, OP_TOP6, true},
// TSA=Transfer	source address
	{"TSA",	 00056,	OP_NONE, OP_ASIS, true},
// JRV=Jump reverse unconditional
	{"JRV",	 00057,	OP_EXPR, OP_TOP6, true},
// CEQ=Compare equal
	{"CEQ",	 00060,	OP_EXPR, OP_TOP6, true},
// CNE=Compare not equal
	{"CNE",	 00061,	OP_EXPR, OP_TOP6, true},
// CEG=Compare greater or equal
	{"CEG",	 00062,	OP_EXPR, OP_TOP6, true},
// CGR=Compare greater
	{"CGR",	 00063,	OP_EXPR, OP_TOP6, true},
// BIS=Set bit
	{"BIS",	 00064,	OP_EXPR, OP_TOP6, true},
// BIR=Reset bit
	{"BIR",	 00065,	OP_EXPR, OP_TOP6, true},
// OCV=Output convert
	{"OCV",	 00066,	OP_EXPR, OP_TOP6, true},
// ICV=Input convert
	{"ICV",	 00067,	OP_EXPR, OP_TOP6, true},
// CEL=Compare equal or	less
	{"CEL",	 00070,	OP_EXPR, OP_TOP6, true},
// CLS=Compare less
	{"CLS",	 00071,	OP_EXPR, OP_TOP6, true},
// FSU=Field subtract
	{"FSU",	 00072,	OP_EXPR, OP_TOP6, true},
// FAD=Field add
	{"FAD",	 00073,	OP_EXPR, OP_TOP6, true},
// TRP=Transfer	program	characters
	{"TRP",	 00074,	OP_EXPR, OP_TOP6, true},
// TRN=Transfer	source numerics
	{"TRN",	 00075,	OP_EXPR, OP_TOP6, true},
// TRZ=Transfer	source zones
	{"TRZ",	 00076,	OP_EXPR, OP_TOP6, true},
// TRS=Transfer	source characters
	{"TRS",	 00077,	OP_EXPR, OP_TOP6, true},
// end of table
	{0, 0, OP_NONE,	OP_NONE, false},
};

