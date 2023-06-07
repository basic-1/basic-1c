# BASIC1 language reference  
  
**(refers to BASIC1 language compiler for microcontrollers)**  
  
## Introduction  
  
BASIC1 language program is a sequence of text strings (program lines). BASIC1 compiler processes the program line by line starting from the first one. Every program line consists of line number, a single BASIC language statement and `EOL` sequence. Line number or statement or both can be absent.  
  
Line number is a number in the range \[1 ... 65530\]  
  
Statement is a minimal unit of program which can be processed by compiler. Every statement should start from a statement keyword except for the implicit assignment (`LET` keyword can be omitted). Statement keywords of BASIC1 language are: `BREAK`, `CONTINUE`, `DATA`, `DEF`, `DIM`, `ELSE`, `ELSEIF`, `ERASE`, `FOR`, `GOTO`, `GOSUB`, `IF`, `INPUT`, `IOCTL`, `LET`, `NEXT`, `OPTION`, `PRINT`, `READ`, `REM`, `RESTORE`, `RETURN`, `WHILE`, `WEND`.  
  
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
`30 A = 20 'assign the value 20 to the variable A`  
  
## Constants  
  
Constant is an element of every BASIC program representing a number or text string. String constants are embraced in double-quote characters. All double-quote characters withing string constants have to be doubled (to distinguish them from the embracing characters). BASIC1 compiler does not support fractional data types. Numeric constants allow type specifying character addition at the end. See **Data type specifiers** chapter for details. Optional `B1_FEATURE_HEX_NUM` feature allows writing integer constants in hexadecimal form (with `0x` prefix).  
  
**Examples of constants:**  
`"this is a string constant"`  
`"string constant with "" character"`  
`0`  
`-1`  
`+10`  
`123`  
`0xFF`  
  
## Identifiers  
  
Identifier is a text string representing function or variable. Identifier must start from a Latin letter, can consist of Lating letters, digits and underscore character, can end with a type specifier and must not be longer than 31 character. Type specifier character is mandatory for identifiers representing string variables or functions returing string values. String type specifier is `$` character. Numeric type specifiers are optional. Identifier are case-insensitive so identifiers `var1`, `Var1` and `VAR1` all refer to the same function or variable.  
  
**Examples of identifiers:**  
`a` - can be a numeric variable name or a function returning numeric value  
`s$`, `s1$`, `text$` - string variables or functions names  
  
## Variables  
  
Variable is a named program object used for storing values. In BASIC program variables are represented with identifiers. Every variable has data type which determines the values that the variable can contain. BASIC1 data types are described below (see **Data types** chapter). There are two types of variables in BASIC1 language: simple variables and subscripted variables. A simple variable can contain a single value only and a subscripted variable can contain multiple values, each identified with subscript(-s). Subscripted variables are often called arrays. BASIC1 compiler supports one-, two- and three-dimensional arrays. Initially every variable gets a value choosen as default value for its data type: zero for numeric data types and empty string for textual one. Simple and subscripted variables can be created explicitly using `DIM` statement. Subscripted variables created implicitly get minimum subscript value equal to 0 and maximum subscript value equal to 10. Minimum subscript value can be changed with `OPTION BASE` statement. `OPTION EXPLICIT` statement can be used to turn off implicit variables declaration.  
  
**Examples:**  
`A = 10` 'here `A` is a numeric variable  
`SV(1) = 10` '`SV` is a numeric subscripted variable (numeric array)  
`SV2(10, 10) = 100` '`SV2` is a numeric two-dimensional subscripted variable  
`S$ = "a string of text"` '`S$` is a simple string variable  
  
## Data types  
  
Data types supported by BASIC1 compiler:  
- `STRING` - used for storing textual data  
- `INT` - 16-bit integer  
- `WORD` - 16-bit unsigned integer  
- `BYTE` - 8-bit unsigned integer  
  
Every constant, variable, function or function argument is processed according to its data type. Default numeric data type is `INT`. String constants are enclosed in double-quotes and string variables names, names of functions returning string values or names of function string arguments must end with `$` character. Operands of every operator are converted to their common data type. Common data type is selected according to data types priority: `STRING` (the highest priority), `WORD`, `INT`, `BYTE` (the lowest priority). When assigning a value to a variable the value is converted to the variable's data type if possibly.  
  
