/*
 BASIC1 compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 errors.h: BASIC1 error codes definition and function prototypes
*/


#pragma once

#include <string>

extern "C"
{
#include "b1err.h"
}


enum class B1C_T_ERROR
{
	B1C_RES_OK = B1_RES_OK,

	B1C_RES_FIRSTERRCODE = B1_RES_LASTERRCODE + 1,

	B1C_RES_EFOPEN = B1C_RES_FIRSTERRCODE,
	B1C_RES_EFWRITE,
	B1C_RES_EVARTYPMIS,
	B1C_RES_EVARDIMMIS,
	B1C_RES_EINCOPTS,
	B1C_RES_ENOTIMP,
	B1C_RES_EUNKIODEV,
	B1C_RES_EWDEVTYPE,
	B1C_RES_EUNKDEVCMD,
	B1C_RES_ECNSTVOLVAR,
	B1C_RES_ENCNSTINIT,
	B1C_RES_ECNSTADDR,
	B1C_RES_ECNSTNOINIT,

	B1C_RES_LASTERRCODE
};

enum class B1C_T_WARNING
{
	B1C_WRN_FIRSTWRNCODE = 100,

	B1C_WRN_WMULTEND = B1C_WRN_FIRSTWRNCODE,
	B1C_WRN_WOPTEXPLEN,
	B1C_WRN_WOPTBASE1EN,
	B1C_WRN_WOPTNOCHKEN,
	B1C_WRN_WUNKNMCU,
	B1C_WRN_WSTATNONSUBVAR,
	B1C_WRN_WCNSTALSTAT,

	B1C_WRN_LASTWRNCODE
};


extern void b1c_print_error(B1C_T_ERROR err_code, int line_cnt, const std::string &file_name, bool print_err_desc);
extern void b1c_print_warning(B1C_T_WARNING wrn_code, int line_cnt, const std::string &file_name, bool print_wrn_desc);
