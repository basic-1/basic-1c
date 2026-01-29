# RISC-V RV32 assembler reference  
  
*simple ASSEMBLER for RISC-V microcontrollers*  
  
# Introduction  
  
The assembler was initially created for BASIC1 language compiler for microcontrollers but can be used alone. The assembler supports `RV32I` and `RV32E` instruction sets with `C`, `M`, `Zmmul` and `Zicsr` extensions.  
  
# Syntax  
  
A program consists of statements located within special regions called sections. Assembler generates code for every section and places it into specific memory area depending on section type. Every section begins with a section keyword and ends with the next section or the end of the file.  
  
## Section keywords  
  
`.DATA` - declares a section for random access data (in RAM memory).  
`.HEAP` - a section for heap memory.  
`.STACK` - a section for stack, assembler does not initialize `SP` register, it just uses stack size when calculating total RAM used by program.  
`.CONST` - a section for read-only data, placed in flash memory between `CODE INIT` and `CODE` sections.  
`.CODE [INIT]` - code, `.CODE INIT` section is placed in flash memory before `.CONST` sections, other `.CODE` sections go after read-only data.  
  
`.CODE INIT` section is placed in the beginning of the MCU flash memory. The section can be used to place interrupt vector table and other initialization code at the beginning of the flash memory. Only one `.CODE INIT` section is allowed.  
Multiple `.STACK` sections are not combined into one, assembler uses the largest one to calculate total RAM memory usage. `.HEAP` sections are not aggregated too: the largest section size is used.  
  
### Sections layout in memory  
  
`[RAM memory start]`  
`.DATA` sections  
`.HEAP` section  
`.STACK` section  
`[RAM memory end]`  
  
`[FLASH memory start]`  
`.CODE INIT` section  
`.CONST` sections  
`.CODE` sections  
`[FLASH memory end]`  
  
### Predefined symbolic constants  
  
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
`__TARGET_NAME` - a string constant, assembler's target architecture name (e.g. `RV32`)  
`__MCU_NAME` - a string constant, MCU name specified in the command line (e.g. `CH32V003F4`)  
  
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
  
### Extracting bytes and words from numeric constants  
  
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
  
### Extracting RISC-V specific parts of numeric values  
  
RISC-V instructions cannot encode full 32-bit integer constants on their own. Every constant should be split in two parts instead: 20-bit upper immediate part and and the lower 12-bit signed value. The values can be extracted with `.h20` and `.l12` suffixes respectively.  
**Examples:**  
`2047.h20` - `0`  
`2047.l12` - `0x7FF` or `2047`  
`2048.h20` - `1`  
`2048.l12` - `0xFFFFF800` or `-2048`  
`-2048.h20` - `0`  
`-2048.l12` - `0xFFFFF800` or `-2048`  
`-2049.h20` - `0xFFFFFFFF`  
`-2049.l12` - `0x7FF` or `2047`  
`LUI A0, 123456789.H20`  
`ADDI A0, A0, 123456789.L12` - this `LUI` + `ADDI` instructions pair loads `123456789` integer value into `A0` register  
  
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
`DD (5) 1, 2, 3` - five double words (20 bytes) initialized with 1, 2, 3, 0, 0 values  
  
### CPU instructions  
  
