# b5500emulator
A Burroughs B5500 emulation in "C"

This is absolute WORK IN PROGRESS!

Work starts in the "experiments" subdirectory, where I check some
possible implementation variants.

Focus is on using 64 bit "unsigned long long" for the B5500 words.

I try to use not more than shift, mask and addition/subtraction to
implement B5500 arithmetic operations.

This is to allow the software to run on embedded SOCs like those with 32
Bit ARM cores or in FPGAs.

The B5500 did not have hardware multiply or divide.

Currently I use GNU C, which is available for embedded CPUs as well.
For convienience all tests run under cygwin, but they should run on any
system where "unsigned long long" is 64 bits wide.