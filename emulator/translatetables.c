/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* translate tables BIC / ASCII / BCL
************************************************************************
* 2017-09-08    R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-09-30  R.Meyer
*   overhaul of file names
***********************************************************************/

#include <stdio.h>
#include "common.h"

// BIC (Burroughs Internal Code)
// @00: 0 1 2 3 4 5 6 7
// @10: 8 9 # @ ? : > }
// @20: + A B C D E F G
// @30: H I . [ & ( < ~
// @40: | J K L M N O P
// @50: Q R $ * - ) ; {
// @60:   / S T U V W X
// @70: Y Z , % ! = ] "

// There are 5 BIC Symbols that got no ASCII equivalent and are substituted as follows:
//  } is >= (greater or equal)
//  { is <= (less or equal)
//  ! is /= (not equal)
//  ~ is <- (left arrow)
//  | is x (multiply symbol)

const WORD6 translatetable_ascii2bic[128] = { // Index by 8-bit ASCII to get 6-bit BIC (upcased, invalid=>"?")
        0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,  // 00-0F
        0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,  // 10-1F
        0x30,0x3C,0x3F,0x0A,0x2A,0x3B,0x1C,0x0C,0x1D,0x2D,0x2B,0x10,0x3A,0x2C,0x1A,0x31,  // 20-2F
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0D,0x2E,0x1E,0x3D,0x0E,0x0C,  // 30-3F
        0x0B,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x21,0x22,0x23,0x24,0x25,0x26,  // 40-4F
        0x27,0x28,0x29,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x1B,0x0C,0x3E,0x0C,0x1F,  // 50-5F
        0x0C,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x21,0x22,0x23,0x24,0x25,0x26,  // 60-6F
        0x27,0x28,0x29,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x2F,0x20,0x0F,0x1F,0x0C}; // 70-7F

const WORD8 translatetable_bic2ascii[64] = { // Index by 6-bit BIC to get 8-bit ASCII
        0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,        // 00-07, @00-07
        0x38,0x39,0x23,0x40,0x3F,0x3A,0x3E,0x7D,        // 08-0F, @10-17
        0x2B,0x41,0x42,0x43,0x44,0x45,0x46,0x47,        // 10-17, @20-27
        0x48,0x49,0x2E,0x5B,0x26,0x28,0x3C,0x7E,        // 18-1F, @30-37
        0x7C,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,        // 20-27, @40-47
        0x51,0x52,0x24,0x2A,0x2D,0x29,0x3B,0x7B,        // 28-2F, @50-57
        0x20,0x2F,0x53,0x54,0x55,0x56,0x57,0x58,        // 30-37, @60-67
        0x59,0x5A,0x2C,0x25,0x21,0x3D,0x5D,0x22};       // 38-3F, @70-77

const WORD6 translatetable_bcl2bic[64]   = { // Index by 6-bit BCL to get 6-bit BIC
        014,001,002,003,004,005,006,007,        // @00-07
        010,011,000,012,013,015,016,017,        // @10-17
        060,061,062,063,064,065,066,067,        // @20-27
        070,071,074,072,073,075,076,077,        // @30-37
        054,041,042,043,044,045,046,047,        // @40-47
        050,051,040,052,053,055,056,057,        // @50-57
        034,021,022,023,024,025,026,027,        // @60-67
        030,031,020,032,033,035,036,037};       // @70-77

const char *translatetable_bic2baudot_as_ascii[64] = {
// Index by 6-bit BIC to get 8-bit BAUDOT with ASCII representation
// Note: ASCII # stands for BAUDOT "who are you?" symbol
// Note: ASCII ^ stands for BAUDOT "bell" symbol
        "0", "1", "2", "3", "4", "5", "6", "7",         // @00: 0 1 2 3 4 5 6 7
        "8", "9", "#", "^A", "?", ":", ")-", ")=",      // @10: 8 9 # @ ? : > >=
        "+", "A", "B", "C", "D", "E", "F", "G",         // @20: + A B C D E F G
        "H", "I", ".", "(.", "^U", "(", "-(", ":=",     // @30: H I . [ & ( < <-
        "//", "J", "K", "L", "M", "N", "O", "P",        // @40: | J K L M N O P
        "Q", "R", "^S", "^+", "-", ")", ".,", "=(",     // @50: Q R $ * - ) ; <=
        " ", "/", "S", "T", "U", "V", "W", "X",         // @60: _ / S T U V W X  (_ = blank)
        "Y", "Z", ",", "./.", "^I", "=", ".)", "''"};   // @70: Y Z , % ! = ] "

// index by BIC to get collation value
const WORD6 collation[64] = {
        53, 54, 55, 56, 57, 58, 59, 60,         // @00: 0 1 2 3 4 5 6 7
        61, 62, 19, 20, 63, 21, 22, 23,         // @10: 8 9 # @ ? : > }
        24, 25, 26, 27, 28, 29, 30, 31,         // @20: + A B C D E F G
        32, 33,  1,  2,  6,  3,  4,  5,         // @30: H I . [ & ( < ~
        34, 35, 36, 37, 38, 39, 40, 41,         // @40: | J K L M N O P
        42, 43,  7,  8, 12,  9, 10, 11,         // @50: Q R $ * - ) ; {
         0, 13, 45, 46, 47, 48, 49, 50,         // @60: _ / S T U V W X  (_ = blank)
        51, 52, 14, 15, 44, 16, 17, 18};        // @70: Y Z , % ! = ] "


