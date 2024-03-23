# STM8 assembler reference  
  
*simple ASSEMBLER for STM8 microcontrollers*  
  
# Introduction  
  
The assembler was initially created for BASIC1 language compiler for microcontrollers but can be used alone.  
  
# Syntax  
  
A program consists of statements located within special regions called sections. Assembler generates code for every section and places it into specific memory area depending on section type. Every section begins with a section keyword and ends with the next section or the end of the file.  
  
## Section keywords  
  
`.DATA [PAGE0]` - declares a section for random access data (in RAM memory), assembler tries placing `.DATA PAGE0` sections in the first 256 bytes of RAM (to take advantage of `STM8` short addressing).  
`.HEAP` - a section for heap memory.  
`.STACK` - a section for stack, assembler does not initialize `SP` register, it just uses stack size when calculating total RAM used by program.  
`.CONST` - a section for read-only data, placed in first 32kB of flash memory.  
`.CODE [INIT]` - code, `.CODE INIT` section is placed in flash memory before `.CONST` sections, other `.CODE` sections go after read-only data.  
  
`.CODE INIT` section is placed in the beginning of the MCU flash memory (at `0x8000` address for `STM8` MCUs) so executing starts from it. Only one `.CODE INIT` section is allowed.  
Multiple `.STACK` sections are not combined into one, assembler uses the largest one to calculate total RAM memory usage. `.HEAP` sections are not aggregated too: the largest section size is used.  
  
### Sections layout in memory  
  
`.DATA PAGE0` addresses range 0..0xFF  
`.DATA` addresses range 0x100..0x7FFF  
`.HEAP` addresses range 0..0x7FFF  
`.STACK` addresses range 0..0x7FFF  
`.CODE INIT` addresses range 0x8000..0xFFFF  
`.CONST` addresses range 0x8000..0xFFFF  
`.CODE` addresses range 0x8000..0x27FFF  
  
### Predefined symbolic constants computed at assembly time  
  
`__DATA_START` - `.DATA` sections starting address  
`__DATA_SIZE` - overall size of all `.DATA` sections  
`__HEAP_START` - heap memory section starting address  
`__HEAP_SIZE` - heap memory area size  
`__STACK_START` - stack starting address  
`__STACK_SIZE` - stack size  
`__DATA_TOTAL_SIZE` - size of all RAM sections (`__DATA_SIZE` + `__HEAP_SIZE` + `__STACK_SIZE`)  
`__INIT_START` - `.CODE INIT` section starting address  
`__INIT_SIZE` - `.CODE INIT ` section size  
`__CONST_START` - read-only data (`.CONST`) sections starting address  
`__CONST_SIZE` - read-only data sections size  
`__CODE_START` - `.CODE` sections starting address  
`__CODE_SIZE` - code sections size  
`__CODE_TOTAL_SIZE` - total ROM sections size (`__INIT_SIZE` + `__CONST_SIZE` + `__CODE_SIZE`)  
`__RET_ADDR_SIZE` - return address size for the current memory model (2 or 3)  
  
## Comments  
  
Comment is a text used to describe a piece of code. Comments are ignored by assembler. Use semicolon character to start new single line comment (the rest of the string is ignored).  
**Syntax:** `;<comment text>`  
**Examples:**  
`; a comment`  
`.DATA ; this is a .DATA section`  
  
## Numeric constants  
  
Numeric constants are numbers written with one or more digits. Decimal and hexadecimal numbers are supported by this assembler implementation. Hexadecimal numbers have to start from `0x` prefix. Use unary minus sign to specify negative values.  
**Examples:**  
`100`  
`-1000`  
`0xFF`  
`0xAA00`  
`0x8000`  
  
### Extracting bytes and words from constants  
  
