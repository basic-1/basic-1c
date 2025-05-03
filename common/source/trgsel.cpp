/*
 BASIC1 compiler
 Copyright (c) 2021-2025 Nikolay Pletnev
 MIT license

 trgsel.cpp: target selection
*/

#include "trgsel.h"


#ifdef B1_DEF_STD_FNS
// standard functions definition
std::vector<B1_CMP_FN> B1_CMP_FNS::_fns =
{
	// the last empty record, used to get records number
	B1_CMP_FN(L"",			B1Types::B1T_UNKNOWN,	std::initializer_list<B1Types>(),	L"")
};
#endif


std::string get_c1_compiler_name(const Settings &settings)
{
#if defined(B1_TARGET_STM8) || defined(B1_TARGET_ALL)
	if(settings.GetTargetName() == "STM8")
	{
		return "c1stm8";
	}
#endif

	return std::string();
}

bool select_target(Settings &settings)
{
#if defined(B1_TARGET_STM8) || defined(B1_TARGET_ALL)
	if(settings.GetTargetName() == "STM8")
	{
		// default values: 2 kB of RAM, 16 kB of FLASH, 256 bytes of stack
		settings.Init(0x0, 0x0800, 0x8000, 0x4000, 0x100, 0x0, (settings.GetMemModelSmall() ? STM8_RET_ADDR_SIZE_MM_SMALL : (STM8_RET_ADDR_SIZE_MM_LARGE)));

#ifdef B1_DEF_STD_FNS
		B1_CMP_FNS::_fns =
		{
			//			name,		ret. type				arg. types(def. values)				fn. name in std. library
			// standard functions
			B1_CMP_FN(L"LEN",		B1Types::B1T_BYTE,		{ B1Types::B1T_STRING },			L"__LIB_STR_LEN"),
			B1_CMP_FN(L"ASC",		B1Types::B1T_BYTE,		{ B1Types::B1T_STRING },			L"__LIB_STR_ASC"),
			B1_CMP_FN(L"CHR$",		B1Types::B1T_STRING,	{ B1Types::B1T_BYTE },				L"__LIB_STR_CHR"),
			B1_CMP_FN(L"STR$",		B1Types::B1T_STRING,	{ B1Types::B1T_INT },				L"__LIB_STR_STR_I"),
			B1_CMP_FN(L"STR$",		B1Types::B1T_STRING,	{ B1Types::B1T_WORD },				L"__LIB_STR_STR_W"),
			B1_CMP_FN(L"STR$",		B1Types::B1T_STRING,	{ B1Types::B1T_LONG },				L"__LIB_STR_STR_L"),
			B1_CMP_FN(L"VAL",		B1Types::B1T_INT,		{ B1Types::B1T_STRING },			L"__LIB_STR_CINT"),
			B1_CMP_FN(L"CBYTE",		B1Types::B1T_BYTE,		{ B1Types::B1T_STRING },			L"__LIB_STR_CBYTE"),
			B1_CMP_FN(L"CINT",		B1Types::B1T_INT,		{ B1Types::B1T_STRING },			L"__LIB_STR_CINT"),
			B1_CMP_FN(L"CWRD",		B1Types::B1T_WORD,		{ B1Types::B1T_STRING },			L"__LIB_STR_CWRD"),
			B1_CMP_FN(L"CLNG",		B1Types::B1T_LONG,		{ B1Types::B1T_STRING },			L"__LIB_STR_CLNG"),
			B1_CMP_FN(L"MID$",		B1Types::B1T_STRING,	{ B1_CMP_FN_ARG(B1Types::B1T_STRING), B1_CMP_FN_ARG(B1Types::B1T_BYTE), B1_CMP_FN_ARG(B1Types::B1T_BYTE, true, std::to_wstring(B1C_T_CONST::B1C_MAX_STR_LEN)) }, L"__LIB_STR_MID"),
			B1_CMP_FN(L"INSTR",		B1Types::B1T_BYTE,		{ B1_CMP_FN_ARG(B1Types::B1T_BYTE, true, L"1"), B1_CMP_FN_ARG(B1Types::B1T_STRING), B1_CMP_FN_ARG(B1Types::B1T_STRING) }, L"__LIB_STR_INS"),
			B1_CMP_FN(L"LTRIM$",	B1Types::B1T_STRING,	{ B1Types::B1T_STRING },			L"__LIB_STR_LTRIM"),
			B1_CMP_FN(L"RTRIM$",	B1Types::B1T_STRING,	{ B1Types::B1T_STRING },			L"__LIB_STR_RTRIM"),
			B1_CMP_FN(L"LEFT$",		B1Types::B1T_STRING,	{ B1Types::B1T_STRING, B1Types::B1T_BYTE },	L"__LIB_STR_LEFT"),
			B1_CMP_FN(L"RIGHT$",	B1Types::B1T_STRING,	{ B1Types::B1T_STRING, B1Types::B1T_BYTE },	L"__LIB_STR_RIGHT"),
			B1_CMP_FN(L"LSET$",		B1Types::B1T_STRING,	{ B1Types::B1T_STRING, B1Types::B1T_BYTE },	L"__LIB_STR_LSET"),
			B1_CMP_FN(L"RSET$",		B1Types::B1T_STRING,	{ B1Types::B1T_STRING, B1Types::B1T_BYTE },	L"__LIB_STR_RSET"),
			B1_CMP_FN(L"UCASE$",	B1Types::B1T_STRING,	{ B1Types::B1T_STRING },			L"__LIB_STR_UCASE"),
			B1_CMP_FN(L"LCASE$",	B1Types::B1T_STRING,	{ B1Types::B1T_STRING },			L"__LIB_STR_LCASE"),
			B1_CMP_FN(L"SET$",		B1Types::B1T_STRING,	{ B1_CMP_FN_ARG(B1Types::B1T_STRING, true, L"\" \"") , B1_CMP_FN_ARG(B1Types::B1T_BYTE) },	L"__LIB_STR_SET"),

			// inline functions
			B1_CMP_FN(L"ABS",		B1Types::B1T_LONG,		{ B1Types::B1T_LONG },				L""),
			B1_CMP_FN(L"ABS",		B1Types::B1T_WORD,		{ B1Types::B1T_INT },				L""),
			B1_CMP_FN(L"ABS",		B1Types::B1T_WORD,		{ B1Types::B1T_WORD },				L""),
			B1_CMP_FN(L"ABS",		B1Types::B1T_BYTE,		{ B1Types::B1T_BYTE },				L""),
			B1_CMP_FN(L"SGN",		B1Types::B1T_INT,		{ B1Types::B1T_LONG },				L""),
			B1_CMP_FN(L"SGN",		B1Types::B1T_INT,		{ B1Types::B1T_INT },				L""),
			B1_CMP_FN(L"SGN",		B1Types::B1T_BYTE,		{ B1Types::B1T_WORD },				L""),
			B1_CMP_FN(L"SGN",		B1Types::B1T_BYTE,		{ B1Types::B1T_BYTE },				L""),
			B1_CMP_FN(L"STR$",		B1Types::B1T_STRING,	{ B1Types::B1T_STRING },			L""),
			B1_CMP_FN(L"CBYTE",		B1Types::B1T_BYTE,		{ B1Types::B1T_BYTE },				L""),
			B1_CMP_FN(L"CBYTE",		B1Types::B1T_BYTE,		{ B1Types::B1T_INT },				L""),
			B1_CMP_FN(L"CBYTE",		B1Types::B1T_BYTE,		{ B1Types::B1T_WORD },				L""),
			B1_CMP_FN(L"CBYTE",		B1Types::B1T_BYTE,		{ B1Types::B1T_LONG },				L""),
			B1_CMP_FN(L"CINT",		B1Types::B1T_INT,		{ B1Types::B1T_BYTE },				L""),
			B1_CMP_FN(L"CINT",		B1Types::B1T_INT,		{ B1Types::B1T_INT },				L""),
			B1_CMP_FN(L"CINT",		B1Types::B1T_INT,		{ B1Types::B1T_WORD },				L""),
			B1_CMP_FN(L"CINT",		B1Types::B1T_INT,		{ B1Types::B1T_LONG },				L""),
			B1_CMP_FN(L"CWRD",		B1Types::B1T_WORD,		{ B1Types::B1T_BYTE },				L""),
			B1_CMP_FN(L"CWRD",		B1Types::B1T_WORD,		{ B1Types::B1T_INT },				L""),
			B1_CMP_FN(L"CWRD",		B1Types::B1T_WORD,		{ B1Types::B1T_WORD },				L""),
			B1_CMP_FN(L"CWRD",		B1Types::B1T_WORD,		{ B1Types::B1T_LONG },				L""),
			B1_CMP_FN(L"CLNG",		B1Types::B1T_LONG,		{ B1Types::B1T_BYTE },				L""),
			B1_CMP_FN(L"CLNG",		B1Types::B1T_LONG,		{ B1Types::B1T_INT },				L""),
			B1_CMP_FN(L"CLNG",		B1Types::B1T_LONG,		{ B1Types::B1T_WORD },				L""),
			B1_CMP_FN(L"CLNG",		B1Types::B1T_LONG,		{ B1Types::B1T_LONG },				L""),

			// special PRINT statement functions
			B1_CMP_FN(L"TAB",		B1Types::B1T_STRING,	{ B1Types::B1T_BYTE },				L""),
			B1_CMP_FN(L"SPC",		B1Types::B1T_STRING,	{ B1Types::B1T_BYTE },				L""),
			B1_CMP_FN(L"NL",		B1Types::B1T_STRING,	std::initializer_list<B1Types>(),	L""),

			B1_CMP_FN(L"XORIN",		B1Types::B1T_BYTE,		{ B1Types::B1T_BYTE },				L""),
			B1_CMP_FN(L"XOROUT",	B1Types::B1T_BYTE,		{ B1Types::B1T_BYTE },				L""),

			// the last empty record, used to get records number
			B1_CMP_FN(L"",			B1Types::B1T_UNKNOWN,	std::initializer_list<B1Types>(),	L"")
		};
#endif

		return true;
	}
#endif

	return false;
}
