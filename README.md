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
  
# BASIC1 compiler sub-projects description  
  
`b1c` - BASIC1 compiler, takes BASIC program source code and produces intermediate code (files with .b1c extension)  
`c1stm8` - intermediate code compiler, produces STM8 assembly language code  
`a1stm8` - STM8 assembler  
  