To get a specific byte or word from a constant special suffixes should be added at the end of the constant.  
Suffixes to extract words:  
`.l` - get low word  
`.h` - get high word  
Suffixes to extract bytes:  
`.ll` - low byte of low word  
`.lh` - high byte of low word  
`.hl` - low byte of high word  
`.hh` - high byte of high word  
**Examples:**  
`0x27FFF.hl` - `2`  
`0xFFAA.lh` - `0xFF`  
`0xFFAA.ll` - `0xAA`  
`0xFFAA.hl` - `0`  
`1000.lh` - `3`  
`__CONST_START.ll` - the least significant byte of `__CONST_START` constant  
  
## Statements  
  
Statement is a label, data definition directive or CPU instruction. Every statement is terminated with new-line sequence, comment or the end of file.  
  
### Labels  
  
Labels are used to create symbolic constants to refer to specific code or data location (assembler resolves the constants to memory addresses). A label starts with a colon character.  
**Syntax:**: `:<label_name>`  
**Examples:**  
`:label1` - a label named `label1`  
`::another_label` - a label named `:another_label`  
  
### Data definition directives  
  
Data definition directives are used to allocate memory and initialize it with some data.  
**Syntax:** `<data_definition_directive> [(<size>)] [<init_value0>[,<init_value1>..<init_valueN>]]`  
The assembler supports three data definition directives: `DB` to declare 1-byte data, `DW` to declare two-byte (word) data and `DD` to declare four-byte (double word) data.  
`<size>` should be a numeric constant to specify number of bytes or words to allocate. `<init_value0>, ..<init_valueN>` clause should be used to specify initial values for the memory area. Initial values are not allowed for RAM sections (`.DATA`, `.HEAP` and `.STACK`). Non-initialized data in `.CONST` and `.CODE` sections is initialized with zeroes.  
**Examples:**  
`.DATA`  
`:word_var`  
`DW` - declare a `.DATA` section with a single word variable that can be referenced by `word_var` label  
`.HEAP`  
`DB (512)` - declare 512 bytes long heap area  
`.CONST`  
`DB (10)` - reserve 10 bytes initialized with zeroes in `.CONST` section  
`DB (10) "DATA"` - reserve another 10 bytes block, first 4 bytes are initialized with "DATA" text and the rest with zeroes  
`DW 0xAAAA, 0xBBBB, 0xCCCC` - three words (6 bytes) initialized with 0xAAAA, 0xBBBB and 0xCCCC  
`DD -1` - a double word  
  
### CPU instructions  
  
Refer to STMicroelectronics' "PM0044 Programming Manual" for CPU instructions syntax. The only difference is immediate and direct addressing modes: the assembler requires for parentheses around address constants and no hash character (#) is needed to indicate immediate values. Simple expressions (without parentheses) can be used instead of address constants and immediate values. Supported operators: `-` (unary minus), `!` (unary bitwise NOT), `*`, `/`, `%` (remainder operator), `+`, `-`, `>>` (bitwise right shift), `<<`  (bitwise left shift), `&` (bitwise AND), `^` (bitwise XOR), `|` (bitwise OR).  
  
#### Operator precedence order:  
  
- `-` (unary minus), `!` (unary bitwise NOT) - highest precedence  
- `*`, `/`, `%` (remainder operator)  
- `+`, `-`  
- `>>` (bitwise right shift), `<<` (bitwise left shift)  
- `&` (bitwise AND)  
- `^` (bitwise XOR)  
- `|` (bitwise OR)  - lowest precedence
  
**Examples:**  
`LD A, #5` -> `LD A, 5` - do not use hash character for immediate value  
`LD A, 5` -> `LD A, (5)` - use parentheses for direct addressing  
`LDW X, $5000` -> `LDW X, (0x5000)` - use parentheses for direct addressing  
`LDW X, __HEAP_START` - load immediate value into X  
`LDW X, (__HEAP_START)` - not the same as previous: load value located at `__HEAP_START` address  
`LDW X, __HEAP_START + __HEAP_SIZE - 1` - load immediate value  
`LDW X, (__HEAP_START + __HEAP_SIZE - 1)` - load value located at `__HEAP_START + __HEAP_SIZE - 1` address  
  
## Conditional assembly directives  
  
