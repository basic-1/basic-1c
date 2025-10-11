# BASIC1 compiler for microcontrollers (now STM8 only)  
  
*just one more BASIC compiler*  
  
# Brief  
  
- supports classic BASIC language dialect with various extensions  
- command-line compiler built for Windows x86, Windows x64, Linux i386, Linux amd64, Linux armhf  
- produces a kind of intermediate code that can be compiled into STM8 assembler code  
- static and dynamic memory allocation (built-in simple dynamic memory manager)  
- some essential peripherals support (GPIO, timer, UART, SPI, displays)  
- interrupts handling  
- includes STM8 assembler  
- licensed under MIT license  
  
# Supported microcontrollers
  
Supported STM8 microcontrollers list:  
  
- STM8S001J3M3  
- STM8S003F3P6  
- STM8S003K3T6  
- STM8S103F3P6  
- STM8S103K3T6  
- STM8S903K3T6  
- STM8S105K4T6  
- STM8S105K6T6  
- STM8L050J3M3  
- STM8L151K4T6  
- STM8L151K6T6  
  
# Data types  
  
- STRING  
- INT (16-bit signed integer)  
- WORD (16-bit unsigned integer)  
- BYTE (8-bit unsigned integer)  
- LONG (32-bit signed integer)  
  
# Statements  
  
- BREAK  
- CONTINUE  
- DATA  
- DEF  
- DIM  
- ELSE  
- ELSEIF ... THEN  
- ERASE  
- FOR ... TO ... \[STEP\]  
- GET  
- GOTO  
- GOSUB  
- IF ... THEN  
- INPUT  
- IOCTL  
- LET  
- NEXT  
- OPTION BASE | EXPLICIT | NOCHECK | INPUTDEVICE | OUTPUTDEVICE  
- PRINT  
- PUT  
- READ  
- REM  
- RESTORE  
- RETURN  
- TRANSFER  
- WHILE ... WEND  
  
# More features  
  
- optional line numbers  
- optional LET statement  
- case-insensitive statement, variable and function names  
- one-, two- and three-dimensional arrays  
- relational operators can be used with strings  
- automatic numeric to string conversion  
- functions: ABS, INT, SGN, LEN, ASC, CHR$, STR$, VAL, IIF, IIF$, CBYTE, CINT, CWRD, CLNG  
- more functions: MID$, INSTR, LTRIM$, RTRIM$, LEFT$, RIGHT$, LSET$, RSET$, SET$, UCASE$, LCASE$  
  
# Program example  
  
```
REM blink with an LED connected to pin 5 of port B
10 IOCTL PB, CFGPIN5, OUT_PUSHPULL_FAST
20 WHILE 1 > 0
30 IOCTL PB, INVPIN5
40 GOSUB 1000
50 WEND
60 END

REM delay subroutine
1000 FOR I1 = 0 TO 9
1010 FOR I2 = -32768 TO 32767
1020 NEXT I2
1030 NEXT I1
1040 RETURN
```
  
# Building and usage  
  
## Building  
  
Use CMake 3.1 tools to build **b1c** BASIC to intermediate code compiler, **c1stm8** intermediate code compiler for STM8 and **a1stm8** STM8 assembler. Use one of the next C/C++ toolchains: MinGW-W64 (x86 and x64) under Windows, MSVC 2019 (x86 and x64) under Windows, gcc/g++ (x86, x64 and armhf) under Linux. Other compilers/toolchains are probably compatible too.  
  
First download source files copy to your local computer from [GitHub](https://github.com/basic-1/basic-1c)  
  
Commands to get the source files:  
`git clone https://github.com/basic-1/basic-1c`  
`cd basic-1c`  
`git submodule update --init`  
  
To build the compiler under Windows go to `build` subdirectories of every project `b1c`, `c1stmp` and `a1stm8` and run corresponding batch file depending on your compiler and target platform, e.g: `b1c_win_x64_mingw_rel.bat`, `c1stm8_win_x64_mingw_rel.bat`, `a1stm8_lnx_x64_gcc_rel.sh`, etc.  
  
**Important note:** The batch files mentioned above run corresponding `<platform_toolchain_name>_env` scripts from `./env` directory if they exist to set compiler-specific environment variables(PATH, INCLUDE, LIB, etc.). So the file names are: `win_x86_mingw_env.bat`, `win_x64_mingw_env.bat`, `lnx_x64_gcc_env.sh`, etc. Create them if necessary before building the project.  
  
## Usage  
  
BASIC1 compiler includes three executable modules: `b1c` - compiler translating BASIC programs to platform independent intermediate code, `c1stm8` - intermediate code compiler, producing STM8 assembly language code and `a1stm8` assembler. By default `b1c` calls intermediate code compiler automatically and `c1stm8` compiler calls `a1stm8` assembler. Run the executable modules without arguments to see available options.  
  
**Samples:**  
`b1c -d -s -m STM8S103F3 samples/blink.bsc` - compile `blink.bsc` program for STM8S103F3P6 MCU  
`b1c.exe -d -s -mu -m STM8S105K4 -o "E:\Temp\\" samples\heap.bsc` - compile `heap.bsc` program for STM8S105K4T6 MCU, put output files into "E:\Temp\\" directory  
  
# BASIC1 compiler projects description  
  
`b1c` - BASIC1 compiler, takes BASIC program source code and produces intermediate code (files with .b1c extension)  
`c1stm8` - intermediate code compiler, produces STM8 assembly language code  
`a1stm8` - STM8 assembler  
  
# More information  
  
[BASIC1 language reference](./b1c/docs/reference.md)  
[BASIC1 language limitations](./b1c/docs/limits.md)  
[IOCTL statement commands](./b1c/docs/ioctl.md)  
[BASIC1 program examples](./b1c/docs/samples)  
[STM8 assembler reference](./a1stm8/docs/reference.md)  
[Assembler program examples](./a1stm8/docs/samples)  
[Change log](./common/docs/changelog)  
[Download compiler distribution packages for Windows and Linux](https://github.com/basic-1/basic-1c/releases)  
