/*
 STM8 intermediate code compiler
 Copyright (c) 2020-2023 Nikolay Pletnev
 MIT license

 feat.h: compiler features
*/


#ifndef _B1_FEATURES_
#define _B1_FEATURES_


// ERASE statement
#define B1_FEATURE_STMT_ERASE

// DATA, READ and RESTORE statements
#define B1_FEATURE_STMT_DATA_READ

// LEN, ASC, CHR$, STR$, VAL, IIF, IIF$ functions
#define B1_FEATURE_FUNCTIONS_STANDARD

// ABS, INT, RND, SGN functions and RANDOMIZE statement
//#define B1_FEATURE_FUNCTIONS_MATH_BASIC

// ATN, COS, EXP, LOG, PI, SIN, SQR, TAN functions
//#define B1_FEATURE_FUNCTIONS_MATH_EXTRA

// MID$, INSTR, LTRIM$, RTRIM$, LEFT$, RIGHT$, LSET$, RSET$, UCASE$, LCASE$ functions
#define B1_FEATURE_FUNCTIONS_STRING

// DEF statement and user defined functions
#define B1_FEATURE_FUNCTIONS_USER

// enable SINGLE type
//#define B1_FEATURE_TYPE_SINGLE

// enable DOUBLE type
//#define B1_FEATURE_TYPE_DOUBLE

// enable RPN caching
//#define B1_FEATURE_RPN_CACHING

// locales support (for LCASE$, UCASE$, INSTR functions and string comparison operators)
#define B1_FEATURE_LOCALES

// enables two byte B1_T_CHAR type for Unicode (UCS-2) support
#define B1_FEATURE_UNICODE_UCS2

// type size for array subscripts (if no one macro is enabled 16 bit type is used)
//#define B1_FEATURE_SUBSCRIPT_8BIT
//#define B1_FEATURE_SUBSCRIPT_12BIT
//#define B1_FEATURE_SUBSCRIPT_16BIT
//#define B1_FEATURE_SUBSCRIPT_24BIT

// type size for pointer offsets and memory block sizes (if no one macro is define 32 bit type is used)
//#define B1_FEATURE_MEMOFFSET_16BIT
//#define B1_FEATURE_MEMOFFSET_32BIT

// if enabled b1_int_reset function frees memory allocated during preceding program execution
// in the most cases the feature must be enabled to allow resetting program state
#define B1_FEATURE_INIT_FREE_MEMORY

// enables storing identifier names (not only hashes) in B1_ID structure, allows reading variables
// created during program execution, makes b1_dbg_* functions and variables available
#define B1_FEATURE_DEBUG

// forbid using statement keywords as variable names
#define B1_FEATURE_CHECK_KEYWORDS

// enables WHILE and WEND statements
#define B1_FEATURE_STMT_WHILE_WEND

// enable three-dimensional arrays
#define B1_FEATURE_3_DIM_ARRAYS

// enables minimal (short-circuit) evaluation of expresions passed to IIF and IIF$ functions
// (depends on the logical expression evaluation result)
#define B1_FEATURE_MINIMAL_EVALUATION

// BREAK and CONTINUE statements
#define B1_FEATURE_STMT_BREAK_CONTINUE

// STOP statement
#define B1_FEATURE_STMT_STOP

// enables three small integer types: INT16 (16-bit signed integer), WORD (16-bit unsigned
// integer) and BYTE (8-bit unsigned integer)
#define B1_FEATURE_TYPE_SMALL

// enables hexadecimal form of numerics (value has to be preceded by 0x prefix, e.g. 0x10)
#define B1_FEATURE_HEX_NUM

// enables IOCTL statement
#define B1_FEATURE_STMT_IOCTL

// allow underscore characters in identifiers
#define B1_FEATURE_UNDERSCORE_ID


// constraints and memory usage
#define B1_MAX_PROGLINE_LEN 255
#define B1_MAX_STRING_LEN (B1_MAX_PROGLINE_LEN / 2)
#define B1_MAX_LINE_NUM_LEN 5
#define B1_MAX_FN_ARGS_NUM 5
#define B1_MAX_IDENTIFIER_LEN 31
#ifdef B1_FEATURE_3_DIM_ARRAYS
#define B1_MAX_VAR_DIM_NUM 3
#else
#define B1_MAX_VAR_DIM_NUM 2
#endif
#define B1_DEF_SUBSCRIPT_UBOUND 10
#define B1_MAX_RPN_LEN 64
#define B1_MAX_RPN_EVAL_BUFFER_LEN 12
#define B1_MAX_RPN_BRACK_NEST_DEPTH 7

#ifdef B1_FEATURE_FUNCTIONS_USER
#define B1_MAX_UDEF_FN_RPN_LEN (768 / sizeof(B1_RPNREC))
#define B1_MAX_UDEF_CALL_NEST_DEPTH 3
#endif

#ifdef B1_FEATURE_DEBUG
#define B1_MAX_BREAKPOINT_NUM 32
#endif

#endif
