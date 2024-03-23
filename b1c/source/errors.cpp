/*
 BASIC1 compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 errors.cpp: BASIC1 error messages and error reporting functions
*/


#include <string>

#include "errors.h"


static const std::string b1c_err_msgs[] =
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
	"file write error",
	"redefining variable with different type",
	"redefining variable with different dimensions number",
	"incompatible options",
	"not implemented",
	"unknown IO device",
	"wrong device type",
	"unknown IO device or command name",

	"the last message"
};

static const std::string b1c_wrn_msgs[] =
{
	"using multiple END statements is not recommended",
	"explicit variables declaration is enabled for all source files",
	"option BASE1 is enabled for all source files",
	"option NOCHECK is enabled for all source files",
	"unknown MCU name",
	"non-subscripted variables are already static",

	"the last message"
};


void b1c_print_error(B1C_T_ERROR err_code, int line_cnt, const std::string &file_name, bool print_err_desc)
{
	if(!file_name.empty())
	{
		fprintf(stderr, "%s: ", file_name.c_str());
	}

	int err_ind = static_cast<std::underlying_type_t<B1C_T_ERROR>>(err_code);

	fprintf(stderr, "error: %d", err_ind);
	
	if(line_cnt > 0)
	{
		fprintf(stderr, " at line %d", line_cnt);
	}

	int err_fst = static_cast<int>(B1_RES_FIRSTERRCODE);
	int err_lst = static_cast<std::underlying_type_t<B1C_T_ERROR>>(B1C_T_ERROR::B1C_RES_LASTERRCODE);

	if(print_err_desc && err_ind >= err_fst && err_ind < err_lst)
	{
		fprintf(stderr, " (%s)", b1c_err_msgs[err_ind - err_fst].c_str());
	}

	fputs("\n", stderr);
}

void b1c_print_warning(B1C_T_WARNING wrn_code, int line_cnt, const std::string &file_name, bool print_wrn_desc)
{
	if(!file_name.empty())
	{
		fprintf(stderr, "%s: ", file_name.c_str());
	}

	int wrn_ind = static_cast<std::underlying_type_t<B1C_T_WARNING>>(wrn_code);

	fprintf(stderr, "warning: %d", wrn_ind);

	if(line_cnt > 0)
	{
		fprintf(stderr, " at line %d", line_cnt);
	}

	int wrn_fst = static_cast<std::underlying_type_t<B1C_T_WARNING>>(B1C_T_WARNING::B1C_WRN_FIRSTWRNCODE);
	int wrn_lst = static_cast<std::underlying_type_t<B1C_T_WARNING>>(B1C_T_WARNING::B1C_WRN_LASTWRNCODE);

	if(print_wrn_desc && wrn_ind >= wrn_fst && wrn_ind < wrn_lst)
	{
		fprintf(stderr, " (%s)", b1c_wrn_msgs[wrn_ind - wrn_fst].c_str());
	}

	fputs("\n", stderr);
}
