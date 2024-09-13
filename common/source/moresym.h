/*
 BASIC1 compiler
 Copyright (c) 2021-2024 Nikolay Pletnev
 MIT license

 moresym.h: common symbols and constants (classes and variables declaration)
*/


#pragma once

#include <string>
#include <map>

#include "Utils.h"


#define RTE_ERROR_TYPE (B1Types::B1T_BYTE)

// IO device supports text mode (PRINT, INPUT)
#define B1C_DEV_OPT_TXT L"TXT"
// IO device supports binary mode (PUT, GET, TRANSFER)
#define B1C_DEV_OPT_BIN L"BIN"
// binary statements are implemented as inline code (not subroutines)
#define B1C_DEV_OPT_INL L"INL"
#define B1C_DEV_OPT_IN L"IN"
#define B1C_DEV_OPT_OUT L"OUT"


// run-time errors
enum class B1C_T_RTERROR
{
	B1C_RTE_OK = 0,
	B1C_RTE_ARR_ALLOC, // the array is already allocated
	B1C_RTE_STR_TOOLONG, // string is too long
	B1C_RTE_MEM_NOT_ENOUGH, // not enough memory
	B1C_RTE_STR_WRONG_INDEX, // wrong character index
	B1C_RTE_STR_INV_NUM, // invalid string representation of an integer
	B1C_RTE_ARR_UNALLOC,  // unallocated array access
	B1C_RTE_COM_WRONG_INDEX, // common wrong index error
	B1C_RTE_STR_INVALID, // invalid quoted string

};


extern std::map<B1C_T_RTERROR, std::wstring> _RTE_error_names;
extern std::map<std::wstring, B1C_T_RTERROR> _RTE_errors;


// constants and limits
class B1C_T_CONST
{
public:
	// max. string length
	static const uint8_t B1C_MAX_STR_LEN = 253;
};


extern std::map<std::wstring, std::pair<int32_t, B1Types>> _B1C_consts;


// assemply-time constant names (their values are computed at assembly time)
extern std::map<std::wstring, B1Types> _B1AT_consts;