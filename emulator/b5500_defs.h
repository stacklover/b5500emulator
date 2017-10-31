/* b5500_defs.h: Burroughs 5500 simulator definitions

   Copyright (c) 2016, Richard Cornwell

   Copyright (c) 2017, Reinhard Meyer (for the adaption to my B5500 project)

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   RICHARD CORNWELL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


#ifndef _B5500_H_
#define _B5500_H_

#include <stdio.h>
#include "common.h"
#undef PRESENT
typedef unsigned long long t_uint64;
typedef unsigned long uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef int t_stat;

/* Word mode opcodes */
#define WMOP_LITC               00000   /* Load literal */
#define WMOP_OPDC               00002   /* Load operand */
#define WMOP_DESC               00003   /* Load Descriptor */
#define WMOP_OPR                00001   /* Operator */
#define WMOP_DEL                00065   /* Delete top of stack */
#define WMOP_NOP                00055   /* Nop operation */
#define WMOP_XRT                00061   /* Set Variant */
#define WMOP_ADD                00101   /* Add */
#define WMOP_DLA                00105   /* Double Precision Add */
#define WMOP_PRL                00111   /* Program Release */
#define WMOP_LNG                00115   /* Logical Negate */
#define WMOP_CID                00121   /* Conditional Integer Store Destructive */
#define WMOP_GEQ                00125   /* WMOP_B greater than or equal to A */
#define WMOP_BBC                00131   /* Branch Backward Conditional */
#define WMOP_BRT                00135   /* Branch Return */
#define WMOP_INX                00141   /* Index */
#define WMOP_ITI                00211   /* Interrogate interrupt */
#define WMOP_LOR                00215   /* Logical Or */
#define WMOP_CIN                00221   /* Conditional Integer Store non-destructive */
#define WMOP_GTR                00225   /* B Greater than A */
#define WMOP_BFC                00231   /* Branch Forward Conditional */
#define WMOP_RTN                00235   /* Return normal */
#define WMOP_COC                00241   /* Construct Operand Call */
#define WMOP_SUB                00301   /* Subtract */
#define WMOP_DLS                00305   /* WMOP_Double Precision Subtract */
#define WMOP_MUL                00401   /* Multiply */
#define WMOP_DLM                00405   /* Double Precision Multiply */
#define WMOP_RTR                00411   /* Read Timer */
#define WMOP_LND                00415   /* Logical And */
#define WMOP_STD                00421   /* B Store Destructive */
#define WMOP_NEQ                00425   /* B Not equal to A */
#define WMOP_SSN                00431   /* Set Sign Bit */
#define WMOP_XIT                00435   /* Exit */
#define WMOP_MKS                00441   /* Mark Stack */
#define WMOP_DIV                01001   /* Divide */
#define WMOP_DLD                01005   /* Double Precision Divide */
#define WMOP_COM                01011   /* Communication operator */
#define WMOP_LQV                01015   /* Logical Equivalence */
#define WMOP_SND                01021   /* B Store Non-destructive */
#define WMOP_XCH                01025   /* Exchange */
#define WMOP_CHS                01031   /* Change sign bit */
#define WMOP_RTS                01235   /* Return Special */
#define WMOP_CDC                01241   /* Construct descriptor call */
#define WMOP_FTC                01425   /* Transfer F Field to Core Field */
#define WMOP_MOP                02015   /* Reset Flag bit */
#define WMOP_LOD                02021   /* Load */
#define WMOP_DUP                02025   /* Duplicate */
#define WMOP_TOP                02031   /* Test Flag Bit */
#define WMOP_IOR                02111   /* I/O Release */
#define WMOP_LBC                02131   /* Word Branch Backward Conditional */
#define WMOP_SSF                02141   /* Set or Store S or F registers */
#define WMOP_HP2                02211   /* Halt P2 */
#define WMOP_LFC                02231   /* Word Branch Forward Conditional */
#define WMOP_ZP1                02411   /* Conditional Halt */
#define WMOP_TUS                02431   /* Interrogate Peripheral Status */
#define WMOP_LLL                02541   /* Link List Look-up */
#define WMOP_IDV                03001   /* Integer Divide Integer */
#define WMOP_SFI                03011   /* Store for Interrupt */
#define WMOP_SFT                03411   /* Store for Test */
#define WMOP_FTF                03425   /* Transfer F Field to F Field */
#define WMOP_MDS                04015   /* Set Flag Bit */
#define WMOP_IP1                04111   /* Initiate P1 */
#define WMOP_ISD                04121   /* Interger Store Destructive */
#define WMOP_LEQ                04125   /* B Less Than or Equal to A */
#define WMOP_BBW                04131   /* Banch Backward Conditional */
#define WMOP_IP2                04211   /* Initiate P2 */
#define WMOP_ISN                04221   /* Integer Store Non-Destructive */
#define WMOP_LSS                04225   /* B Less Than A */
#define WMOP_BFW                04231   /* Branch Forward Unconditional */
#define WMOP_IIO                04411   /* Initiate I/O */
#define WMOP_EQL                04425   /* B Equal A */
#define WMOP_SSP                04431   /* Reset Sign Bit */
#define WMOP_CMN                04441   /* Enter Character Mode In Line */
#define WMOP_IFT                05111   /* Test Initiate */
#define WMOP_CTC                05425   /* Transfer Core Field to Core Field */
#define WMOP_LBU                06131   /* Word Branch Backward Unconditional */
#define WMOP_LFU                06231   /* Word Branch Forward Unconditional */
#define WMOP_TIO                06431   /* Interrogate I/O Channels */
#define WMOP_RDV                07001   /* Remainder Divide */
#define WMOP_FBS                07031   /* Flag Bit Search */
#define WMOP_CTF                07425   /* Transfer Core Field to F Field */
#define WMOP_ISO                00045   /* Variable Field Isolate XX */
#define WMOP_CBD                00351   /* Non-Zero Field Branch Backward Destructive Xy */
#define WMOP_CBN                00151   /* Non-Zero Field Branch Backward Non-Destructive Xy */
#define WMOP_CFD                00251   /* Non-Zero Field Branch Forward Destructive Xy */
#define WMOP_CFN                00051   /* Non-Zero Field Branch Forward Non-Destructive Xy */
#define WMOP_DIA                00055   /* Dial A XX */
#define WMOP_DIB                00061   /* Dial B XX Upper 6 not Zero */
#define WMOP_TRB                00065   /* Transfer Bits XX */
#define WMOP_FCL                00071   /* Compare Field Low XX */
#define WMOP_FCE                00075   /* Compare Field Equal XX */

