/*
 A1 assembler
 Copyright (c) 2021-2024 Nikolay Pletnev
 MIT license

 a1errors.cpp: error messages and error reporting functions
*/


#include <string>

#include "a1errors.h"


static const std::string err_msgs[] =
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

	"end of file",
	"file open error",
	"file read error",
	"file write error",
	"syntax error",
	"invalid number",
	"wrong address",
	"numeric overflow",
	"unresolved symbol",
	"wrong section size",
	"wrong section name",
	"wrong statement size",
	"duplicate symbol",
	"relative offset out of range",
	"invalid reference type",
	"invalid instruction",
	"wrong data block size",
	"file close error",
	".ERROR: ",
	"internal error",

	"the last message"
};

static const std::string wrn_msgs[] =
{
	"integer out of range",
	"address out of range",
	"relative address out of range",
	"data truncated",
	"unknown MCU name",
	"more than one .CODE INIT section",
	"more than one .STACK section",
	"stack, heap and data sections size exceeds the overall RAM size",
	"more than one .HEAP section",
	"invalid wide character",
	"non-ANSI character",

	"the last message"
};


void a1_print_error(A1_T_ERROR err_code, int line_cnt, const std::string &file_name, bool print_err_desc, const std::string &custom_err_msg)
{
	if(!file_name.empty())
	{
		fprintf(stderr, "%s: ", file_name.c_str());
	}

	int err_ind = static_cast<std::underlying_type_t<A1_T_ERROR>>(err_code);

	fprintf(stderr, "error: %d", err_ind);
	
	if(line_cnt > 0)
	{
		fprintf(stderr, " at line %d", line_cnt);
	}

	int err_fst = static_cast<int>(B1_RES_FIRSTERRCODE);
	int err_lst = static_cast<std::underlying_type_t<A1_T_ERROR>>(A1_T_ERROR::A1_RES_LASTERRCODE);

	if(print_err_desc && err_ind >= err_fst && err_ind < err_lst)
	{
		const std::string msg = err_msgs[err_ind - err_fst].c_str() + ((err_code == A1_T_ERROR::A1_RES_EERRDIR) ? custom_err_msg : "");
		fprintf(stderr, " (%s)", msg.c_str());
	}

	fputs("\n", stderr);
}

void a1_print_warning(A1_T_WARNING wrn_code, int line_cnt, const std::string &file_name, bool print_wrn_desc)
{
	if(!file_name.empty())
	{
		fprintf(stderr, "%s: ", file_name.c_str());
	}

	int wrn_ind = static_cast<std::underlying_type_t<A1_T_WARNING>>(wrn_code);

	fprintf(stderr, "warning: %d", wrn_ind);

	if(line_cnt > 0)
	{
		fprintf(stderr, " at line %d", line_cnt);
	}

	int wrn_fst = static_cast<std::underlying_type_t<A1_T_WARNING>>(A1_T_WARNING::A1_WRN_FIRSTWRNCODE);
	int wrn_lst = static_cast<std::underlying_type_t<A1_T_WARNING>>(A1_T_WARNING::A1_WRN_LASTWRNCODE);

	if(print_wrn_desc && wrn_ind >= wrn_fst && wrn_ind < wrn_lst)
	{
		fprintf(stderr, " (%s)", wrn_msgs[wrn_ind - wrn_fst].c_str());
	}

	fputs("\n", stderr);
}
