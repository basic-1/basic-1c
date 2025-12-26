# BASIC1 language reference  
  
**(refers to BASIC1 compiler for microcontrollers)**  
  
## Introduction  
  
BASIC1 language program is a sequence of text strings (program lines). BASIC1 compiler processes the program line by line starting from the first one. Every program line consists of line number, a single BASIC language statement and `EOL` sequence. Line number or statement, or both can be absent.  
  
Line number is a number in the range \[1 ... 65530\]  
  
Statement is a minimal unit of program which can be processed by compiler. Every statement should start from a statement keyword except for the implicit assignment (`LET` keyword can be omitted). Statement keywords of BASIC1 language are: `BREAK`, `CONTINUE`, `DATA`, `DEF`, `DIM`, `ELSE`, `ELSEIF`, `ERASE`, `FOR`, `GET`, `GOTO`, `GOSUB`, `IF`, `INPUT`, `IOCTL`, `LET`, `NEXT`, `OPTION`, `PRINT`, `PUT`, `READ`, `REM`, `RESTORE`, `RETURN`, `TRANSFER`, `WHILE`, `WEND`.  
  
**Examples of program lines:**  
`REM FOR statement with omitted line number`  
`FOR I = 0 TO 10`  
  
`REM Program line without statement`  
`100`  
  
`REM Implicit assignment statement`  
`50 A = A + 1`  
  
## Comments  
  
There are two types of comments in BASIC1: full-line comments and end-of-line comments. A full-line comment starts from `REM` keyword so the rest of the line is ignored. Once `REM` keyword is a BASIC language statement it can be preceeded by line number. End-of-line comment starts from `'` character (apostrophe). It can be placed in any position of any program line and makes the compiler ignoring the rest of the line.
  
**Examples of comments:**  
`10 REM This is a full-line comment`  
`REM Another full-line comment`  
`30 A = 20 'assign 20 to variable A`  
  
## Constants  
  
