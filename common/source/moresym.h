/*
 BASIC1 compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 moresym.h: common symbols and constants (classes and variables declaration)
*/


#pragma once

#include <string>
#include <map>


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


// constants
class B1C_T_CONST
{
public:
	static const int32_t B1C_MAX_STR_LEN = 253;
};


extern std::map<int32_t, std::wstring> _B1C_const_names;
extern std::map<std::wstring, int32_t> _B1C_consts;