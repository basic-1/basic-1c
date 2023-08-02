# STM8 ASSEMBLER  
  
*simple ASSEMBLER for STM8 microcontrollers*  
  
# Brief  
  
- simple syntax  
- supports all STM8 instructions  
- written in C++  
- can be built for Windows x86, Windows x64, Linux i386, Linux amd64, Linux armhf  
- Intel HEX output format (.ihx)  
- licensed under MIT license  
  
# The simplest program sample  
  
`.CODE INIT`  
`:__RESET`  
`INT __START`  
`:__START`  
`JRA __START`  
  
# Features  
  
- takes advantage of short addresses (more compact instructions)  
- conditional assembling  
- fixes relative address out-of-range errors, e.g. replaces `JRA _label` with `JP _label` or `JPF _label` (depending on memory model)  
  
# Usage  
  
Executable file name of the assembler is `a1stm8.exe` or `a1stm8` depending on target platform. Command line syntax:  
`a1stm8 [options] <filename> [<filename1> .. <filenameN>]`  
Here `<filename>` .. `<filenameN>` are names of source files. Possible options are listed below.  
  
## Command-line options  
  
`-d` or `/d` - prints error description  
`-f` or `/f` - fix out-of-range errors caused by relative addressing (replace relative addressing instructions with absolute addressing ones, e.g. `JRA` -> `JP` or `JPF`, `CALLR` -> `CALL` or `CALLF`)  
`-l` or `/l` - libraries directory, e.g.: `-l "../lib"`  
`-m` or `/m` - specifies MCU name, e.g.: `-m STM8S103F3`  
`-ml` or `/ml` - large memory model (selects extended addresses when used with `-f` option)  
`-ms` or `/ms` - small memory model (default, selects long addresses when used with `-f` option)  
`-mu` or `/mu` - prints memory usage  
`-o` or `/o` - specifies output file name, e.g.: `-o out.ihx`  
`-ram_size` or `/ram_size` - specifies RAM size, e.g.: `-ram_size 0x400`  
`-ram_start` or `/ram_start` - specifies RAM starting address, e.g.: `-ram_start 0`  
`-rom_size` or `/rom_size` - specifies ROM size, e.g.: `-rom_size 0x2000`  
`-rom_start` or `/rom_start` - specifies ROM starting address, e.g.: `-rom_start 0x8000`  
`-t` or `/t` - sets target (default STM8), e.g.: `-t STM8`  
`-v` or `/v` - shows assembler version and terminates  
  
**Samples:**  
`a1stm8.exe -d -mu first.asm`  
`a1stm8.exe -d -mu blink.asm`  
`a1stm8.exe -d -mu -m STM8S103F3 blink1.asm`  
  
# More documents  
  
[STM8 assembler reference](./docs/reference.md)  
[BASIC1 compiler for STM8](../README.md)  
[BASIC1 language reference](../b1c/docs/reference.md)  
[Change log](../common/docs/changelog)  
  