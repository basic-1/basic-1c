/*
 A1 assembler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 a1errors.h: error codes definition and function prototypes
*/


#pragma once

extern "C"
{
#include "b1err.h"
}


enum class A1_T_ERROR
{
	A1_RES_OK = B1_RES_OK,

	A1_RES_FIRSTERRCODE = B1_RES_LASTERRCODE + 1,

	A1_RES_EEOF = A1_RES_FIRSTERRCODE,
	A1_RES_EFOPEN,
	A1_RES_EFREAD,
	A1_RES_EFWRITE,
	A1_RES_ESYNTAX,
	A1_RES_EINVNUM,
	A1_RES_EWADDR,
	A1_RES_ENUMOVF,
	A1_RES_EUNRESSYMB,
	A1_RES_EWSECSIZE,
	A1_RES_EWSECNAME,
	A1_RES_EWSTMTSIZE,
	A1_RES_EDUPSYM,
	A1_RES_ERELOUTRANGE,
	A1_RES_EINVREFTYPE,
	A1_RES_EINVINST,
	A1_RES_EWBLKSIZE,
	A1_RES_EFCLOSE,
	A1_RES_EERRDIR,
	A1_RES_EINTERR,

	A1_RES_LASTERRCODE
};

enum class A1_T_WARNING
{
	A1_WRN_FIRSTWRNCODE = 100,

	A1_WRN_WINTOUTRANGE = A1_WRN_FIRSTWRNCODE,
	A1_WRN_WADDROUTRANGE,
	A1_WRN_WOFFOUTRANGE,
	A1_WRN_WDATATRUNC,
	A1_WRN_WUNKNMCU,
	A1_WRN_WMANYCODINIT,
	A1_WRN_WMANYSTKSECT,
	A1_WRN_EWNORAM,
	A1_WRN_WMANYHPSECT,

	A1_WRN_LASTWRNCODE
};


extern void a1_print_error(A1_T_ERROR err_code, int line_cnt, const std::string &file_name, bool print_err_desc, const std::string &custom_err_msg = std::string());
extern void a1_print_warning(A1_T_WARNING wrn_code, int line_cnt, const std::string &file_name, bool print_wrn_desc);