**Examples:**  
`s$ = "text"` - assign string constant to the `s$` string variable  
`s = "text"` - not correct, `s` cannot be a string variable name because of `$` type specifier absence  
`var = 5` - assign numeric value to the `var` numeric variable  
`var$ = 5` - numeric value will be converted to string before assignment  
`var$ = 5 + "text"` - numeric value will be converted to string when processing addition operator  
  
## Data type specifiers  
  
Data type specifiers can be used to define types of constants, variables, functions arguments and values returning with functions. Data type specifier has to be the last character of an identifier or constant. BASIC1 language supports the next data type specifiers:  
- `$` (dollar sign) - used to define string identifier  
- `%` (percent) - integer identifier or constant  
  
String data type specifier cannot be used with constants. Identifiers with the same names but different data type specifiers are different identifiers. Default data type for identifiers without data type specifiers is `INT`. `DIM` statement can declare variables of any numeric data type without data type specifiers in their names.  

**Examples of data type specifiers usage:**  
`a = 10% + 5` - 15 is assigned to integer variable `a`  
`a% = 10% + 5` - 15 integer value is assigned to `a%` variable of type `INT`  
`a = 10 / 3` - 3 is assigned to `a`  
  
## Operators and expressions  
  
Operators are characters indicating arithmetic and other operations performed on constants and variables in expressions. Every operator requires one or two values (operands) to perform operation on. Operators that require one operand are called unary operators and operators with two operands - binary operators. An expression is a mathematical expression consisting of constants, variables, function calls and operators. Parentheses can be used in expressions to change operators evaluation order.  
  
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
`A = 5 + 10 * 2` 'after the expression evaluation 25 numeric value is assigned to `A`  
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
  
There are two types of functions in BASIC1: built-in functions and user-defined functions. Built-in functions are provided by the language itself and can be used without any additional steps such as definition. User-defined functions have to be defined using special `DEF` statement.  
  
### Built-in functions  
  
`ASC(<string>)` - returns integer code of the first character of a specified text string  
`LEN(<string>)` - returns number of characters in a string  
`CHR$(<numeric>)` - returns string consisting of a single character corresponding to integer code specified as function argument  
`IIF(<logical>, <numeric1>, <numeric2>)` - takes three arguments: a logical expression, and two numeric expressions; evaluates the logical expression and if the result of the expression is `TRUE` the function returns result of evaluation of the first numeric expression and result of the second numeric exprssion is returned otherwise.  
`IIF$(<logical>, <string1>, <string2>)` - takes three arguments: a logical expression, and two string expressions, is similar to `IIF` function but works with string arguments and returns string value  
`STR$(<numeric>)` - converts numeric value to string  
`VAL(<string>)` - converts textual representation of a number to numeric value if possibly  
`ABS(<numeric>)` - returns the absolute value of a numeric value  
`SGN(<numeric>)` - returns a numeric value indicating the sign of a specified number (-1 if the input number is negative, 0 if it is equal to 0 and 1 if the value is positive)  
`MID$(<string>, <numeric1>, [<numeric2>])` - returns the substring of a string specified with the first argument, one-based starting position is specified with the second argument and the third stands for substring length. The last parameter is optional and if it's absent the function returns all the characters to the right of the starting position  
`INSTR([<numeric>], <string1>, <string2>)` - returns one-based position of a string provided with the third argument in a string specified with the second argument. The first argument stands for a position to start search from (if it is omitted search starts from the beginning of the string). If the string is not found zero value is returned  
`LTRIM$(<string>)` - trims leading blank (space and TAB) characters  
`RTRIM$(<string>)` - trims trailing blank characters  
`LEFT$(<string>, <numeric>)` - returns the leftmost part of a string specified with the first argument, the substring length is specified with the second argument  
`RIGHT$(<string>, <numeric>)` - returns the rightmost part of a string specified with the first argument, the substring length is specified with the second argument  
`LSET$(<string>, <numeric>)` - returns the string specifed with the first argument left justified to a length provided with the second argument  
`RSET$(<string>, <numeric>)` - returns the string specifed with the first argument right justified to a length provided with the second argument  
`LCASE$(<string>)` - converts all string letters to lower case  
`RCASE$(<string>)` - converts all string letters to upper case  
  