Conditional assembly directives provides a way excluding specific code portions from assembling process. There are four directives: `.IF`, `.ELIF`, `.ELSE` and `.ENDIF`. `.ELIF` and `.ELSE` clauses are optional, multiple `.ELIF` directives are allowed.  
  
**Syntax:**  
`.IF <condition>`  
`<source_lines>`  
`.ELIF <condition1>`  
`<source_lines1>`  
`...`  
`.ELSE`  
`<source_linesN>`  
`.ENDIF`  
  
Assembler keeps source code lines between the first directive with true condition and the next one. If all condition expressions are false the source code lines between `.ELSE` and `.ENDIF` directives are selected (if `.ELSE` clause exists).  
  
The next comparison operators can be used in the condition expressions:  
- `==` - equality check  
- `!=` - inequality check  
- `>` - greater than check  
- `<` - less than check  
- `>=` - greater than or equal check  
- `<=` - less than equal check  
  
**Examples:**  
`.IF __RET_ADDR_SIZE == 2`  
`RET`  
`.ELSE`  
`RETF`  
`.ENDIF`  
  
Expressions on the left and right sides of the comparison operators can be simple expressions (without parentheses) described [here](#CPU-instructions). Another type of condition is `DEFINED()` function, which returns true if its argument is an existing symbolic constant. The function can be used with `NOT` operator to negate its result.  
  
**Examples:**  
`.IF NOT DEFINED(_SIZE)`  
`.DEF _SIZE 128`  
`.ENDIF`  
`DB (_SIZE)`  
  
## .ERROR directive  
  
`.ERROR` directive emits a specified error message and stops assempling.  
  
**Syntax:**  
`.ERROR <double_quoted_error_message>`  
**Examples:**  
`.IF __HEAP_SIZE < 2`  
`.ERROR "insufficient heap size"`  
`.ENDIF`  
  
# Usage  
  
Executable file name of the assempler is `a1stm8.exe` or `a1stm8` depending on target platform. Command line syntax:  
`a1stm8 [options] <filename> [<filename1> .. <filenameN>]`  
Here `<filename>` .. `<filenameN>` are names of source files. Possible options are listed below.  
  
## .DEF directive  
  
`.DEF` directive creates new symbolic constant and assigns it a numeric value. A constant with omitted expression part is assigned zero value. After definition the constant can be used in expressions.  
  
**Syntax:**  
`DEF <constant_name> [<expression>]`  
**Examples:**  
`.DEF PORTA_BASE 0x5000`  
`.DEF PORTA_DDR PORTA_BASE + 2`  
...  
...  
...  
`.IF DEFINED(PORTA_DDR)`  
`MOV (PORTA_DDR), 0`  
`.ELSE`  
`.ERROR "PORTA_DDR is not defined"`  
`.ENDIF`  
  
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
  
**Examples:**  
`a1stm8.exe -d -mu first.asm`  
`a1stm8.exe -d -mu blink.asm`  
`a1stm8.exe -d -mu -m STM8S103F3 blink1.asm`  
  
### Fixing relative address out of range errors  
  
STM8 instruction set includes some relative addressing instructions such as `CALLR`, `JRA`, `JREQ`, `BTJF`, etc. These instructions can modify PC register by adding signed 8-bit offset to its current value. The offset is calculated as a difference between the referenced instruction address and the current instruction address. If the instructions are too far from each other, the offset can exceed 8-bit value: "relative address out of range" error is reported.  
E.g.:  
`CP A, 10`  
`JREQ __LBL_EQUAL` - a relative addressing instruction  
...  
...  
...  
`:__LBL_EQUAL`  
`CLR A` - this instruction is referenced by the `JREQ` instruction above  
  
Specify `-f` command-line option for the assembler to fix relative addressing out-of-range errors automatically: the problem instructions are replaced with corresponding direct addressing ones. Conditional jumps are replaced with several instructions (there're no direct addressing conditional jumps in STM8 instruction set).  
E.g.:  
`JREQ __LBL_EQUAL`  
->  
`JRNE __TMP_NOT_EQUAL`  
`JP __LBL_EQUAL`  
`:__TMP_NOT_EQUAL`  
  