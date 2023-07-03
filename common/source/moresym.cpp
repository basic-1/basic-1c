/*
 BASIC1 compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 moresym.cpp: common symbols and constants
*/

#include "moresym.h"


// run-time errors
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


// constants and limits
std::map<std::wstring, std::pair<int32_t, B1Types>> _B1C_consts =
{
	// max. string length
	std::make_pair(L"B1C_MAX_STR_LEN", std::make_pair((int32_t)B1C_T_CONST::B1C_MAX_STR_LEN, B1Types::B1T_BYTE)),
};


// assemply-time constant names (their values are computed at assembly time)
std::map<std::wstring, B1Types> _B1AT_consts =
{
	std::make_pair(L"__RET_ADDR_SIZE", B1Types::B1T_BYTE),
	std::make_pair(L"__STACK_START", B1Types::B1T_WORD),
	std::make_pair(L"__STACK_SIZE", B1Types::B1T_WORD),
	std::make_pair(L"__HEAP_START", B1Types::B1T_WORD),
	std::make_pair(L"__HEAP_SIZE", B1Types::B1T_WORD),
	std::make_pair(L"__DATA_START", B1Types::B1T_WORD),
	std::make_pair(L"__DATA_SIZE", B1Types::B1T_WORD),
	std::make_pair(L"__DATA_TOTAL_SIZE", B1Types::B1T_WORD),
	std::make_pair(L"__INIT_START", B1Types::B1T_LONG),
	std::make_pair(L"__INIT_SIZE", B1Types::B1T_LONG),
	std::make_pair(L"__CONST_START", B1Types::B1T_LONG),
	std::make_pair(L"__CONST_SIZE", B1Types::B1T_LONG),
	std::make_pair(L"__CODE_START", B1Types::B1T_LONG),
	std::make_pair(L"__CODE_SIZE", B1Types::B1T_LONG),
	std::make_pair(L"__CODE_TOTAL_SIZE", B1Types::B1T_LONG),
};