**Examples:**  
`POS = INSTR(, "BASIC1", "BASIC")` 'look for "BASIC" in "BASIC1" string  
`S$ = LEFT("BASIC1", 5)` 'get first five characters of "BASIC1" string  
`S$ = MID$("BASIC1", 1, 5)` 'the same as previous  
`MIN = IIF(A > B, B, A)` 'get the minimum of two values  
`C% = IIF(B% = 0%, 0%, A% / B%)` 'avoid division by zero error using `IIF` function  
  
## Statements  
  
### `DATA`, `READ`, `RESTORE` statements  
  
`DATA`, `READ` and `RESTORE` statements can be used for storing large number of numeric and textual values in BASIC program code and reading them successively.  
  
**Usage:**  
`DATA [(<type1>[, <type2>, ... <typeM>])] <value1>[, <value2>, ... <valueN>]`  
`READ <var_name1>[, <var_name2>, ... <var_nameM>]`  
`RESTORE [<line_number>]`  
  
`DATA` statement specifies a set of comma-delimited constant values. Textual constants have to be enclosed in double-quotes: such values have to meet the rules of BASIC regular string constants definition. Optional data types list can be specified after `DATA` keyword in parentheses: the types are used by the complier to distinguish between values of different numeric types (`INT`, `WORD`, `BYTE`). A program can have multiple `DATA` statements, the order of the statements in the program determines the order of the values. BASIC1 program has internal next value pointer: at the program start it points to the first value defined with `DATA` statements. Every reading operation changes the pointer making it referring the next value. `READ` statement reads values specified with `DATA` statements. `READ` keyword must be followed by either one variable name or comma-separated list of variable names to store values in. `RESTORE` statement sets the next value pointer to the first value of a `DATA` statement identified with the line number coming after `RESTORE` statemenr keyword. `RESTORE` statement without line number sets the pointer to the first value in the program (like at the program execution start).  
  
**Examples:**  
`10 DATA "a", "b", "c"` 'three textual constants consisting of single `a`, `b` and `c` letters  
`20 DATA 1, 2, 3` 'three numeric or textual constants  
`30 DATA " a ", " b""", " c,"` 'three textual constants containing spaces, double-quote and comma  
`40 DATA(STRING) "a", "b", "c"` 'three textual constants consisting of single `a`, `b` and `c` letters  
`50 DATA(STRING, WORD) "A", 0, "B", 32767, "C", "65535"` ' `A`, `0`, `B`, `32767`, `C` and `65535` values  
`60 READ a$, b$, c$` 'read first three constants into `a$`, `b$` and `c$` variables  

`70 READ a$, b$, c$` 'read `1`, `2` and `3` constants as strings  
`80 RESTORE 30` 'restore the next value pointer to line number 30  
`90 READ a%, b%, c%` 'read `1`, `2` and `3` constants as integer values  
`100 READ a$, b$, c$` 'read textual constants with spaces and other special characters: ` a `, ` b"` and ` c,`  
`110 END`  
  
### `DEF` statement  
  
`DEF` statement creates a user-defined function. 

**Usage:**  
`DEF <function_name> = <function_expression>` - creating user-defined function without arguments  
`DEF <function_name>(<arg_name1>[, <arg_name2>, ... <arg_nameN>]) = <function_expression>` - creating user-defined function with arguments  
  
A user-defined function must be defined before being used. Function arguments are temporary variables existing only when the function is called. They differ from program variables with the same names defined outside the function (so such variables cannot be accessed with the function expression).  
  
