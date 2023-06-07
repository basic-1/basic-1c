/*
 BASIC1 compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 moresym.cpp: common symbols and constants
*/

#include "moresym.h"


std::map<B1C_T_RTERROR, std::wstring> _RTE_error_names =
{
	{ B1C_T_RTERROR::B1C_RTE_OK, L"B1C_RTE_OK" },
	{ B1C_T_RTERROR::B1C_RTE_ARR_ALLOC, L"B1C_RTE_ARR_ALLOC" },
	{ B1C_T_RTERROR::B1C_RTE_STR_TOOLONG, L"B1C_RTE_STR_TOOLONG" },
	{ B1C_T_RTERROR::B1C_RTE_MEM_NOT_ENOUGH, L"B1C_RTE_MEM_NOT_ENOUGH" },
	{ B1C_T_RTERROR::B1C_RTE_STR_WRONG_INDEX, L"B1C_RTE_STR_WRONG_INDEX" },
	{ B1C_T_RTERROR::B1C_RTE_STR_INV_NUM, L"B1C_RTE_STR_INV_NUM" },
	{ B1C_T_RTERROR::B1C_RTE_ARR_UNALLOC, L"B1C_RTE_ARR_UNALLOC" },
	{ B1C_T_RTERROR::B1C_RTE_COM_WRONG_INDEX, L"B1C_RTE_COM_WRONG_INDEX" },
	{ B1C_T_RTERROR::B1C_RTE_STR_INVALID, L"B1C_RTE_STR_INVALID" },
};

std::map<std::wstring, B1C_T_RTERROR> _RTE_errors =
{
	std::make_pair(L"B1C_RTE_OK", B1C_T_RTERROR::B1C_RTE_OK),
	std::make_pair(L"B1C_RTE_ARR_ALLOC", B1C_T_RTERROR::B1C_RTE_ARR_ALLOC),
	std::make_pair(L"B1C_RTE_STR_TOOLONG", B1C_T_RTERROR::B1C_RTE_STR_TOOLONG),
	std::make_pair(L"B1C_RTE_MEM_NOT_ENOUGH", B1C_T_RTERROR::B1C_RTE_MEM_NOT_ENOUGH),
	std::make_pair(L"B1C_RTE_STR_WRONG_INDEX", B1C_T_RTERROR::B1C_RTE_STR_WRONG_INDEX),
	std::make_pair(L"B1C_RTE_STR_INV_NUM", B1C_T_RTERROR::B1C_RTE_STR_INV_NUM),
	std::make_pair(L"B1C_RTE_ARR_UNALLOC", B1C_T_RTERROR::B1C_RTE_ARR_UNALLOC),
	std::make_pair(L"B1C_RTE_COM_WRONG_INDEX", B1C_T_RTERROR::B1C_RTE_COM_WRONG_INDEX),
	std::make_pair(L"B1C_RTE_STR_INVALID", B1C_T_RTERROR::B1C_RTE_STR_INVALID),
};


std::map<int32_t, std::wstring> _B1C_const_names =
{
	{ B1C_T_CONST::B1C_MAX_STR_LEN, L"B1C_MAX_STR_LEN" },
};

std::map<std::wstring, int32_t> _B1C_consts =
{
	std::make_pair(L"B1C_MAX_STR_LEN", B1C_T_CONST::B1C_MAX_STR_LEN),
};