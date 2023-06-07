# STM8 ASSEMBLER  
  
*simple ASSEMBLER for STM8 microcontrollers*  
  
# Brief  
  
- simple syntax  
- supports all STM8 instructions  
- written in C++  
- can be built for Windows x86, Windows x64, Linux i386, Linux amd64, Linux armhf  
- Intel HEX output format (.ihx)  
- licensed under MIT license  
  
# Program sample  
  
`.CODE INIT`  
`:__RESET`  
`INT __START`  
`:__START`  
`LDW X, 0x7ff`  
`LDW SP, X`  

  
# Syntax  
  
Any program consists of sections. Assembler generates code for every section to be placed into specific memory area depending on section type. Every section begins with a special keyword and ends with the next section or the end of the file.  
  
## Section keywords  
`.DATA [PAGE0]` - declares a section for random access data (in RAM memory), assembler tries placing `.DATA PAGE0` sections in the first 256 bytes of RAM (to take advantage of `STM8` short addresses).  
`.STACK` - a section for stack, assembler does not initialize `SP` register, it just uses stack size when calculating total RAM used by program.  
`.CONST` - a section for read-only data, placed in first 32kB of flash memory.  
`.CODE [INIT]` - code, `.CODE INIT` section is placed in flash memory before `.CONST` sections, other `.CODE` sections go after read-only data.  
  
`.CODE INIT` section is placed in the beginning of the MCU flash memory (at `0x8000` address for `STM8` MCUs) so executing starts from it. Only one `.CODE INIT` section is allowed.  
Multiple `.STACK` sections are not combined into one, assembler uses the largest one to calculate total RAM memory usage.  
  
assembler statement = label | opcode [operands] | data definition statement  
  