**Examples:**  
`DEF RND100 = RND * 100` 'returns a random value from within a range (0, 100)  
`DEF MIN(A, B) = IIF(A > B, B, A)` 'returns a minimum of the two values specified with arguments  
`DEF CIRCAREA(R) = PI * R ^ 2` 'area of circle with radius R  
`DEF ROUND%(A) = A` 'converts floating-point value to integer  
`DEF CONCAT3$(S1$, S2$, S3$) = S1$ + S2$ + S3$` 'concatenates three string values  
  
### `DIM` and `ERASE` statements  
  
`DIM` statement allocates memory for variable(-s) and `ERASE` statement frees memory occupied by variable(-s). By default BASIC1 interpreter creates a variable when meets it first in an expression. The behavior can be changed by specifying `OPTION EXPLICIT` statement in the beginning of a program. If the explicit variables declaration option is turned on every variable must be created with `DIM` statement before usage.  
  
**Usage:**  
`<var_decl> = <var_name>[([<subs1_lower> TO ]<subs1_upper>[, [<subs2_lower> TO ]<subs2_upper>[, [<subs3_lower> TO ]<subs3_upper>]])][AS <type_name>]`  
`DIM <var_decl1>[, <var_decl2>, ... <var_declN>]`  
`ERASE <var_name1>[, <var_name2>, ... <var_nameM>]`  
  
`<subs1_lower>`, `<subs1_upper>`, `<subs2_lower>`, `<subs2_upper>`, `<subs3_lower>`, `<subs3_upper>` must be numeric expressions to specify lower and upper boundaries of variable subscripts. If a lower boundary of subscript is omitted it is taken equal to zero. The default value of lower boundary of subscripts can be changed with `OPTION BASE` statement. BASIC1 interpreter supports one-, two- and three-dimensional subscripted variables (arrays).  Three-dimensional arrays support is optional but enabled by default for Linux and Windows builds. Optional variable type `<var_type>` must be one of the types described in the **Data types** chapter above. The type must correspond to the variable's data type specifier if it is present. If both data type specifier and data type name are omitted the statement creates variable of default numeric type (`SINGLE`).  
  
**Examples:**  
`DIM I%, I AS INT` 'declare two integer variables  
`DIM I1% AS INT` 'declare `I1%` integer variable  
`DIM A, A!, B AS SINGLE, B! AS SINGLE` 'declare floating-point variables  
`DIM S1$, S2$ AS STRING` 'declare two string variables  
`DIM IARR(25) AS INT` 'declare one-dimensional integer array with valid subscript range \[0 ... 25\]  
`DIM IARR1%(-10 TO 10)` 'integer array with subscript range \[-10 ... 10\]  
`DIM MAP(0 TO 10, 0 TO 10), MSG$(10)` 'two-dimensional floating-point array and one-dimensional string array  
`DIM A3(5, 5, 5)` 'declare three-dimensional array  
`DIM DA(100) AS DOUBLE` 'declare one-dimensional array of `DOUBLE` values  
`DIM DA#(100)` 'another way of declaring `DOUBLE` array  
`ERASE MAP, MSG$` 'free memory occupied by `MAP` and `MSG$` variables  
`ERASE I%, I, I1%` 'delete three variables  
  
### `IF`, `ELSE`, `ELSEIF` statements  
  
`IF`, `ELSE`, `ELSEIF` statements allow executing other statements conditionaly depending on logical expression result.  
  
**Usage**:  
`IF <logical_expr1> THEN <statement1> | <line_number1>`  
`ELSEIF <logical_expr2> THEN <statement2> | <line_number2>`  
`...`  
`ELSEIF <logical_exprN> THEN <statementN> | <line_numberN>`  
`ELSE <statementE> | <line_numberE>`  
  
`IF` statement must be the first statement in every `IF`, `ELSE`, `ELSEIF` statements group and `ELSE` statement must be the last. `ELSE` and `ELSEIF` statements are optional. BASIC interpreter evaluates every logical expression one by one and tests their resulting values: the first expression that evaluates to TRUE value causes corresponding statement execution. The rest of `ELSEIF` and `ELSE` statements are skipped. `ELSE` statement is executed only if all logical expressions of preceding `IF` and `ELSEIF` statements evaluate to FALSE. `IF`, `ELSE`, `ELSEIF` statements allow specifing line numbers after `THEN` and `ELSE` keywords (instead of the statements to execute): in this case interpreter just changes order of statements execution and goes to processing of a program line identified with the line number.  
  
