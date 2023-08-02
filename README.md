# BASIC1 compiler for microcontrollers (now STM8 only)  
  
*just one more BASIC compiler*  
  
# Brief  
  
- supports classic BASIC language dialect with various extensions  
- command-line compiler built for Windows x86, Windows x64, Linux i386, Linux amd64, Linux armhf  
- produces a kind of intermediate code that can be compiled into STM8 assembler code  
- includes STM8 assembler  
- licensed under MIT license  
  
# Data types  
  
- STRING  
- INT (16-bit integer)  
- WORD (16-bit unsigned integer)  
- BYTE (8-bit unsigned integer)  
  
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
- GOTO  
- GOSUB  
- IF ... THEN  
- INPUT  
- IOCTL  
- LET  
- NEXT  
- OPTION BASE | EXPLICIT | NOCHECK  
- PRINT  
- READ  
- REM  
- RESTORE  
- RETURN  
- WHILE ... WEND  
  
# More features  
  
- optional line numbers  
- optional LET statement  
- case-insensitive statement, variable and function names  
- one-, two- and three-dimensional arrays  
- relational operators can be used with strings  
- automatic numeric to string conversion  
- functions: ABS, INT, SGN, LEN, ASC, CHR$, STR$, VAL, IIF, IIF$  
- more functions: MID$, INSTR, LTRIM$, RTRIM$, LEFT$, RIGHT$, LSET$, RSET$, UCASE$, LCASE$  

# Building and usage  
  
## Building  
  
Use CMake 3.1 tools to build **b1c** BASIC to intermediate code compiler, **c1stm8** intermediate code compiler for STM8 and **a1stm8** STM8 assembler. Use one of the next C/C++ toolchains: MinGW-W64 (x86 and x64) under Windows, MSVC 2019 (x86 and x64) under Windows, gcc/g++ (x86, x64 and armhf) under Linux. Other compilers/toolchains are probably compatible too.  
  
First download source files copy to your local computer from [GitHub](https://github.com/basic-1/basic-1c)  
  
Commands to get the source files:  
`git clone https://github.com/basic-1/basic-1�`  
`cd basic-1�`  
`git submodule update --init`  
  
To build the interpreter under Windows go to `build` subdirectories of every project `b1�`, `c1stmp` and `a1stm8` and run corresponding batch file depending on your compiler and target platform, e.g: `b1c_win_x64_mingw_rel.bat`, `c1stm8_win_x64_mingw_rel.bat`, `a1stm8_lnx_x64_gcc_rel.sh`, etc.  
  
**Important note:** The batch files mentioned above run corresponding `<platform_toolchain_name>_env` scripts from `./env` directory if they exist to set compiler-specific environment variables(PATH, INCLUDE, LIB, etc.). So the file names are: `win_x86_mingw_env.bat`, `win_x64_mingw_env.bat`, `lnx_x64_gcc_env.sh`, etc. Create them if necessary before building the project.  
  
## Usage  
  
  
# BASIC1 compiler projects description  
  
`b1c` - BASIC1 compiler, takes BASIC program source code and produces intermediate code (files with .b1c extension)  
`c1stm8` - intermediate code compiler, produces STM8 assembly language code  
`a1stm8` - STM8 assembler  
  