/* Character Mode */
#define CMOP_EXC                00000   /* CMOP_Exit Character Mode */
#define CMOP_CMX                00100   /* Exit Character Mode In Line */
#define CMOP_BSD                00002   /* Skip Bit Destiniation */
#define CMOP_BSS                00003   /* SKip Bit Source */
#define CMOP_RDA                00004   /* Recall Destination Address */
#define CMOP_TRW                00005   /* Transfer Words */
#define CMOP_SED                00006   /* Set Destination Address */
#define CMOP_TDA                00007   /* Transfer Destination Address */
#define CMOP_TBN                00012   /* Transfer Blanks for Non-Numerics */
#define CMOP_SDA                00014   /* Store Destination Address */
#define CMOP_SSA                00015   /* Store Source Address */
#define CMOP_SFD                00016   /* Skip Forward Destination */
#define CMOP_SRD                00017   /* Skip Reverse Destination */
#define CMOP_SES                00022   /* Set Source Address */
#define CMOP_TEQ                00024   /* Test for Equal */
#define CMOP_TNE                00025   /* Test for Not-Equal */
#define CMOP_TEG                00026   /* Test for Greater Or Equal */
#define CMOP_TGR                00027   /* Test For Greater */
#define CMOP_SRS                00030   /* Skip Reverse Source */
#define CMOP_SFS                00031   /* Skip Forward Source */
#define CMOP_TEL                00034   /* Test For Equal or Less */
#define CMOP_TLS                00035   /* Test For Less */
#define CMOP_TAN                00036   /* Test for Alphanumeric */
#define CMOP_BIT                00037   /* Test Bit */
#define CMOP_INC                00040   /* Increase Tally */
#define CMOP_STC                00041   /* Store Tally */
#define CMOP_SEC                00042   /* Set Tally */
#define CMOP_CRF                00043   /* Call repeat Field */
#define CMOP_JNC                00044   /* Jump Out Of Loop Conditional */
#define CMOP_JFC                00045   /* Jump Forward Conditional */
#define CMOP_JNS                00046   /* Jump out of loop unconditional */
#define CMOP_JFW                00047   /* Jump Forward Unconditional */
#define CMOP_RCA                00050   /* Recall Control Address */
#define CMOP_ENS                00051   /* End Loop */
#define CMOP_BNS                00052   /* Begin Loop */
#define CMOP_RSA                00053   /* Recall Source Address */
#define CMOP_SCA                00054   /* Store Control Address */
#define CMOP_JRC                00055   /* Jump Reverse Conditional */
#define CMOP_TSA                00056   /* Transfer Source Address */
#define CMOP_JRV                00057   /* Jump Reverse Unconditional */
#define CMOP_CEQ                00060   /* Compare Equal */
#define CMOP_CNE                00061   /* COmpare for Not Equal */
#define CMOP_CEG                00062   /* Compare For Greater Or Equal */
#define CMOP_CGR                00063   /* Compare For Greater */
#define CMOP_BIS                00064   /* Set Bit */
#define CMOP_BIR                00065   /* Reet Bit */
#define CMOP_OCV                00066   /* Output Convert */
#define CMOP_ICV                00067   /* Input Convert */
#define CMOP_CEL                00070   /* Compare For Equal or Less */
#define CMOP_CLS                00071   /* Compare for Less */
#define CMOP_FSU                00072   /* Field Subtract */
#define CMOP_FAD                00073   /* Field Add */
#define CMOP_TRP                00074   /* Transfer Program Characters */
#define CMOP_TRN                00075   /* Transfer Numeric */
#define CMOP_TRZ                00076   /* Transfer Zones */
#define CMOP_TRS                00077   /* Transfer Source Characters */