**Examples:**  
`10 A = 10`  
`20 B = 20`  
`30 IF A < B THEN S$ = "A < B"` '`A < B` evaluates to TRUE so `S$ = "A < B"` will be executed  
`40 ELSEIF A > B THEN S$ = "A = B"` 'this statement will be skipped  
`50 ELSE S$ = "A = B"` 'skipped too  
`60 IF B > 5 THEN 80` '`B > 5` evaluates to TRUE so interpreter goes executing statement on line 80  
`70 A = B` 'this statement will not be executed  
`80 END`  
  
### `FOR`, `NEXT` statements  
  
`FOR` and `NEXT` statements are used to organize loops allowing statements to be executed repeatedly.  
  
**Usage:**  
`FOR <loop_var_name> = <init_value> TO <end_value> [STEP <incr_value>]`  
`<statement_to_repeat1>`  
` ... `  
`<statement_to_repeatN>`  
`NEXT [<loop_var_name>]`  
  
Here `<loop_var_name>` is a loop control numeric variable name, `<init_value>` and `<end_value>` are numeric expressions specifying initial and ending variable values and optional `<incr_value>` is a numeric expression specifying the value at which the variable is incremented on each loop iteration. If `STEP <incr_value>` clause is omitted the interpreter assumes the value is equal to 1. All three values are evaluated only once on the loop initialization stage. The loop terminates when the control variable's value reaches the ending value of the loop. Statements within `FOR` - `NEXT` loop can include another loop called inner or nested.  
  
**Examples:**  
`A = 0`  
`B = 1`  
`FOR I = 1 TO 10` '`I` variable changes from 1 to 10 within the loop, increment value is 1  
`A = A + I`  
`B = B * I`  
`NEXT I`  
`PRINT I, A, B`' here `I` = 11, `A` = 55, `B` = 3628800  
`END`  
  
`A = 0`  
`B = 1`  
`FOR I = -1 TO -10 STEP -1` '`I` variable changes from -1 to -10 within the loop, increment value is -1  
`A = A + I`  
`B = B * I`  
`NEXT` '`NEXT` statement allows omitting variable name  
`PRINT I, A, B`' here `I` = -11, `A` = -55, `B` = 3628800  
`END`  
  
`REM nested loop sample`  
`A = 0`  
`FOR I = -1 TO -10 STEP -0.5` 'outer loop, increment can be a fractional value  
`FOR J = 1 TO 10` 'inner or nested loop  
`A = A + I * J`  
`NEXT` 'loop control variable name is omitted: `J` variable is assumed  
`NEXT` '`I` loop control variable is assumed  
`PRINT I, J, A` 'here `I` = -10.5, `J` = 11, `A` = -5747.5  
`END`  
  
### `GOTO` statement  
  
`GOTO` statement changes normal program line execution order, interpreter goes to a program line specified with line number coming after `GOTO` keyword.  

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
  
`GOSUB` and `RETURN` statements can be used to organize subroutine calls. `GOSUB` statement changes program line execution order similar to `GOTO` statement but it saves its program line pointer and then `RETURN` statement goes back to execution of a statement following the line saved by `GOSUB`.  
  
**Usage:**  
`GOSUB <subroutine_line_number>`  
`RETURN`  
  
**Examples:**  
`10 A = 1`  
`20 GOSUB 1000` 'go to a subroutine on line 1000  
`30 PRINT A` 'here `A` is equal to 2  
`40 GOSUB 1000` 'go to the subroutine a time more  
`50 PRINT A` 'here `A` isd equal to 3  
`60 GOSUB 1000`  
`70 PRINT A` 'here `A` is equal to 4  
`80 END`  
`1000 A = A + 1` ' the subroutine increments `A` variable  
`1010 RETURN` 'return from the subroutine  
  
### `INPUT` statement  
  
`INPUT` statement reads user input data from keyboard and stores it in variables.  
  
