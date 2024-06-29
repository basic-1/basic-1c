/*
 Intermediate code compiler
 Copyright (c) 2021-2024 Nikolay Pletnev
 MIT license

 c1errors.cpp: error messages and error reporting functions
*/


#include <string>

#include "c1errors.h"


static const std::string c1_err_msgs[] =
{
	"invalid token",
	"program line too long",
	"invalid line number",
	"invalid statement",
	"invalid argument",
	"expression too long",
	"missing comma or bracket",
	"unbalanced brackets",
	"wrong argument count",
	"expression evaluation temporary stack overflow",
	"unknown syntax error",
	"wrong argument type",
	"not enough memory",
	"invalid memory block descriptor",
	"buffer too small",
	"string too long",
	"too many open brackets",
	"unknown identifier",
	"wrong subscript count",
	"type mismatch",
	"subscript out of range",
	"identifier already in use",
	"integer divide by zero",
	"nested IF statement not allowed",
	"ELSE without IF",
	"line number not found",
	"statement stack overflow",
	"statement stack underflow",
	"can't use the reserved word in this context",
	"not a variable",
	"environment fatal error",
	"unexpected RETURN statement",
	"unexpected end of program",
	"the end of DATA block reached",
	"WEND without WHILE",
	"NEXT without FOR",
	"FOR without NEXT",
	"can't use subscripted variable as FOR loop control variable",
	"invalid number",
	"numeric overflow",
	"too many DEF statements",
	"user functions stack overflow",
	"end of file",
	"use of a reserved keyword as identifer forbidden",
	"WHILE without WEND",
	"BREAK or CONTINUE statement not within a loop",
	"too many breakpoints",

	"file open error",
	"input file is empty",
	"file write error",
	"invalid label name",
	"invalid command name",
	"unknown type",
	"redefining variable with different type",
	"redefining variable with different dimensions number",
	"local variable redefined",
	"user function redefined",
	"internal error",
	"stack overflow",
	"stack failure",
	"no data",
	"variable redefined",
	"unknown instruction",
	"conditional jump without compare operator",
	"unresolved symbol",
	"not enough memory",
	"recursive inline",
	"no default IO device specified",
	"unknown IO device",
	"unknown interrupt name",
	"multiple handlers for a single interrupt",
	"wrong device type",
	"",
	"wrong optimization log file format",

	"the last message"
};

static const std::string c1_wrn_msgs[] =
{
	"possible wrong heap size",
	"possible wrong stack size",
	"possible stack overflow",
	"",
	"unknown MCU name",
	"unknown MCU extensions",

	"the last message"
};


void c1_print_error(C1_T_ERROR err_code, int line_cnt, const std::string &file_name, bool print_err_desc)
{
	if(!file_name.empty())
	{
		fprintf(stderr, "%s: ", file_name.c_str());
	}

	int err_ind = static_cast<std::underlying_type_t<C1_T_ERROR>>(err_code);

	fprintf(stderr, "error: %d", err_ind);
	
	if(line_cnt > 0)
	{
		fprintf(stderr, " at line %d", line_cnt);
	}

	int err_fst = static_cast<int>(B1_RES_FIRSTERRCODE);
	int err_lst = static_cast<std::underlying_type_t<C1_T_ERROR>>(C1_T_ERROR::C1_RES_LASTERRCODE);

	if(print_err_desc && err_ind >= err_fst && err_ind < err_lst)
	{
		fprintf(stderr, " (%s)", c1_err_msgs[err_ind - err_fst].c_str());
	}

	fputs("\n", stderr);
}

void c1_print_warning(C1_T_WARNING wrn_code, int line_cnt, const std::string &file_name, bool print_wrn_desc)
{
	if(!file_name.empty())
	{
		fprintf(stderr, "%s: ", file_name.c_str());
	}

	int wrn_ind = static_cast<std::underlying_type_t<C1_T_WARNING>>(wrn_code);

	fprintf(stderr, "warning: %d", wrn_ind);

	if(line_cnt > 0)
	{
		fprintf(stderr, " at line %d", line_cnt);
	}

	int wrn_fst = static_cast<std::underlying_type_t<C1_T_WARNING>>(C1_T_WARNING::C1_WRN_FIRSTWRNCODE);
	int wrn_lst = static_cast<std::underlying_type_t<C1_T_WARNING>>(C1_T_WARNING::C1_WRN_LASTWRNCODE);

	if(print_wrn_desc && wrn_ind >= wrn_fst && wrn_ind < wrn_lst)
	{
		fprintf(stderr, " (%s)", c1_wrn_msgs[wrn_ind - wrn_fst].c_str());
	}

	fputs("\n", stderr);
}