/* Masks */
#define FLAG            04000000000000000LL     /* Operand Flag */
#define FWORD           03777777777777777LL     /* Full word mask */
#define MSIGN           02000000000000000LL     /* Operator Word */
#define ESIGN           01000000000000000LL
#define EXPO            00770000000000000LL
#define EXPO_V          39
#define MANT            00007777777777777LL
#define NORM            00007000000000000LL
#define ROUND           00004000000000000LL
#define PRESENT         01000000000000000LL     /* Oprand Type */
#define DFLAG           02000000000000000LL     /* Descriptor */
#define WCOUNT          00017770000000000LL
#define WCOUNT_V        30
#define INTEGR          00000002000000000LL
#define CONTIN          00000001000000000LL
#define CORE            00000000000077777LL
#define RFIELD          00077700000000000LL     /* Mark Stack Control Word */
#define RFIELD_V        27                      /* Shift off by 6 bits */
#define SMSFF           00000020000000000LL
#define SSALF           00000010000000000LL
#define SVARF           00000000100000000LL
#define SCWMF           00000000000100000LL
#define FFIELD          00000007777700000LL
#define FFIELD_V        15
#define REPFLD          00000770000000000LL
#define REPFLD_V        30
#define MODEF           00200000000000000LL     /* Program Descriptor +FFIELD and CORE */
#define ARGF            00100000000000000LL
#define PROGF           00400000000000000LL
#define RGH             00340700000000000LL     /* Return Control Word +FFIELD and CORE */
#define RGH_V           33
#define RKV             00034070000000000LL
#define RKV_V           30
#define RL              00003000000000000LL     /* Save L register */
#define RL_V            36
#define LMASK           00000000007777777LL
#define HMASK           00007777770000000LL

#endif /* _B5500_H_ */