**Usage:**  
`INPUT [<prompt>,] <var_name1>[, <var_name2>, ... <var_nameN>]`  
  
Here `<prompt>` is an optional string constant displaying to user before reading input data. Default user prompt string is "? ". After displaying the prompt the statement starts reading values from keyboard and assigning them to specified variables one by one. Input values must be separated with commas and the values number must be equal to the number of specified variables. The interpreter repeats the input if it fails with parsing of entered data.  
  
**Examples:**  
`INPUT A, B, C` 'input three numeric values  
`INPUT A%, B%, C%` 'input three integer values  
`INPUT A$` 'input a text value  
`INPUT "Enter your age: ", AGE` 'enter one numeric value with prompt  
`IF AGE > 100 THEN PRINT "You're so old"`  
  
### `LET` statement  
  
`LET` statement assignes result of an expression evaluation to a variable.  
  
**Usage:**  
`LET <var_name> = <expression>`  
  
The expression has to evaluate to a numeric or string value and type of the variable has to be compatible with the value's type. BASIC1 interpreter implicitly converts any numeric value to a string and different numeric data types between each other (even with data loss). `LET` statement has a simplified form with omitted `LET` keyword. So the interpreter treats a program line without statement keyword as `LET` statement.  
  
**Examples:**  
`10 LET A = 10` 'assign value 10 to `A` variable  
`20 LET A = A * A + RND` 'more complex expression sample on the right side of the assignment operator  
`30 A = A + 1` 'implicit `LET` statement (with omitted keyword)  
  
### `ON` ... `GOTO`|`GOSUB` statements  
  
The statements are similar to `GOTO` and `GOSUB` statements: they change normal program line execution order but allow selecting destination program line number from list of line numbers.  
  
**Usage:**  
`ON <numeric_expression> GOTO <line_number1>[, <line_number2>, ... <line_numberN>]`  
`ON <numeric_expression> GOSUB <line_number1>[, <line_number2>, ... <line_numberN>]`  
  
BASIC1 interpreter evaluates an expression `<numeric_expression>` and then selects line number to proceed with `GOTO` or `GOSUB` according to the expression result: if the result is 1 it takes the first line number from the list, if the result is 2 - the second, etc. If the result is less than one or greater than the number of line numbers in the list an error is reported.  
  
**Examples:**  
`10 A = 2`  
`20 ON A - 1 GOSUB 100, 200, 300` 'the first line number from the list will be selected  
`30 END`  
`100 PRINT "first"` 'this subroutine will be called with `ON ... GOSUB` statement  
`110 RETURN`  
`200 PRINT "second"`  
`210 RETURN`  
`300 PRINT "third"`  
`310 RETURN`  
  
### `OPTION` statement  
  
`OPTION` is a special statement that changes interpreter's behavior. The statement affects on entire program and all `OPTION` statements must precede any significant statement of a program (`REM` is the only statement which can be used prior to `OPTION`). There are two options supported by BASIC1 interpreter: `OPTION BASE` and `OPTION EXPLICIT`.  
  
**Usage:**  
`OPTION BASE 0 | 1`  
`OPTION EXPLICIT [ON | OFF]`  
  
`OPTION BASE` statement specifies default value of lower boundary of variable subscripts. At the program execution beginning the value is set to zero. The statement allows changing it to 1.  
  
`OPTION EXPLICIT` is used to turn on the explicit mode of variables creation. If the mode is enabled every variable must be created with `DIM` statement prior to usage. At the program execution beginning the explicit mode is disabled. Omitting `ON` and `OFF` keywords is interpreted as enabling the mode.  
  
**Examples:**:  
`10 OPTION BASE 1`  
`20 DIM IARR(25) AS INT` 'declare one-dimensional integer array with valid subscript range \[1 ... 25\]  
`30 ARR(1, 1) = 10` 'here interpreter will create two-dimensional `ARR` array with subscripts ranges \[1 ... 10\]  
  
`10 OPTION EXPLICIT` 'the same as `OPTION EXPLICIT ON`  
`20 DIM A, B, C` 'explicit variables creation: every variable must be created using `DIM` statement  
  
