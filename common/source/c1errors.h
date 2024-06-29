/*
 Intermediate code compiler
 Copyright (c) 2021-2024 Nikolay Pletnev
 MIT license

 c1errors.h: error codes definition and function prototypes
*/


#pragma once

#include <string>

extern "C"
{
#include "b1err.h"
}


enum class C1_T_ERROR
{
	C1_RES_OK = B1_RES_OK,

	C1_RES_FIRSTERRCODE = B1_RES_LASTERRCODE + 1,

	C1_RES_EFOPEN = C1_RES_FIRSTERRCODE,
	C1_RES_EIFEMPTY,
	C1_RES_EFWRITE,
	C1_RES_EINVLBNAME,
	C1_RES_EINVCMDNAME,
	C1_RES_EINVTYPNAME,
	C1_RES_EVARTYPMIS,
	C1_RES_EVARDIMMIS,
	C1_RES_ELCLREDEF,
	C1_RES_EUFNREDEF,
	C1_RES_EINTERR,
	C1_RES_ESTCKOVF,
	C1_RES_ESTKFAIL,
	C1_RES_ENODATA,
	C1_RES_EVARREDEF,
	C1_RES_EUNKINST,
	C1_RES_ENOCMPOP,
	C1_RES_EUNRESSYMBOL,
	C1_RES_ENOMEM,
	C1_RES_ERECURINL,
	C1_RES_ENODEFIODEV,
	C1_RES_EUNKIODEV,
	C1_RES_EUNKINT,
	C1_RES_EMULTINTHND,
	C1_RES_EWDEVTYPE,
	C1_RES_ENOIMMOFF,
	C1_RES_EWOPTLOGFMT,

	C1_RES_LASTERRCODE
};

enum class C1_T_WARNING
{
	C1_WRN_FIRSTWRNCODE = 100,

	C1_WRN_WWRNGHEAPSIZE = C1_WRN_FIRSTWRNCODE,
	C1_WRN_WWRNGSTKSIZE,
	C1_WRN_WRETSTKOVF,
	C1_WRN_RESERVED1,
	C1_WRN_WUNKNMCU,
	C1_WRN_WUNKMCUEX,

	C1_WRN_LASTWRNCODE
};


extern void c1_print_error(C1_T_ERROR err_code, int line_cnt, const std::string &file_name, bool print_err_desc);
extern void c1_print_warning(C1_T_WARNING wrn_code, int line_cnt, const std::string &file_name, bool print_wrn_desc);
