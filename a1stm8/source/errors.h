/*
 STM8 assembler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 errors.h: STM8 error codes definition and function prototypes
*/


#pragma once

extern "C"
{
#include "b1err.h"
}


enum class A1STM8_T_ERROR
{
	A1STM8_RES_OK = B1_RES_OK,

	A1STM8_RES_FIRSTERRCODE = B1_RES_LASTERRCODE + 1,

	A1STM8_RES_EEOF = A1STM8_RES_FIRSTERRCODE,
	A1STM8_RES_EFOPEN,
	A1STM8_RES_EFREAD,
	A1STM8_RES_EFWRITE,
	A1STM8_RES_ESYNTAX,
	A1STM8_RES_EINVNUM,
	A1STM8_RES_EWADDR,
	A1STM8_RES_ENUMOVF,
	A1STM8_RES_EUNRESSYMB,
	A1STM8_RES_EWSECSIZE,
	A1STM8_RES_ENOSEC,
	A1STM8_RES_EWSTMTSIZE,
	A1STM8_RES_EDUPSYM,
	A1STM8_RES_ERELOUTRANGE,
	A1STM8_RES_EINVREFTYPE,
	A1STM8_RES_EINVINST,
	A1STM8_RES_EWBLKSIZE,
	A1STM8_RES_EFCLOSE,
	A1STM8_RES_EERRDIR,

	A1STM8_RES_LASTERRCODE
};

enum class A1STM8_T_WARNING
{
	A1STM8_WRN_FIRSTWRNCODE = 100,

	A1STM8_WRN_WINTOUTRANGE = A1STM8_WRN_FIRSTWRNCODE,
	A1STM8_WRN_WADDROUTRANGE,
	A1STM8_WRN_WOFFOUTRANGE,
	A1STM8_WRN_WDATATRUNC,
	A1STM8_WRN_WUNKNMCU,
	A1STM8_WRN_WMANYCODINIT,
	A1STM8_WRN_WMANYSTKSECT,
	A1STM8_WRN_EWNORAM,
	A1STM8_WRN_WMANYHPSECT,

	A1STM8_WRN_LASTWRNCODE
};


extern void a1stm8_print_error(A1STM8_T_ERROR err_code, int line_cnt, const std::string &file_name, bool print_err_desc, const std::string &custom_err_msg = std::string());
extern void a1stm8_print_warning(A1STM8_T_WARNING wrn_code, int line_cnt, const std::string &file_name, bool print_wrn_desc);
