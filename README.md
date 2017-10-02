# b5500emulator
A Burroughs B5500 emulation in "C"

This is absolute WORK IN PROGRESS!

Work starts in the "emulator" subdirectory, where I check some
possible implementation variants.

Focus is on using 64 bit "unsigned long long" for the B5500 words.

I try to use not more than shift, mask and addition/subtraction to
implement B5500 arithmetic operations.

The B5500 itself did not have gate level multiply or divide.

Currently I use GNU C, which is available for embedded CPUs as well.
For convienience all tests run under cygwin, but they should run on any
system where "unsigned long long" is 64 bits wide.

UPDATE 17-10-02:

The emulator sucessfully runs under
- cygwin
- linux for PC
- linux for ARM9

A typical execution:

# ./emulator/arm9/emulator.exe mta=tapes/B5500-XIII-SYSTEM-adc00257.bcd mtb=tapes/B5500-XIII-SYMBOL1-adc00255.bcd mtc=tapes/B5500-XIII-SYMBOL2-adc00253.bcd dka=disk/dka.dat dkb=disk/dkb.dat lpa=lc10=lp.txt cra=cards/algol1.card
B5500 Emulator Main Thread
magnetic tape option(s): mta=tapes/B5500-XIII-SYSTEM-adc00257.bcd
magnetic tape option(s): mtb=tapes/B5500-XIII-SYMBOL1-adc00255.bcd
magnetic tape option(s): mtc=tapes/B5500-XIII-SYMBOL2-adc00253.bcd
disk drive option(s): dka=disk/dka.dat
disk drive option(s): dkb=disk/dkb.dat
printer option(s): lpa=lc10=lp.txt
card reader option(s): cra=cards/algol1.card
spo option(s):
-H/L WITH MCP/DISK MARK XIII MODS RRRRRRRR-
 TIME IS 2310
 DATE IS TUESDAY, 6/18/85
 5:ALGOL/FIRST= 1 BOJ 2301
 PBD0055 OUT LINE:ALGOL/FIRST= 1
 0:PRNPBT/DISK= 2 BOJ 2301
 ALGOL/FIRST= 1 EOJ 2301
 5:FIRST/TRY= 1 BOJ 2301
 PBD0056 OUT LINE:FIRST/TRY= 1
 FIRST/TRY= 1 EOJ 2301
 PBD/0055001 REMOVED
 PRNPBT/DISK= 2 EOJ 2302
 0:PRNPBT/DISK= 1 BOJ 2302
 PBD/0056001 REMOVED
 PRNPBT/DISK= 1 EOJ 2302
OL MT
 MTA 00000 LABELED SYSTEM FILE000 001 71295 01
 MTB 00000 LABELED SYMBOL1 FILE000 001 71279 01
 MTC 00000 LABELED SYMBOL2 FILE000 001 71279 01

Explanation:
The three tapes SYSTEM, SYMBOL1, SYMBOL2 are mounted on MTA, MTB, MTC,
the card stack algol1.card is made ready in CRA,
the MCP reads and executes it,
the listings are written using control codes for the STAR* LC10 printer into
the file lp.txt