### `PRINT` statement  
  
The statement writes textual data to an output device (usually it is display).  
  
**Usage:**  
`PRINT <expression1> [, | ; <expression2> , | ; ... <expressionN>] [, | ;]`  
  
Interpreter evaluates expressions specified with `PRINT` statement and writes result values to output device one by one. Textual values are written as is and numeric values are first converted to textual representation. Comma expression separator makes the interpreter write the next value in the next print zone and semicolon separator allows writing values one after another. Finally `PRINT` statement writes end-of-line sequence if the expressions list does not terminate with semicolon. Putting semicolon at the end of the statement makes interpreter leave cursor on the current line.  Entire print area is assumed to be divided into print zones. `PRINT` statement writes a value starting from the next print zone if the expression is separated from previous one with comma. The interpreter uses two special values to locate the next print zone: `MARGIN` and `ZONEWIDTH`. `MARGIN` is the maximum width of output device (in characters) and `ZONEWIDTH` is a width of one print zone. Default margin value is set to 80 characters and zone width is set to 10 characters so there are 8 print zones in a single line. Margin and zone width values can be changed with `SET MARGIN` and `SET ZONEWIDTH` statements.  
  
**Examples:**  
`PRINT` 'just go to the new line  
`PRINT A, B, C` 'print values of `A`, `B` and `C` variables, each in its separate print zone  
`PRINT A, B, C;` 'the same as previous but without moving cursor on new line  
`PRINT A$; B$; C$` 'print values of `A$`, `B$` and `C$` variables one right after another  
`PRINT "0"; TAB(6); "5"; TAB(11); "A"; TAB(16); "F"` 'prints "0    5    A    F" text  
`PRINT "0"; SPC(4); "5"; SPC(4); "A"; SPC(4); "F"` 'another way to print "0    5    A    F" text  
  
### `RANDOMIZE` statement  
  
`RANDOMIZE` statement initializes random-sequence generator making it start a new random values sequence. See also `RND` function description.  
  
**Usage:**  
`RANDOMIZE`  
  
### `REM` statement  
  
The statement is used to write remarks or comments in program text. Interpreter ignores all the text between the statement and the end of the line. `REM` is the only statement that can precede `OPTION` statements in a program.  
  
**Usage:**  
`REM [<comment_text>]`  
  
### `SET` statement  
  
`SET` is similar to `OPTION` statement because it changes interpreter behavior but in contrast to `OPTION` statement it can be used in any part of a program. Now the statement allows changing the next parameters `MARGIN` and `ZONEWIDTH`, both of them have effect on `PRINT` statement.  
  
**Usage:**  
`SET MARGIN <new_margin>`  
`SET ZONEWIDTH <new_zonewidth>`  
  
`<new_margin>` and `<new_zonewidth>` must be integer constants designating print area margin and print zone width (see `PRINT` statement description for details).  
  
**Examples:**  
`SET MARGIN 80`  
`SET ZONEWIDTH 10`  
  
### `WHILE`, `WEND` statements  
  
`WHILE` and `WEND` statements are used to create loops executing statements while a logical expression evaluates as `TRUE`.  
  
**Usage:**  
`WHILE <logical_expr>`  
`<statement_to_repeat1>`  
` ... `  
`<statement_to_repeatN>`  
`WEND`  
  
Here `<logical_expr>` is a logical expression evaluated before every loop iteration. If the expression evaluates as `TRUE` interpreter executes code fragment between `WHILE` and `WEND` statements once and goes to the `WHILE` statement execution again. Statements within `WHILE` - `WEND` loop can include another loop called inner or nested.  
  
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
  
The statement makes the interpreter starting the next loop iteration.  
  
**Usage:**  
`CONTINUE`  
  
**Examples:**  
`REM print non-negative numbers only`  
`FOR I = 0 TO 10`  
`IF ARR(I) < 0 THEN CONTINUE`  
`PRINT ARR(I)`  
`NEXT I`  
  
### `STOP` statement  
  
The statement stops program execution. Unlike `END` statement the program execution can be resumed. Usually the statement is used to bring some debugging abilities.  
  