/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* instruction table for assembler/disassembler
************************************************************************
* 2016-02-29  R.Meyer
*   From thin air (based on my Pascal P5 assembler).
***********************************************************************/

#include "b5500_common.h"

const INSTRUCTION instruction_table[] = {
//
// pseudo instructions
//
	{".ORG", 00000, OP_EXPR, OP_ORG},
	{".RUN", 00000, OP_NONE, OP_RUN},
	{".END", 00000, OP_NONE, OP_END},
	{".SET", 00000, OP_REGVAL, OP_SET},
	{".VFY", 00000, OP_REGVAL, OP_VFY},
	{".WORD", 00000, OP_EXPR, OP_WORD},
	{".SYLL", 00000, OP_EXPR, OP_SYLL},
//
// WORD mode instructions
//
	{"LITC", 00000, OP_RELA, OP_TOP10},
	{"OPDC", 00002, OP_RELA, OP_TOP10},
	{"DESC", 00003, OP_RELA, OP_TOP10},
// single-precision numerics
	{"ADD",  00101, OP_NONE, OP_ASIS},
	{"SUB",  00301, OP_NONE, OP_ASIS},
	{"MUL",  00401, OP_NONE, OP_ASIS},
	{"DIV",  01001, OP_NONE, OP_ASIS},
	{"IDV",  03001, OP_NONE, OP_ASIS},
	{"RDV",  07001, OP_NONE, OP_ASIS},
// double-precision numerics
	{"DLA",  00105, OP_NONE, OP_ASIS},
	{"DLS",  00305, OP_NONE, OP_ASIS},
	{"DLM",  00405, OP_NONE, OP_ASIS},
	{"DLD",  01005, OP_NONE, OP_ASIS},
// Control State and communication ops
	{"PRL",  00111, OP_NONE, OP_ASIS},
	{"ITI",  00211, OP_NONE, OP_ASIS},
	{"RTR",  00411, OP_NONE, OP_ASIS},
	{"COM",  01011, OP_NONE, OP_ASIS},
	{"IOR",  02111, OP_NONE, OP_ASIS},
	{"HP2",  02211, OP_NONE, OP_ASIS},
	{"ZPI",  02411, OP_NONE, OP_ASIS},
	{"SFI",  03011, OP_NONE, OP_ASIS},
	{"SFT",  03411, OP_NONE, OP_ASIS},
	{"IP1",  04111, OP_NONE, OP_ASIS},
	{"IP2",  04211, OP_NONE, OP_ASIS},
	{"IIO",  04411, OP_NONE, OP_ASIS},
	{"IFT",  05111, OP_NONE, OP_ASIS},
// logical (bitmask) ops
	{"LNG",  00115, OP_NONE, OP_ASIS},
	{"LOR",  00215, OP_NONE, OP_ASIS},
	{"LND",  00415, OP_NONE, OP_ASIS},
	{"LQV",  01015, OP_NONE, OP_ASIS},
	{"MOP",  02015, OP_NONE, OP_ASIS},
	{"MDS",  04015, OP_NONE, OP_ASIS},
// load & store ops
	{"CID",  00121, OP_NONE, OP_ASIS},
	{"CIN",  00221, OP_NONE, OP_ASIS},
	{"STD",  00421, OP_NONE, OP_ASIS},
	{"SND",  01021, OP_NONE, OP_ASIS},
	{"LOD",  02021, OP_NONE, OP_ASIS},
	{"ISD",  04121, OP_NONE, OP_ASIS},
	{"ISN",  04221, OP_NONE, OP_ASIS},
// comparison & misc. stack ops
	{"GEQ",  00125, OP_NONE, OP_ASIS},
	{"GTR",  00225, OP_NONE, OP_ASIS},
	{"NEQ",  00425, OP_NONE, OP_ASIS},
	{"XCH",  01025, OP_NONE, OP_ASIS},
	{"FTC",  01425, OP_NONE, OP_ASIS},
	{"DUP",  02025, OP_NONE, OP_ASIS},
	{"FTF",  03425, OP_NONE, OP_ASIS},
	{"LEQ",  04125, OP_NONE, OP_ASIS},
	{"LSS",  04225, OP_NONE, OP_ASIS},
	{"EQL",  04425, OP_NONE, OP_ASIS},
	{"CTC",  05425, OP_NONE, OP_ASIS},
	{"CTF",  07425, OP_NONE, OP_ASIS},
// branch, sign-bit, interrogate ops
	{"BBC",  00131, OP_BRAS, OP_BRAS},
	{"BFC",  00231, OP_BRAS, OP_BRAS},
	{"SSN",  00431, OP_NONE, OP_ASIS},
	{"CHS",  01031, OP_NONE, OP_ASIS},
	{"TOP",  02031, OP_NONE, OP_ASIS},
	{"LBC",  02131, OP_BRAW, OP_BRAW},
	{"LFC",  02231, OP_BRAW, OP_BRAW},
	{"TUS",  02431, OP_NONE, OP_ASIS},
	{"BBW",  04131, OP_BRAS, OP_BRAS},
	{"BFW",  04231, OP_BRAS, OP_BRAS},
	{"SSP",  04431, OP_NONE, OP_ASIS},
	{"LBU",  06131, OP_BRAW, OP_BRAW},
	{"LFU",  06231, OP_BRAW, OP_BRAW},
	{"TIO",  06431, OP_NONE, OP_ASIS},
	{"FBS",  07031, OP_NONE, OP_ASIS},
// exit & return ops
	{"BRT",  00135, OP_NONE, OP_ASIS},
	{"RTN",  00235, OP_NONE, OP_ASIS},
	{"XIT",  00435, OP_NONE, OP_ASIS},
	{"RTS",  01235, OP_NONE, OP_ASIS},
// index, mark stack, etc.
	{"INX",  00141, OP_NONE, OP_ASIS},
	{"COC",  00241, OP_NONE, OP_ASIS},
	{"MKS",  00441, OP_NONE, OP_ASIS},
	{"CDC",  01241, OP_NONE, OP_ASIS},
	{"SSF",  02141, OP_NONE, OP_ASIS},
	{"LLL",  02541, OP_NONE, OP_ASIS},
	{"CMN",  04441, OP_NONE, OP_ASIS},
// ISO=Variable Field Isolate op
	{"ISO",  00045, OP_EXPR, OP_TOP6},
// delete & conditional branch ops
	{"DEL",  00051, OP_NONE, OP_ASIS},
	{"CFN",  00051, OP_EXPR, OP_TOP4},
	{"CBN",  00151, OP_EXPR, OP_TOP4},
	{"CFD",  00251, OP_EXPR, OP_TOP4},
	{"CBD",  00351, OP_EXPR, OP_TOP4},
// NOP & DIA=Dial A ops
	{"NOP",  00055, OP_NONE, OP_ASIS},
	{"DIA",  00055, OP_EXPR, OP_TOP6},
// XRT & DIB=Dial B ops
	{"XRT",  00061, OP_NONE, OP_ASIS},
	{"DIB",  00061, OP_EXPR, OP_TOP6},
// TRB=Transfer Bits
	{"TRB",  00065, OP_EXPR, OP_TOP6},
// FCL=Compare Field Low
	{"FCL",  00071, OP_EXPR, OP_TOP6},
// FCE=Compare Field Equal
	{"FCE",  00075, OP_EXPR, OP_TOP6},
//
// CHAR mode instructions
//
// CMX, EXC: Exit character mode
	{"EXC",  00000, OP_NONE, OP_ASIS, true},
	{"CMX",  00100, OP_NONE, OP_ASIS, true},
// BSD=Skip bit destination
	{"BSD",  00002, OP_EXPR, OP_TOP6, true},
// BSS=Skip bit source
	{"BSS",  00003, OP_EXPR, OP_TOP6, true},
// RDA=Recall destination address
	{"RDA",  00004, OP_EXPR, OP_TOP6, true},
// TRW=Transfer words
	{"TRW",  00005, OP_EXPR, OP_TOP6, true},
// SED=Set destination address
	{"SED",  00006, OP_EXPR, OP_TOP6, true},
// TDA=Transfer destination address
	{"TDA",  00007, OP_NONE, OP_ASIS, true},
// Control State ops
	{"ZPI",  02411, OP_NONE, OP_ASIS, true},
   	{"SFI",  03011, OP_NONE, OP_ASIS, true},
	{"SFT",  03411, OP_NONE, OP_ASIS, true},
// TBN=Transfer blanks for non-numeric
	{"TBN",  00012, OP_EXPR, OP_TOP6, true},
// SDA=Store destination address
	{"SDA",  00014, OP_EXPR, OP_TOP6, true},
// SSA=Store source address
	{"SSA",  00015, OP_EXPR, OP_TOP6, true},
// SFD=Skip forward destination
	{"SFD",  00016, OP_EXPR, OP_TOP6, true},
// SRD=Skip reverse destination
	{"SRD",  00017, OP_EXPR, OP_TOP6, true},
// SES=Set source address
	{"SES",  00022, OP_EXPR, OP_TOP6, true},
// TEQ=Test for equal
	{"TEQ",  00024, OP_EXPR, OP_TOP6, true},
// TNE=Test for not equal
	{"TNE",  00025, OP_EXPR, OP_TOP6, true},
// TEG=Test for equal or greater
	{"TEG",  00026, OP_EXPR, OP_TOP6, true},
// TGR=Test for greater
	{"TGR",  00027, OP_EXPR, OP_TOP6, true},
// SRS=Skip reverse source
	{"SRS",  00030, OP_EXPR, OP_TOP6, true},
// SFS=Skip forward source
	{"SFS",  00031, OP_EXPR, OP_TOP6, true},
// FSB=Field subtract (aux)
	{"FSUX",  00032, OP_EXPR, OP_TOP6, true},
// FAD=Field add (aux)
	{"FADX",  00033, OP_EXPR, OP_TOP6, true},
// TEL=Test for equal or less
	{"TEL",  00034, OP_EXPR, OP_TOP6, true},
// TLS=Test for less
	{"TLS",  00035, OP_EXPR, OP_TOP6, true},
// TAN=Test for alphanumeric
	{"TAN",  00036, OP_EXPR, OP_TOP6, true},
// BIT=Test bit
	{"BIT",  00037, OP_EXPR, OP_TOP6, true},
// INC=Increase TALLY
	{"INC",  00040, OP_EXPR, OP_TOP6, true},
// STC=Store TALLY
	{"STC",  00041, OP_EXPR, OP_TOP6, true},
// SEC=Set TALLY
	{"SEC",  00042, OP_EXPR, OP_TOP6, true},
// CRF=Call repeat field
	{"CRF",  00043, OP_EXPR, OP_TOP6, true},
// JNC=Jump out of loop conditional
	{"JNC",  00044, OP_EXPR, OP_TOP6, true},
// JFC=Jump forward conditional
	{"JFC",  00045, OP_EXPR, OP_TOP6, true},
// JNS=Jump out of loop
	{"JNS",  00046, OP_EXPR, OP_TOP6, true},
// JFW=Jump forward unconditional
	{"JFW",  00047, OP_EXPR, OP_TOP6, true},
// RCA=Recall control address
	{"RCA",  00050, OP_EXPR, OP_TOP6, true},
// ENS=End loop
	{"ENS",  00051, OP_NONE, OP_ASIS, true},
// BNS=Begin loop
	{"BNS",  00052, OP_EXPR, OP_TOP6, true},
// RSA=Recall source address
	{"RSA",  00053, OP_EXPR, OP_TOP6, true},
// SCA=Store control address
	{"SCA",  00054, OP_EXPR, OP_TOP6, true},
// JRC=Jump reverse conditional
	{"JRC",  00055, OP_EXPR, OP_TOP6, true},
// TSA=Transfer source address
	{"TSA",  00056, OP_NONE, OP_ASIS, true},
// JRV=Jump reverse unconditional
	{"JRV",  00057, OP_EXPR, OP_TOP6, true},
// CEQ=Compare equal
	{"CEQ",  00060, OP_EXPR, OP_TOP6, true},
// CNE=Compare not equal
	{"CNE",  00061, OP_EXPR, OP_TOP6, true},
// CEG=Compare greater or equal
	{"CEG",  00062, OP_EXPR, OP_TOP6, true},
// CGR=Compare greater
	{"CGR",  00063, OP_EXPR, OP_TOP6, true},
// BIS=Set bit
	{"BIS",  00064, OP_EXPR, OP_TOP6, true},
// BIR=Reset bit
	{"BIR",  00065, OP_EXPR, OP_TOP6, true},
// OCV=Output convert
	{"OCV",  00066, OP_EXPR, OP_TOP6, true},
// ICV=Input convert
	{"ICV",  00067, OP_EXPR, OP_TOP6, true},
// CEL=Compare equal or less
	{"CEL",  00070, OP_EXPR, OP_TOP6, true},
// CLS=Compare less
	{"CLS",  00071, OP_EXPR, OP_TOP6, true},
// FSU=Field subtract
	{"FSU",  00072, OP_EXPR, OP_TOP6, true},
// FAD=Field add
	{"FAD",  00073, OP_EXPR, OP_TOP6, true},
// TRP=Transfer program characters
	{"TRP",  00074, OP_EXPR, OP_TOP6, true},
// TRN=Transfer source numerics
	{"TRN",  00075, OP_EXPR, OP_TOP6, true},
// TRZ=Transfer source zones
	{"TRZ",  00076, OP_EXPR, OP_TOP6, true},
// TRS=Transfer source characters
	{"TRS",  00077, OP_EXPR, OP_TOP6, true},
// end of table
	{0, 0, OP_NONE, OP_NONE},
};

