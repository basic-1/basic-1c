/*
 STM8 intermediate code compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 errors.h: error codes definition and function prototypes
*/


#pragma once

#include <string>

extern "C"
{
#include "b1err.h"
}


enum class C1STM8_T_ERROR
{
	C1STM8_RES_OK = B1_RES_OK,

	C1STM8_RES_FIRSTERRCODE = B1_RES_LASTERRCODE + 1,

	C1STM8_RES_EFOPEN = C1STM8_RES_FIRSTERRCODE,
	C1STM8_RES_EIFEMPTY,
	C1STM8_RES_EFWRITE,
	C1STM8_RES_EINVLBNAME,
	C1STM8_RES_EINVCMDNAME,
	C1STM8_RES_EINVTYPNAME,
	C1STM8_RES_EVARTYPMIS,
	C1STM8_RES_EVARDIMMIS,
	C1STM8_RES_ELCLREDEF,
	C1STM8_RES_EUFNREDEF,
	C1STM8_RES_EINTERR,
	C1STM8_RES_ESTCKOVF,
	C1STM8_RES_ESTKFAIL,
	C1STM8_RES_ENODATA,
	C1STM8_RES_EVARREDEF,
	C1STM8_RES_EUNKINST,
	C1STM8_RES_ENOCMPOP,
	C1STM8_RES_EUNRESSYMBOL,
	C1STM8_RES_ENOMEM,
	C1STM8_RES_ERECURINL,
	C1STM8_RES_ENODEFIODEV,
	C1STM8_RES_EUNKIODEV,

	C1STM8_RES_LASTERRCODE
};

enum class C1STM8_T_WARNING
{
	C1STM8_WRN_FIRSTWRNCODE = 100,

	C1STM8_WRN_WWRNGHEAPSIZE = C1STM8_WRN_FIRSTWRNCODE,
	C1STM8_WRN_WWRNGSTKSIZE,
	C1STM8_WRN_RESERVED0,
	C1STM8_WRN_RESERVED1,
	C1STM8_WRN_WUNKNMCU,

	C1STM8_WRN_LASTWRNCODE
};


extern void c1stm8_print_error(C1STM8_T_ERROR err_code, int line_cnt, const std::string &file_name, bool print_err_desc);
extern void c1stm8_print_warning(C1STM8_T_WARNING wrn_code, int line_cnt, const std::string &file_name, bool print_wrn_desc);