Constant is a part of BASIC program representing a number or a text string. String constants are embraced in double-quote characters. All double-quote characters withing string constants have to be doubled (to distinguish them from the embracing ones). BASIC1 compiler does not support fractional data types. Data type specifier can be added at the end of a numeric constant. See [**Data type specifiers**](#Data-type-specifiers) chapter for details. Optional `B1_FEATURE_HEX_NUM` feature allows writing integer constants in hexadecimal form (with `0x` prefix). BASIC1 compiler includes the option by default (refer to `b1core` submodule description for details).  
  
**Examples of constants:**  
`"this is a string constant"` - a string constant  
`"string constant with "" character"` - another string constant  
`0` - a numeric constant  
`-1`  
`+10`  
`123%` - a nummeric constant with `INT` data type specifier  
`0xFF` - a numeric constant in hexadecimal form  
  
## Identifiers  
  
Identifier is a text string used to name function or variable. Identifier must start from a Latin letter, can consist of Latin letters, digits and underscore character, can end with a type specifier and must not be longer than 31 characters. Type specifier character is mandatory for identifiers representing string variables or functions returing string values. String type specifier is `$` character. Numeric type specifiers are optional. Identifiers are case-insensitive so identifiers `var1`, `Var1` and `VAR1` all refer to the same function or variable.  
  
**Examples of identifiers:**  
`a` - can be a numeric variable name or a function returning numeric value  
`A%` - a numeric identifier with `INT` data type specifier  
`s$`, `s1$`, `text$` - string variables or functions names  
  
## Variables  
  
Variable is a named program object used for storing values. In BASIC program identifiers are used to name variables. Every variable has data type which determines values that the variable can contain. BASIC1 data types are described below (see [**Data types**](#Data-types) chapter). There are two types of variables in BASIC1 language: simple variables and subscripted variables. A simple variable can contain a single value only and a subscripted variable can contain multiple values, each identified with subscript(-s). Subscripted variables are often called arrays. BASIC1 compiler supports one-, two- and three-dimensional arrays. Initially every variable gets a value choosen as default value for its data type: zero for numeric data types and empty string for textual type. Simple and subscripted variables can be created explicitly using `DIM` statement. Subscripted variables created implicitly get minimum subscript value equal to 0 and maximum subscript value equal to 10. Minimum subscript value can be changed with `OPTION BASE` statement. `OPTION EXPLICIT` statement can be used to turn off implicit variables declaration.  
  
**Examples:**  
`A = 10` 'here `A` is a numeric variable  
`SV(1) = 10` '`SV` is a numeric subscripted variable (numeric array)  
`SV2(10, 10) = 100` '`SV2` is a numeric two-dimensional subscripted variable  
`S$ = "a string of text"` '`S$` is a simple string variable  
  
## Data types  
  
Data types supported by BASIC1 compiler:  
- `STRING` - used for storing textual data  
- `INT` - 16-bit signed integer  
- `WORD` - 16-bit unsigned integer  
- `BYTE` - 8-bit unsigned integer  
- `LONG` - 32-bit signed integer  
  
Every constant, variable, function or function argument is processed according to its data type. Default numeric data type is `INT`. String constants has to be enclosed in double-quotes. String variables names, names of functions returning string values or names of function string arguments must end with `$` character. Operands of every operator are converted to their common data type. Common data type is selected according to data types priority: `STRING` (the highest priority), `LONG`, `WORD`, `INT`, `BYTE` (the lowest priority). When assigning a value to a variable the value is converted to the variable's data type if possibly.  
  
**Examples:**  
`s$ = "text"` - assign string constant to `s$` string variable  
`s = "text"` - not correct, `s` cannot be a string variable name because of `$` type specifier absence  
`var = 5` - assign numeric value to `var` numeric variable  
`var$ = 5` - numeric value will be converted to string before assignment  
`var$ = 5 + "text"` - numeric value will be converted to string when processing addition (string concatenation) operator  
  
## Data type specifiers  
  
Data type specifiers can be used to define types of constants, variables, functions arguments and values returning with functions. Data type specifier has to be the last character of an identifier or constant. BASIC1 compiler supports the next data type specifiers:  
- `$` (dollar sign) - used to define string identifier  
- `%` (percent) - integer identifier or constant  
  
String data type specifier cannot be used with constants. Identifiers with the same names but different data type specifiers are different identifiers. Default data type for identifiers without data type specifiers is `INT`. `DIM` statement can declare variables of any numeric data type without data type specifiers in their names.  

**Examples of data type specifiers usage:**  
`a = 10% + 5` - 15 is assigned to integer variable `a`  
`a% = 10% + 5` - 15 integer value is assigned to `a%` variable of type `INT`  
`a = 10 / 3` - 3 is assigned to `a`  
  
## Operators and expressions  
  
Operators are characters indicating arithmetic and other operations performed on constants and variables in expressions. Every operator requires one or two values (operands) to perform operation on. Operators that require one operand are called unary operators and operators with two operands - binary operators. An BASIC expression consists of constants, variables, function calls and operators. Parentheses can be used in expressions to change operators evaluation order.  
  
### Unary operators  
  
- `+` - unary plus operator, does nothing, can be used with numeric operands only  
- `-` - unary minus operator, performs arithmetic negation, can be used with numeric operands only  
- `NOT` - bitwise operator performing ones' complement of an integer value (inverting all the bits in binary representation of the given integer value)  
  
### Binary operators  
  
- `+` - arithmetic addition for numeric operands and string concatenation for strings  
- `-` - arithmetic subtraction, numeric operands only  
- `*` - arithmetic multiplication  
- `/` - arithmetic division  
- `MOD` - modulo operator, returns remainder after integer division  
- `^` - calculates power of a number (exponentiation)  
- `AND` - bitwise AND operator, integer operands only  
- `OR` - bitwise OR operator, integer operands only  
- `XOR` - bitwise XOR operator, integer operands only  
- `<<` - bitwise left-shift operator, integer operands only  
- `>>` - bitwise right-shift operator, integer operands only  
- `=` - assignment operator, can be used with numerics and strings  
- `>` - "greater than" comparison operator, can be used with numerics and strings  
- `<` - "less than" comparison operator, can be used with numerics and strings  
- `>=` - "greater than or equal" comparison operator, can be used with numerics and strings  
- `<=` - "less than or equal" comparison operator, can be used with numerics and strings  
- `=` - "equal" comparison operator, can be used with numerics and strings  
- `<>` - "not equal" comparison operator, can be used with numerics and strings  
  
**Examples of expressions:**  
`A = 10` 'the simplest expresssion assigning numeric value to `A` variable  
`A = 5 + 10 * 2` '25 numeric value is assigned to `A`  
`A = (5 + 10) * 2` '30 numeric value is assigned to `A`  
`IF A > 10 THEN GOTO 100` 'here `A > 10` is a logical expression  
`FOR I = 0 TO A + 5 STEP S - 1` 'three expressions here: `I = 0`, `A + 5` and `S - 1`  
`S$ = S$ + 10` 'the expression concatenates two strings: value of `S$` variable and string representation of 10 and assigns the result to the same `S$` variable  
  
## Operator precedence  
  
Operator precedence determines the order of operators evaluation in one expression. An operator with higher precedence will be evaluated before an operator with lower precedence. Operators evaluation order can be changed using parentheses.  
  
### Operator precedence order  
  
- unary `+`, `-` and `NOT` operators (the highest order of precedence)  
- `^`  
- `*`, `/` and `MOD`  
- `+` and `-`  
- `<<` and `>>`  
- `AND`  
- `OR` and `XOR`  
- `>`, `<`, `>=`, `<=`, `=` and `<>` comparison operators  
- `=` (assignment operator, the lowest order of precedence)  
  
**Examples:**  
`A = A + -A` 'operators evaluation order: negation (unary `-`), addition (`+`), assignment (`=`)  
`A = A + B * C` 'order: multiplication (`*`), addition (`+`), assignment (`=`)  
`A = (A + B) * C` 'order: addition (`+`), multiplication (`*`), assignment (`=`)  
`IF A + 1 > B * 2 THEN GOTO 100` 'order: multiplication (`*`), addition (`+`), comparison (`>`)  
  
## Functions  
  
Function is a named block of code that can be reused multiple times by calling it by its name in expressions. Usually functions take one or more arguments and return some value. A function call in expression consists of the function name and the function arguments list enclosed in parentheses. Arguments must be delimited from each other with commas. Some functions allows omitting arguments.  
  
**Examples of function calls:**  
`A = ABS(X)` - calling `ABS` function accepting one argument  
`I = INSTR(, S1$, S2$)` - calling `INSTR` function with the first argument omitted  
`I = SOMEFN()` - calling `SOMEFN` function with a single argument omitted  
`S$ = NOARGS$` - calling function named `NOARGS$` without arguments  
  
There are two types of functions in BASIC1: built-in functions and user-defined functions. Built-in functions are provided by the language itself and can be used without any additional steps such as definition. User-defined functions have to be defined using special `DEF` statement.  
  
### Built-in functions  
  
- `ASC(<string>)` - returns integer code of the first character of a specified text string  
- `LEN(<string>)` - returns number of characters in a string  
- `CHR$(<numeric>)` - returns string consisting of a single character corresponding to integer code specified as function argument  
- `IIF(<logical>, <numeric1>, <numeric2>)` - takes three arguments: a logical expression, and two numeric values; if result of the logical expression is `TRUE` the function returns value of the first numeric argument and value of the second numeric argument is returned otherwise.  
- `IIF$(<logical>, <string1>, <string2>)` - takes three arguments: a logical expression, and two string values, is similar to `IIF` function but works with string arguments and returns string value  
- `STR$(<numeric>)` - converts numeric value to string  
- `VAL(<string>)` - converts textual representation of a number to numeric value if possibly  
- `ABS(<numeric>)` - returns the absolute value of a numeric value  
- `SGN(<numeric>)` - returns a numeric value indicating the sign of a specified number (-1 if the input number is negative, 0 if it is equal to 0 and 1 if the value is positive)  
- `MID$(<string>, <numeric1>, [<numeric2>])` - returns substring of a string specified with the first argument, the second argument is one-based starting position of the substring and the third argument is the substring length. The substring length argument is optional and if it's absent the function returns all the characters to the right of the starting position  
- `INSTR([<numeric>], <string1>, <string2>)` - returns one-based position of a string provided with the third argument in a string specified with the second argument. The first argument stands for a position to start search from (if it is omitted search starts from the beginning of the string). If the string is not found zero value is returned  
- `LTRIM$(<string>)` - trims leading blank (space and TAB) characters  
- `RTRIM$(<string>)` - trims trailing blank characters  
- `LEFT$(<string>, <numeric>)` - returns the leftmost part of a string specified with the first argument, the substring length is specified with the second argument  
- `RIGHT$(<string>, <numeric>)` - returns the rightmost part of a string specified with the first argument, the substring length is specified with the second argument  
- `LSET$(<string>, <numeric>)` - returns the string specifed with the first argument left justified to a length provided with the second argument  
- `RSET$(<string>, <numeric>)` - returns the string specifed with the first argument right justified to a length provided with the second argument  
- `LCASE$(<string>)` - converts all string letters to lower case  
- `RCASE$(<string>)` - converts all string letters to upper case  
- `SET$([<string>], <numeric>)` - returns a string consisting of several repeatitions of the same character, the first argument is a character to repeat (the first character of the string is used) and the second argument is number of character repeatitions. The first argument is optional: if it is omitted the function will return string consisting of spaces.  
- `CBYTE(<string> | <numeric>)` - converts numeric value or textual representation of a numeric value to `BYTE` integer value  
- `CINT(<string> | <numeric>)` - converts numeric value or textual representation of a numeric value to `INT` integer value  
- `CWRD(<string> | <numeric>)` - converts numeric value or textual representation of a numeric value to `WORD` integer value  
- `CLNG(<string> | <numeric>)` - converts numeric value or textual representation of a numeric value to `LONG` integer value  
  
**Examples:**  
`POS = INSTR(, "BASIC1", "BASIC")` 'look for "BASIC" in "BASIC1" string  
`S$ = LEFT("BASIC1", 5)` 'get first five characters of "BASIC1" string  
`S$ = MID$("BASIC1", 1, 5)` 'the same as previous  
`MIN = IIF(A > B, B, A)` 'get the minimum of two values  
`C% = IIF(B% = 0%, 0%, A% / B%)` 'avoid division by zero error using `IIF` function  
`S$ = SET$("ABC", 10)` '`S$` variable is assigned a string "AAAAAAAAAA"  
`A = CINT("-1")` 'variable `A` is assigned -1 value of type `INT`  
`A = CBYTE("-1")` 'error: -1 value cannot be represented with `BYTE` data type  
`A = CBYTE(300)` '`A` is assigned 44 value (300 converted to `BYTE` type)  
  
## Implicit data type conversion in expressions  
  
The implicit data type conversion occurs when a binary arithmetic or bitwise operator is applied to operands of different type. BASIC1 compiler converts value of a smaller data type to the larger one automatically. The resulting value of operator has the same data type as the larger operand has. There's one exception: exponentiation and shift operators do not convert their operands and the resulting value of such an operator has the same type as its left operand.  
  
**Examples:**  
`A = 100 + 200` 'A = 44 because 100 and 200 numeric constants are treated as `BYTE` values  
`A = 100 + 200%` 'A = 300 because 200% is an `INT` value and 100 `BYTE` value is promoted to `INT` automatically  
`A = 100 + 200 + 1000` 'A = 1044 because addition operator is left-associative (so the result of 100 + 200 has `BYTE` type)  
`A = 1000 + 100 + 200` 'A = 1300  
`A = 100% + 200 + 1000` 'A = 1300  
  
## Statements  
  
### `DATA`, `READ`, `RESTORE` statements  
  
`DATA`, `READ` and `RESTORE` statements are used for storing large number of numeric and textual values in program code and reading them successively.  
  
**Usage:**  
`DATA [(<type1>[, <type2>, ... <typeM>])] <value1>[, <value2>, ... <valueN>]`  
`READ <var_name1>[, <var_name2>, ... <var_nameM>]`  
`RESTORE [<line_number>]`  
  
`DATA` statement specifies a set of comma-delimited constant values. Textual constants have to be enclosed in double-quotes: such values have to meet the rules of BASIC regular string constants definition. Optional data types list can be specified after `DATA` keyword in parentheses: complier uses them to distinguish between values of different numeric types (`INT`, `WORD`, `BYTE`, `LONG`). A program can have multiple `DATA` statements. The values are read in the same order as they are defined in program. BASIC1 program has internal next value pointer: at the program start it points to the first value defined with `DATA` statements. Every reading operation changes the pointer making it referring the next value. `READ` statement reads values specified with `DATA` statements. `READ` keyword must be followed by comma-separated list of variable names to read values into. `RESTORE` statement sets the next value pointer to the first value of a `DATA` statement identified with the line number coming after `RESTORE` statement keyword. `RESTORE` statement without line number sets the pointer to the first value in the program (like at the program execution start).  
  
**Examples:**  
`10 DATA "a", "b", "c"` 'three textual constants consisting of single 'a', 'b' and 'c' letters  
`20 DATA 1, 2, 3` 'three numeric constants (`INT` by default) 
`30 DATA " a ", " b""", " c,"` 'three textual constants containing spaces, double-quote and comma  
`40 DATA(STRING) "a", "b", "c"` 'three textual constants consisting of single 'a', 'b' and 'c' letters  
`50 DATA(STRING, WORD) "A", 0, "B", 32767, "C", "65535"` ' "A", 0, "B", 32767, "C" and 65535 values  
`60 READ a$, b$, c$` 'read first three constants into `a$`, `b$` and `c$` variables  
`70 READ a, b, c` 'read 1, 2 and 3 integer values  
`80 RESTORE 20` 'restore the next value pointer to line number 20  
`90 READ a, b, c` 'read 1, 2 and 3 integer values again  
`100 READ a$, b$, c$` 'read textual constants  
`110 END`  
  
### `DEF` statement  
  
`DEF` statement creates a user-defined function. 

**Usage:**  
`DEF [GLOBAL] <function_name> [AS <type_name>] = <function_expression>` - creating user-defined function without arguments  
`<arg_decl> = <arg_name> [AS <type_name>]`  
`DEF [GLOBAL] <function_name>(<arg_decl1>[, <arg_decl2>, ... <arg_declN>]) [AS <type_name>] = <function_expression>` - creating user-defined function with arguments  
  
A user-defined function must be defined before being used. Function arguments are temporary variables existing only when the function is called. They differ from program variables with the same names defined outside the function (so such variables cannot be accessed with the function expression). Optional `GLOBAL` keyword allows declaring functions that can be used in other source files.  
  
**Examples:**  
`DEF MIN(A, B) = IIF(A > B, B, A)` 'returns a minimum of the two values specified with arguments  
`DEF CONCAT3$(S1$, S2$, S3$) = S1$ + S2$ + S3$` 'concatenates three string values  
`DEF GLOBAL MAKEWORD(HIBYTE AS BYTE, LOBYTE AS BYTE) AS WORD = (HIBYTE * 256) + LOBYTE` 'global function sample  
  
### `DIM` and `ERASE` statements  
  
`DIM` statement can be used to declare variables and allocate memory for them. `ERASE` statement clears simple variables (assigns them with their initial values: zero or empty string) or frees memory in case of arrays. BASIC1 language allows using variables without declaration: their types are determined by data type specifiers or set to `INT` if specifiers are absent. For arrays default upper subscript value is 10, default lower subscript value is 0 (can be changed with `OPTION BASE` statement). `OPTION EXPLICIT` statement specified in the beginning of a program forbids using undeclared variables. If the explicit variables declaration option is turned on, every variable must be created with `DIM` statement before usage.  
  
**Usage:**  
`<var_decl> = [GLOBAL] [STATIC] [VOLATILE] [CONST] <var_name>[([<subs1_lower> TO ]<subs1_upper>[, [<subs2_lower> TO ]<subs2_upper>[, [<subs3_lower> TO ]<subs3_upper>]])] [AS <type_name>] [AT <address>] [= <init_clause>]`  
`<init_clause> = <initializer> | (<initializer1>, <initializer2>, ... <initializerN>)`  
`DIM <var_decl1>[, <var_decl2>, ... <var_declN>]`  
`ERASE <var_name1>[, <var_name2>, ... <var_nameM>]`  
  
If optional `GLOBAL` keyword is specified the variable can be used in other source files. Non-global variables with the same names declared in multiple source files are different variables. Variables declaread using `VOLATILE` keyword are excluded from optimization process: compiler always produces code for reading and writing their values. The keyword should be used with variables which values can change unexpectedly: ones used in interrupt handlers, peripheral registers, etc. Adding `STATIC` keyword to an array declaration says compiler to reserve memory for the array at compile time. Using static arrays cause producing faster and more compact code but memory reserved for such arrays cannot be freed with `ERASE` statement (it just clears them assigning with initial values). Obviously, static arrays sizes must be known at compilation time. `AT <address>` optional clause allows declaring variables addressing specific memory area. Such variables do not reserve memory, they are just aliases for specific memory areas.  
`<subs1_lower>`, `<subs1_upper>`, `<subs2_lower>`, `<subs2_upper>`, `<subs3_lower>`, `<subs3_upper>` must be numeric expressions to specify lower and upper boundaries of variable subscripts. If a lower boundary of subscript is omitted it is taken equal to zero. The default value of lower boundary of subscripts can be changed with `OPTION BASE` statement. BASIC1 language supports one-, two- and three-dimensional subscripted variables (arrays). Optional variable type `<var_type>` must be one of the types described in the [**Data types**](#Data-types) chapter above. The type must correspond to the variable's data type specifier if it is present. If both data type specifier and data type name are omitted the statement creates variable of default numeric type (`INT`). `CONST` keyword declares a constant variable whose value (or values in case of array) cannot be changed. Constant variables must be initialized with `<init_clause>`: a single value for scalar variable and values list enclosed with parentheses in case of array.  
  
The `DIM` keyword can be omitted if the `CONST` keyword comes first in a variable declaration, so these two variable declarations are equal:  
`DIM CONST HEX$ = ("0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F")`  
`CONST HEX$ = ("0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F")`  
  
**Examples:**  
`DIM I%, I AS INT` 'declare two integer variables  
`DIM I1% AS INT` 'declare `I1%` integer variable  
`DIM S1$, S2$ AS STRING` 'declare two string variables  
`DIM IARR(25) AS INT` 'declare one-dimensional integer array with valid subscript range \[0 ... 25\]  
`DIM IARR1%(-10 TO 10)` 'integer array with subscript range \[-10 ... 10\]  
`DIM MAP(0 TO 10, 0 TO 10), MSG$(10)` 'two-dimensional `INT` array and one-dimensional string array  
`DIM A3(5, 5, 5)` 'declare three-dimensional array  
`DIM DA(100) AS BYTE` 'declare one-dimensional array of `BYTE` values  
`DIM GLOBAL FLAG AS BYTE` 'declare global `FLAG` variable  
`DIM VOLATILE C` 'compiler will not apply any optimization to `C` variable  
`DIM VOLATILE PA(0 TO 4) AS BYTE AT 0x5000` 'array referring to memory area at 0x5000 address, no memory is really allocated  
`DIM STATIC SARR(20)` 'declaring a static array  
`DIM STATIC SARR1(I)` 'wrong! static array size must be known at compile time  
`DIM CONST INT_MIN AS INT = -32768, CONST INT_MAX AS INT = 32767`'two `INT` constants  
`DIM CONST SDAT$(10 TO 15) = ("A", "B", "C", "D", "E", "F")` 'constant array of strings  
`DIM CONST DIGIT AS BYTE = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9)` 'constant `BYTE` array of ten values  
`ERASE MAP, MSG$` 'free memory occupied by `MAP` and `MSG$` variables  
`ERASE I%, I, I1%` 'erase three variables  
  
### `IF`, `ELSE`, `ELSEIF` statements  
  
`IF`, `ELSE`, `ELSEIF` statements allow executing other statements conditionaly depending on logical expression result.  
  
**Usage**:  
`IF <logical_expr1> THEN <statement1> | <line_number1>`  
`ELSEIF <logical_expr2> THEN <statement2> | <line_number2>`  
`...`  
`ELSEIF <logical_exprN> THEN <statementN> | <line_numberN>`  
`ELSE <statementE> | <line_numberE>`  
  
`IF` statement must be the first statement in every `IF`, `ELSEIF`, `ELSE` statements group and `ELSE` statement must be the last. `ELSEIF` and `ELSE` statements are optional. The logical expressions are tested starting from the first one. If an expression is TRUE, the statement following `THEN` is executed. If no one expression is TRUE the statement following `ELSE` is executed (if `ELSE` clause is present). If a line number is specified instead of statement after `THEN` or `ELSE` control is passed to the line of code identified with the line number.  
  
**Examples:**  
`10 A = 10`  
`20 B = 20`  
`30 IF A < B THEN S$ = "A < B"` '`A < B` is TRUE so `S$ = "A < B"` will be executed  
`40 ELSEIF A > B THEN S$ = "A = B"` 'this statement will be skipped  
`50 ELSE S$ = "A = B"` 'skipped too  
`60 IF B > 5 THEN 80` '`B > 5` is TRUE, go to line 80  
`70 A = B` 'this statement will not be executed  
`80 END`  
  
### `FOR`, `NEXT` statements  
  
`FOR` and `NEXT` statements are used to organize loops, allowing statements to be executed repeatedly.  
  
**Usage:**  
`FOR <loop_var_name> = <init_value> TO <end_value> [STEP <incr_value>]`  
`<statement_to_repeat1>`  
` ... `  
`<statement_to_repeatN>`  
`NEXT [<loop_var_name>]`  
  
Here `<loop_var_name>` is a loop control numeric variable name, `<init_value>` and `<end_value>` are numeric expressions specifying initial and ending variable values and optional `<incr_value>` is a numeric expression specifying the value at which the variable is incremented on each loop iteration. If `STEP <incr_value>` clause is omitted the value is assumed to be equal to 1. All three values are evaluated only once on the loop initialization stage. The loop terminates when the control variable's value reaches the ending value of the loop. Statements within `FOR` - `NEXT` loop can include another loop called inner or nested.  
  
**Examples:**  
`A = 0`  
`B = 1`  
`FOR I = 1 TO 5` '`I` variable changes from 1 to 5 within the loop, increment value is 1  
`A = A + I`  
`B = B * I`  
`NEXT I`  
`PRINT I, A, B` ' here `I` = 6, `A` = 15, `B` = 120  
`END`  
  
`A = 0`  
`B = 1`  
`FOR I = -1 TO -10 STEP -1` '`I` variable changes from -1 to -5 within the loop, increment value is -1  
`A = A + I`  
`B = B * I`  
`NEXT` '`NEXT` statement allows omitting variable name  
`PRINT I, A, B`' here `I` = -6, `A` = -15, `B` = -120  
`END`  
  
`REM nested loop sample`  
`A = 0`  
`FOR I = -1 TO -5 STEP -1` 'outer loop  
`FOR J = 1 TO 5` 'inner or nested loop  
`A = A + I * J`  
`NEXT` 'loop control variable name is omitted: `J` variable is assumed  
`NEXT` '`I` loop control variable is assumed  
`PRINT I, J, A` 'here `I` = -6, `J` = 6, `A` = -225  
`END`  
  
### `GOTO` statement  
  
`GOTO` statement changes normal program line execution order, program execution is passed to the line specified with line number coming after `GOTO` keyword.  

**Usage:**  
`GOTO <line_number>`  
  
**Examples:**  
`10 A = 10`  
`20 GOTO 40`  
`30 A = 20` 'this line will never be executed  
`40 PRINT A` 'here `A` variable is equal to 10  
`50 END`  
  
`10 GOTO 10` 'an infinite loop  
  
### `GOSUB` and `RETURN` statements  
  
`GOSUB` statement transfers program execution control to a program line identified with line number following the statement keyword (similar to `GOTO` statement) but before changing execution order it saves address of the next statement. `RETURN` statement moves execution control back to the statement following the `GOSUB` statement using the saved return address.  
  
**Usage:**  
`GOSUB <subroutine_line_number>`  
`RETURN`  
  
**Examples:**  
`10 A = 1`  
`20 GOSUB 1000` 'go to a subroutine on line 1000  
`30 PRINT A` 'here `A` is equal to 2  
`40 GOSUB 1000` 'go to the subroutine a time more  
`50 PRINT A` 'here `A` is equal to 3  
`60 GOSUB 1000`  
`70 PRINT A` 'here `A` is equal to 4  
`80 END`  
`1000 A = A + 1` ' the subroutine increments `A` variable  
`1010 RETURN` 'return from the subroutine  
  
### `GET` statement  
  
`GET` statement reads binary data from input device and stores it in variable(-s) or `BYTE` array(-s). The statement respects network byte order (big-endian) for numeric data.  
  
**Usage:**  
`GET [#<device_name>,] <var_name1>[, <var_name2>, ... <var_nameN>] [USING XOR(<value_in_out> | [<value_in>], [<value_out>])]` - syntax for scalar variables.  
`GET [#<device_name>,] <arr_var_name1>(<lbound1> TO <ubound1>)[, <arr_var_name2>(<lbound2> TO <ubound2>, ... <arr_var_nameN>(<lboundN> TO <uboundN>] [USING XOR(<value_in_out> | [<value_in>], [<value_out>])]` - syntax for subscripted variables (arrays).  
  
Here `<var_name1>` ... `<var_nameN>` are names of variables to write data into and `<device_name>` is an optional device name to read data from. If the device name is not specified the default input device is used. The device must support binary input mode. Allowed data types for input variables are: `BYTE`, `INT`, `WORD` and `LONG`. A special form of `GET` statement allows reading sequence of bytes and store it in a `BYTE` array: to use it specify one-dimensional `BYTE` array name with lower and upper bounds separated with `TO` keyword and enclosed in parentheses. `USING XOR` clause specifies a `BYTE` value to perform bitwise XOR operation on it and every byte read with the `GET` statement. If a single value `<value_in_out>` is specified in parentheses after `XOR` keyword, the value is used to perform XOR operations on both read and written data (`GET` statement performs input only). If two values `<value_in>` and `<value_out>` are specified, the first value is used for input data and the second one for output (in this case one of two values can be omitted if XOR operation is not needed for input or output data).  
  
**Examples:**  
`GET A` 'read a numeric from the default input device and write it into `A` variable  
`GET #SPI, DAT(I)` 'read a numeric from `SPI` device and write it into `DAT(I)` element of subscripted variable  
`GET #SPI, DAT1(0 TO 10)` 'read 11 bytes from `SPI` device and store them in `DAT1` array from index 0 to 10 (`DAT1` array data size must be `BYTE`)  
`GET #SPI, DAT1(I, I + 5)` 'read 6 bytes from `#SPI` device and store them in `BYTE` array starting from index `I`  
`GET A USING XOR(0xFF)` 'read a numeric value, perform XOR operation with 0xFF byte on every its byte and write it into `A` variable  
`GET A USING XOR(0xFF, 0xAA)` 'the same as previous, 0xAA value is not used because `GET` statement does not perform output operation  
`GET A USING XOR(0xFF, )` 'the same as previous, an unnecessary value is omitted  
`GET A USING XOR(, 0xFF)` 'no XOR operation is performed here because a value to XOR with input data is omitted  
  
### `IOCTL` statement  
  
`IOCTL` statement sends commands to MCU core and peripherals (to call hardware-specific functions). Available commands are listed [here](./ioctl.md).  
  
**Usage:**  
`IOCTL <device_name>, <command_name>[, <command_data>]`  
  
**Examples:**  
`IOCTL CPU, INTERRUPTS, ON`  
`IOCTL UART, SPEED, 9600`  
`IOCTL UART, ENABLE`  
  
### `INPUT` statement  
  
`INPUT` statement reads user input data from input device and stores it in variables. Default input device is UART (can be changed with `OPTION INPUTDEVICE` option).  
  
**Usage:**  
`INPUT [#<device_name>,] [<prompt>,] <var_name1>[, <var_name2>, ... <var_nameN>]`  
  
Here `<prompt>` is an optional string sent to the same device before reading data. Default prompt string is "? ". After displaying the prompt the statement reads values from input device and stores them into specified variables one by one. Input values must be separated with commas and the last value must be followed by carriage return character. If the statement fails (e.g. due to wrong data format or wrong values number) the input process repeats from very beginning. `<device_name>` is name of an input device for the statement to read data from (to override default input device usage).  
  
**Examples:**  
`INPUT A, B, C` 'input three numeric values  
`INPUT A%, B%, C%` 'input three integer values  
`INPUT A$` 'input a text value  
`INPUT "Enter your age: ", AGE` 'input one numeric value with prompt  
`IF AGE > 100 THEN PRINT "You're so old"`  
  
### `LET` statement  
  
`LET` statement assignes result of an expression calculation to a variable.  
  
**Usage:**  
`LET <var_name> = <expression>`  
  
The expression's result data type must be compatible with the variable data type. BASIC1 compiler implicitly converts any numeric value to string if necessary. Similarly, a numeric value can be converted to another numeric type when assigning to a variable. The statement has a simplified form with omitted `LET` keyword. So a program line without statement keyword is always considered as a simplified form of `LET` statement.  
  
**Examples:**  
`10 LET A = 10` 'assign numeric value 10 to `A` variable  
`20 LET A = A * A + VAL(S$)` 'more complex expression sample on the right side of the assignment operator  
`30 A = A + 1` 'implicit `LET` statement (with omitted keyword)  
`40 LET V(1) = A + A` 'assigning value to an element of array `V`  
`50 LET S$ = A` 'OK, value of `A` is converted to string automatically  
  
### `OPTION` statement  
  
`OPTION` is a statement specifying a compiler option that applies to entire program or to the current source file. All `OPTION` statements must precede any significant statement of a program (`REM` is the only statement which can be used prior to `OPTION`). Options supported by BASIC1 compiler are: `OPTION BASE`, `OPTION EXPLICIT`, `OPTION NOCHECK`, `OPTION INPUTDEVICE` and `OPTION OUTPUTDEVICE`. `OPTION INPUTDEVICE` and `OPTION OUTPUTDEVICE` affect the current source file only. Other options are applied to entire program.  
  
**Usage:**  
`OPTION BASE 0 | 1`  
`OPTION EXPLICIT [ON | OFF]`  
`OPTION NOCHECK [ON | OFF]`  
`OPTION INPUTDEVICE #<device_name>`  
`OPTION OUTPUTDEVICE #<device_name>`  
  
`OPTION BASE` statement can be used to change default value of lower boundary of variable subscripts from 0 to 1.  
  
`OPTION EXPLICIT` statement turns on explicit mode of variables creation. If the mode is enabled, every variable must be created with `DIM` statement prior to usage. Omitting `ON` and `OFF` keywords is interpreted as enabling the mode.  
  
`OPTION NOCHECK` option disables generating code that checks subscripted variables for being properly created (memory allocated). The check absence causes compiler to produce more compact code but usage of a non-allocated array can lead to software or hardware failure. The option has sense only when `OPTION EXPLICIT` is enabled too.  
  
`OPTION INPUTDEVICE` specifies default device for `GET`, `INPUT` and `TRANSFER` statements.  
  
`OPTION OUTPUTDEVICE` specifies default device for `PUT`, `PRINT` and `TRANSFER` statements.  
  
**Examples:**:  
`10 OPTION BASE 1`  
`20 DIM ARR1(25) AS INT` 'declare one-dimensional integer array with valid subscript range \[1 ... 25\]  
`30 ARR2(1, 1) = 10` 'here two-dimensional `ARR2` array with subscripts ranges \[1 ... 10\] is created implicitly  
`40 DIM ARR3(0 TO 25)` '`ARR3` subscript range is \[0 ... 25\] even with `OPTION BASE 1`  
  
`10 OPTION EXPLICIT` 'the same as `OPTION EXPLICIT ON`  
`20 DIM A, B, C` 'explicit variables creation: every variable must be created using `DIM` statement  
  
`10 OPTION EXPLICIT`  
`20 OPTION NOCHECK`  
`30 DIM ARR(10)`  
`40 ARR(0) = 1` 'ok here, memory is allocated with `DIM` statement  
`50 ERASE ARR`  
`60 ARR(0) = -1` 'wrong, memory is already freed with `ERASE`  
  
`10 OPTION OUTPUTDEVICE #ST7565_SPI`  
` ... `  
`100 PRINT "Hello world!"` 'this statement writes the string to #ST7565_SPI device  
  
### `PRINT` statement  
  
The statement writes textual data to output device. If device name is not specified the default output device is used (default output device for `PRINT` statement is UART, the device can be changed with `OPTION OUTPUTDEVICE` option).  
  
**Usage:**  
`PRINT [#<device_name>,] <expression1> [, | ; <expression2> , | ; ... <expressionN>] [, | ;]`  
  
`PRINT` statement evaluates expressions specified after the keyword and writes result values to output device one by one. Textual values are written as is and numeric values are first converted to textual representation. Comma expression separator makes the statement write the next value in the next print zone and semicolon separator allows writing values one after another. Finally `PRINT` statement writes end-of-line sequence if the expressions list does not terminate with semicolon. Putting semicolon at the end of the statement leaves cursor on the current line.  Entire print area is assumed to be divided into print zones. `PRINT` statement writes a value starting from the next print zone if the expression is separated from previous one with comma. The statement uses two special values to locate the next print zone: margin and zone width. Margin is the maximum width of output device print area (in characters) and zone width is a width of one print zone. Default values of margin and zone width are specific to output device. An output device should provide a way to change margin and zone width values.  
  
**Examples:**  
`PRINT` 'just go to the new line  
`PRINT A, B, C` 'print values of `A`, `B` and `C` variables, each in its separate print zone  
`PRINT A, B, C;` 'the same as previous but without moving cursor on new line  
`PRINT A$; B$; C$` 'print values of `A$`, `B$` and `C$` variables one right after another  
`PRINT "0"; TAB(6); "5"; TAB(11); "A"; TAB(16); "F"` 'prints "0    5    A    F" text  
`PRINT "0"; SPC(4); "5"; SPC(4); "A"; SPC(4); "F"` 'another way to print "0    5    A    F" text  
  
### `PUT` statement  
  
`PUT` statement writes binary data to output device. The statement respects network byte order (big-endian) for numeric data.  
  
**Usage:**  
`PUT [#<device_name>,] <exp1>[, <exp2>, ... <expN>] [USING XOR(<value_in_out> | [<value_in>], [<value_out>])]` - syntax for scalar values.  
`PUT [#<device_name>,] <arr_var_name1>(<lbound1> TO <ubound1>)[, <arr_var_name2>(<lbound2> TO <ubound2>, ... <arr_var_nameN>(<lboundN> TO <uboundN>] [USING XOR(<value_in_out> | [<value_in>], [<value_out>])]` - syntax for subscripted variables (arrays).  
  
The statement calculates specified expressions and writes their values to `<device_name>` device one by one. If the device name is not specified the default output device is used. The device must support binary output mode. Valid data types for the results of an expressions are: `BYTE`, `INT`, `WORD`, `LONG` or `STRING`. A special form of `PUT` statement allows transmitting sequence of bytes stored in `BYTE` array: to use it specify one-dimensional `BYTE` array name with lower and upper bounds separated with `TO` keyword and enclosed in parentheses. `USING XOR` clause specifies a `BYTE` value to perform bitwise XOR operation on it and every byte written with the `PUT` statement. If a single value `<value_in_out>` is specified in parentheses after `XOR` keyword, the value is used to perform XOR operations on both read and written data (`PUT` statement performs output only). If two values `<value_in>` and `<value_out>` are specified, the first value is used for input data and the second one for output (in this case one of two values can be omitted if XOR operation is not needed for input or output data).  
  
**Examples:**  
`PUT A` 'writes value of numeric `A` variable to the default output device  
`PUT #SPI, DAT(I)` 'writes value of `DAT(I)` element of subscripted variable to `SPI` device  
`PUT #SPI, "hello"` 'writes five characters of the specified string to `SPI` device (string length or any kind of terminating character is not written)  
`PUT #SPI, CBYTE(LEN(S$)), S$` 'writes `S$` string length (one byte) followed by the string data  
`PUT #SPI, DAT(0 TO 10)` 'writes 11 elements of `DAT` byte array starting from index 0  
`PUT A USING XOR(0xFF)` 'writes value of numeric `A` variable to the default output device with performing XOR operation on every its byte with 0xFF byte value  
`PUT A USING XOR(0xAA, 0xFF)` 'the same as previous, 0xAA value is not used because `PUT` statement does not perform input operation  
`PUT A USING XOR(, 0xFF)` 'the same as previous, an unnecessary value is omitted  
`PUT A USING XOR(0xFF, )` 'no XOR operation is performed here because a value to XOR with output data is omitted  
  
### `REM` statement  
  
The statement is used to write remarks or comments in program text. Compiler ignores all text between the statement and the end of the line. `REM` is the only statement that can precede `OPTION` statements in a program.  
  
**Usage:**  
`REM [<comment_text>]`  
  
### `TRANSFER` statement  
  
`TRANSFER` statement performs data transfer over an input/output device in both directions at the same time (for devices that support full-duplex data transmission mode). The statement respects network byte order (big-endian) for numeric data.  
  
**Usage:**  
`TRANSFER [#<device_name>,] <var_name1>[, <var_name2>, ... <var_nameN>] [USING XOR(<value_in_out> | [<value_in>], [<value_out>])]` - syntax for scalar variables.  
`TRANSFER [#<device_name>,] <arr_var_name1>(<lbound1> TO <ubound1>)[, <arr_var_name2>(<lbound2> TO <ubound2>, ... <arr_var_nameN>(<lboundN> TO <uboundN>] [USING XOR(<value_in_out> | [<value_in>], [<value_out>])]` - syntax for subscripted variables (arrays).  
  
Here `<var_name1>` ... `<var_nameN>` are names of variables to read data from and to write data into when performing data exchange with the device. `<device_name>` is an optional input/output device name. If the device name is not specified the default device is used (in this case default input device must be the same as default output device). The device must support binary input and output modes. Allowed data types for the variables are: `BYTE`, `INT`, `WORD` and `LONG`. A special form of `TRANSFER` statement allows exchanging data between input/output device and one-dimensional `BYTE` array: to use it specify the array name with lower and upper bounds separated with `TO` keyword and enclosed in parentheses. `USING XOR` clause specifies a `BYTE` value to perform bitwise XOR operation on it and every byte transferred with the statement. If a single value `<value_in_out>` is specified in parentheses after `XOR` keyword, the value is used to perform XOR operations on both read and written data. If two values `<value_in>` and `<value_out>` are specified, the first value is used for input data and the second one for output (in this case one of two values can be omitted if XOR operation is not needed for input or output data).  
  
**Examples:**  
`TRANSFER A` 'writes value of numeric `A` variable to the default output device and read value from the default input device into the same `A` variable  
`TRANSFER #SPI, DAT(I)` 'writes value of `DAT(I)` element of subscripted variable to `SPI` device and then stores a value read from `SPI` into `DAT(I)`  
`TRANSFER #SPI, DAT(I TO I + 5)` 'sends and receives 6 bytes from `DAT` byte array starting from index `I`  
`TRANSFER A USING XOR(0xFF)` 'writes value of numeric `A` variable to the default output device and read value from the default input device into the same `A` variable, every written byte is XORed with 0xFF value and every read byte is XORed with 0xFF value too  
`TRANSFER A USING XOR(0xFF, )` 'the same as previous, but input data is XORed with 0xFF value only  
`TRANSFER A USING XOR(, 0xFF)` 'the same as previous, but written data is XORed with 0xFF value only  
  
### `WHILE`, `WEND` statements  
  
`WHILE` and `WEND` statements are used to create loops executing statements while a logical expression evaluates as `TRUE`.  
  
**Usage:**  
`WHILE <logical_expr>`  
`<statement_to_repeat1>`  
` ... `  
`<statement_to_repeatN>`  
`WEND`  
  
Here `<logical_expr>` is a logical expression evaluated before every loop iteration. If the expression's result is `TRUE` the code fragment between `WHILE` and `WEND` statements executes once and execution control goes to the `WHILE` statement again. Statements within `WHILE` - `WEND` loop can include another loop called inner or nested.  
  
**Examples:**  
`REM print numbers from 0 to 3`  
`I = 0`  
`WHILE I <= 3`  
`PRINT I`  
`I = I + 1`  
`WEND`  
  
`REM an infinite loop`  
`WHILE 1 < 2`  
`PRINT "infinite loop"`  
`WEND`  
  
`REM the loop below executes zero times`  
`I = 0`  
`WHILE I > 0`  
`PRINT "never executes"`  
`WEND`  
  
### `BREAK` statement  
  
`BREAK` statement exits the enclosing loop.  
  
**Usage:**  
`BREAK`  
  
**Examples:**  
`REM the loop below will be terminated at the end of the second iteration`  
`FOR I = 0 TO 10`  
`PRINT I`  
`IF I > 0 THEN BREAK`  
`NEXT I`  
  
### `CONTINUE` statement  
  
The statement moves execution control on the next loop iteration.  
  
**Usage:**  
`CONTINUE`  
  
**Examples:**  
`REM print non-negative numbers only`  
`FOR I = 0 TO 10`  
`IF ARR(I) < 0 THEN CONTINUE`  
`PRINT ARR(I)`  
`NEXT I`  
  