Refer to "The RISC-V Instruction Set Manual Volume I: Unprivileged ISA" document for CPU instructions syntax. The manual can be found on the [RISC-V International](https://riscv.org/) site. Simple expressions (without parentheses) can be used instead of immediate values. Supported operators: `-` (unary minus), `!` (unary bitwise NOT), `*`, `/`, `%` (remainder operator), `+`, `-`, `>>` (bitwise right shift), `<<`  (bitwise left shift), `&` (bitwise AND), `^` (bitwise XOR), `|` (bitwise OR).  
  
#### Operator precedence order:  
  
- `-` (unary minus), `!` (unary bitwise NOT) - highest precedence  
- `*`, `/`, `%` (remainder operator)  
- `+`, `-`  
- `>>` (bitwise right shift), `<<` (bitwise left shift)  
- `&` (bitwise AND)  
- `^` (bitwise XOR)  
- `|` (bitwise OR)  - lowest precedence
  
**Examples:**  
`LUI A0, __HEAP_SIZE.H20`  
`ADDI A0, A0, __HEAP_SIZE.L12`  
`ADDI A0, A0, -4`  
`LUI A1, __HEAP_START.H20`  
`SW A0, __HEAP_START.L12(A1)`
  
### Pseudo-instructions  
  
A pseudo-instruction is a special assembler command that does not correspond to distinct RISC-V opcode: assembler produces another command's opcode or several opcodes instead. Pseudo-instructions are intended to simplify writing assembly code.  
  
#### Pseudo-instructions list  
  
`MV rd, rs` expands to `ADDI rd, rs, 0`  
  
`NOP` expands to either `C.NOP` or `ADDI ZERO, ZERO, 0`  
  
`NOT rd, rs` corresponds to `XORI rd, rs, -1`  
  
`NOT rs/rd` corresponds to `XORI rs/rd, rs/rd, -1`  
  
`NEG rd, rs` - `SUB rd, ZERO, rs`  
  
`NEG rs/rd` - `SUB rs/rd, ZERO, rs/rd`  
  
`J <label>` - `C.J <label>` or `JAL ZERO, <label>`  
  
`CALL <label>` - `C.JAL <label>`, `JAL RA, <label>` or `LUI RA, <label>.H20` + `JALR RA, <label>.L12(RA)`  
  
`RET` - `C.JR RA` or `JALR ZERO, 0(RA)`  
  
`LA rd, <label>` and `LI rd, <value>` pseudo-instructions can be used to load addresses of symbolic labels and immediate numeric values into registers. At the moment these two instructions produce the same code because the assembler does not support options for position-independent code (PIC) generation and/or Global Offset Tables (GOT) natively.  
Depending on value and register `LA` and `LI` pseudo-instructions can be represented as:  
`C.LI rd <value>`  
`ADDI rd, ZERO, <value>`  
`LUI rd, <value>.H20`  
`LUI rd, <value>.H20 + ADDI rd, rd, <value>.L12`  
`LUI rd, <value>.H20 + C.ADDI rd, <value>.L12`, etc.  
  
`LB/LBU/LH/LHU/LW rd, <address>` expands to `LUI rd, <address>.H20` + `LB/LBU/LH/LHU/LW rd, <address>.L12(rd)`  
  
`LB/LBU/LH/LHU/LW rd, <offset>(rs)` expands to `LUI rd, <offset>.H20` + `ADD rd, rd, rs` + `LB/LBU/LH/LHU/LW rd, <offset>.L12(rd)` or `LUI rd, <offset>.H20` + `C.ADD rd, rs` + `LB/LBU/LH/LHU/LW rd, <offset>.L12(rd)` or `LUI T0, <offset>.H20` + `ADD r, r, T0` + `LB/LBU/LH/LHU/LW r, <offset>.L12(r)`, etc. Note that when rd and rs are the same register these pseudo-instructions use T0 register for full 32-bit address calculation so they should be used very carefully.  
  
`SB/SH/SW rs, <address>` expands to `LUI T0, <address>.H20 + SB/SH/SW rs, <address>.L12(T0)`, T0 register is used for address calculation.  
  
`SB/SH/SW rs1, <offset>(rs2)` expands to `LUI T0, <offset>.H20 + ADD T0, T0, rs2 + SB/SH/SW rs1, <offset>.L12(T0)` or `LUI T0, <offset>.H20 + C.ADD T0, rs2 + SB/SH/SW rs1, <offset>.L12(T0)`, T0 register is used for address calculation.  
  
`ADDI rd, rs, <value>` expands to `C.ADDI r, <value>`, `LUI rd, <value>.H20 + ADDI rd, rd, <value>.L12 + ADD rd, rd, rs`, `LUI rd, <value>.H20 + ADDI rd, rd, <value>.L12 + C.ADD rd, rs`, `LUI T0, <value>.H20 + ADDI r, r, <value>.L12 + ADD r, r, T0`, etc. When rd and rs are the same register T0 register is used to store upper 20-bits of the immediate value.  
`ORI/XORI/ANDI rd, rs, <value>` expands to `LUI rd, <value>.H20 + ADDI rd, rd, <value>.L12 + OR/XOR/AND rd, rd, rs`, `LUI rd, <value>.H20 + ADDI rd, rd, <value>.L12 + C.OR/C.XOR/C.AND rd, rs`, `LUI T0, <value>.H20 + ADDI T0, T0, <value>.L12 + OR/XOR/AND r, r, T0`, etc. When rd and rs are the same register T0 register is used to load full 32-bit immediate value.  
  
**Examples:**  
`LI SP, __DATA_START + __DATA_TOTAL_SIZE`  
`LI A0, __HEAP_SIZE - 4`  
`SW A0, __HEAP_START`  
  
`-nci` command-line option can be used to forbid compressed instructions usage.  
  
By default a1rv32 assembler does not allow pseudo-instructions with 32-bit immediate values (including those which use T0 register internally). To enable them specify `-f` command-line option.  
  
#### I32. instruction prefix  
  
Specifying instruction's mnemonic with `I32.` prefix makes assembler to select a single 32-bit (4-byte) instruction even if a compressed instruction is suitable for the mnemonic.  
  
**Examples:**  
`I32.J __START` ; `J` instruction should be used, not `C.J`  
`I32.ADDI A0, A0, 1` ; `ADDI` instruction, not `C.ADDI` or any of various `ADDI` pseudo-instructions  
  
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
  
Non-numeric values can be compared using `==` and `!=` operators only. Non-numeric immediate values can be enclosed in double quotes. Use two double quote characters to escape a double quote character inside a string.  
The next special functions can be used with non-numeric values:  
- `SUBSTR` function extracts a substring from a non-numeric value  
- `FIND` function searches for the first substring appearance in the source string (regular expressions can be used)  
  
`SUBSTR` function syntax: `SUBSTR(<value>, <start>, <count>)`, starting position is zero-based. If starting position is omitted, zero value is used, if `<count>` argument is omitted - `SUBSTR` function copies characters to the end of the string.  
`FIND` function syntax: `FIND(<source_string>, <substring> | <regexp>)`, supports ECMAScript regexp grammar. Returns zero-based substring position if the string or its part matches the regexp or -1 otherwise.  
  
**Examples:**  
`.IF __DATA_SIZE == 0`  
`.ELIF __DATA_SIZE <= 4`  
`SW ZERO, __DATA_START`  
`.ELIF __DATA_SIZE <= 8`  
`LA A0, __DATA_START`  
`LI A1, 0`  
`SW A1, 0(A0)`  
`SW A1, 4(A0)`  
`...`  
`.ENDIF`  
  
`.IF SUBSTR(__MCU_NAME,0,8) == CH32V003`  
`; CH32V003 MCU`  
`.ELSE`  
`...`  
`.ENDIF`  
  
`.IF FIND(__EXTENSIONS, "Zmmul") == 0`  
`; no Zmmul extension`  
`.ENDIF`  
  
Expressions on the left and right sides of the comparison operators can be simple expressions (without parentheses) described [here](#CPU-instructions). Another type of condition is `DEFINED()` function, which returns true if its argument is an existing symbolic constant. The function can be used with `NOT` operator to negate its result.  
  
**Examples:**  
`.IF NOT DEFINED(_SIZE)`  
`.DEF _SIZE 128`  
`.ENDIF`  
`DB (_SIZE)`  
  
## .ALIGN directive  
  
`.ALIGN` directive aligns the next data element or instruction address on a value that is a multiple of the directive's parameter.  
  
**Syntax:**  
`.ALIGN <align_on_bytes>`  
  
**Examples:**  
`.ALIGN 4`  
`:__VAR_B` - the variable address will be a multiple of 4  
`DB`  
  
## .DEF directive  
  
`.DEF` directive creates new symbolic constant and assigns it a numeric value. A constant with omitted expression part is assigned zero value. After definition the constant can be used in expressions.  
  
**Syntax:**  
`.DEF <constant_name> [<expression>]`  
**Examples:**  
`.DEF PORTA_BASE 0x40010800`  
`.DEF GPIOA_INDR PORTA_BASE + 8`  
...  
...  
...  
`.IF DEFINED(GPIOA_INDR)`  
`...`  
`.ELSE`  
`.ERROR "GPIOA_INDR is not defined"`  
`.ENDIF`  
  
## .ERROR directive  
  
`.ERROR` directive emits a specified error message and stops assembling.  
  
**Syntax:**  
`.ERROR <double_quoted_error_message>`  
**Examples:**  
`.IF __HEAP_SIZE < 4`  
`.ERROR "insufficient heap size"`  
`.ENDIF`  
  
# Usage  
  
Executable file name of the assembler is `a1rv32.exe` or `a1rv32` depending on target platform. Command line syntax:  
`a1rv32 [options] <filename> [<filename1> .. <filenameN>]`  
Here `<filename>` .. `<filenameN>` are names of source files. Possible options are listed below.  
  
## Command-line options  
  
`-a` or `/a` - sections auto-alignment (described [here](#Sections-alignment-with--a-option))  
`-d` or `/d` - prints error description  
`-ex` or `/ex` - specifies RISC-V MCU extensions (default IC), e.g.: `-ex IC_Zmmul`  
`-f` or `/f` - enables pseudo-instructions helping to get rid from 12-bit immediate values limitation (including ones that uses T0 register internally), also the option enables algorithm of proper instruction selection, depending on its arguments  
`-l` or `/l` - libraries directory, e.g.: `-l "../lib"`  
`-m` or `/m` - specifies MCU name, e.g.: `-m CH32V003F4`  
`-mu` or `/mu` - prints memory usage  
`-nci` or `/nci` - forbids compressed instructions generation instead of 32-bit ones (if C extension is available)  
`-o` or `/o` - specifies output file name, e.g.: `-o out.ihx`  
`-ram_size` or `/ram_size` - specifies RAM size, e.g.: `-ram_size 0x800`  
`-ram_start` or `/ram_start` - specifies RAM starting address, e.g.: `-ram_start 0x20000000`  
`-rom_size` or `/rom_size` - specifies ROM size, e.g.: `-rom_size 0x4000`  
`-rom_start` or `/rom_start` - specifies ROM starting address, e.g.: `-rom_start 0x8000`  
`-t` or `/t` - sets target (default RV32), e.g.: `-t RV32`  
`-v` or `/v` - shows assembler version and terminates  
  
### Sections alignment with -a option  
  
`.DATA` - 4-bytes aligned address, size is a multiple of 4  
`.HEAP` - 4-bytes aligned address, size is a multiple of 4  
`.STACK` - 16-bytes aligned address, size is a multiple of 16  
`.CONST` - 4-bytes aligned address, size is a multiple of 2 or 4 (depending on compressed instructions availability)  
`.CODE INIT` - 4-bytes aligned address, size is a multiple of 2 or 4 (depending on compressed instructions availability)  
`.CODE` - 2-bytes or 4-bytes aligned address (depending on compressed instructions availability), size is a multiple of 2 or 4 (depending on compressed instructions availability)  
  
**Examples:**  
`a1rv32.exe -d -a -f -mu -m CH32V003F4 blink.asm`  
  
### Bypassing the 12-bit immediate values limit  
  
RISC-V RV32 instructions cannot use full 32-bit immediate values: usually they accept 12-bit signed values only. The upper 20-bits of the immediate value can be loaded into a register with `LUI` instruction if necessary.  
E.g.:  
`ADDI A0, ZERO, 2047` - loads 2047 immediate value into A0 register  
`ADDI A0, ZERO, 2048` - "integer out of range" warning will be produced because 2048 value does not fit into 12 bits  
`LUI A0, 1`  
`ADDI A0, A0, -2048` - these `LUI` + `ADDI` instructions can be used to load 2048 value into A0 register  
  
Specifying `-f` command-line option allows to avoid the issue because it makes special pseudo-instructions available. The pseudo-instructions accept 32-bit immediate values so assembler generates all necessary instructions automatically.  
E.g.:  
`LI A0, 2048` turns into the next instructions when `-f` option is specified:  
`LUI A0, 1` or `C.LUI A0, 1`  
`ADDI A0, A0, -2048`  
  