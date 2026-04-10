/*
 BASIC1 compiler
 Copyright (c) 2021-2026 Nikolay Pletnev
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


#if defined(B1_TARGET_RV32) || defined(B1_TARGET_ALL)
B1_T_ERROR RV32Settings::ProcessNumPostfix(const std::wstring &postfix, int32_t &n) const
{
	if(postfix.length() == 3)
	{
		if((postfix[0] == L'L' || postfix[0] == L'l') && postfix[1] == L'1' && postfix[2] == L'2')
		{
			n = (n & 0xFFF) | (((n >> 11) & 1) == 1 ? 0xFFFFF000 : 0);
			return B1_RES_OK;
		}
		else
		if((postfix[0] == L'H' || postfix[0] == L'h') && postfix[1] == L'2' && postfix[2] == L'0')
		{
			n = ((n >> 12) + ((n >> 11) & 1));// & 0xFFFFF;
			return B1_RES_OK;
		}
	}

	return Settings::ProcessNumPostfix(postfix, n);
}
#endif


std::string get_c1_compiler_name(const Settings &settings)
{
#if defined(B1_TARGET_STM8) || defined(B1_TARGET_ALL)
	if(settings.GetTargetName() == "STM8")
	{
		return "c1stm8";
	}
#endif

#if defined(B1_TARGET_RV32) || defined(B1_TARGET_ALL)
	if(settings.GetTargetName() == "RV32")
	{
		return "c1rv32";
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
			B1_CMP_FN(L"DATEPART",		B1Types::B1T_INT,	{ B1Types::B1T_STRING, B1Types::B1T_BYTE },	L"__LIB_DT_DTP"),
			B1_CMP_FN(L"TIMEPART",		B1Types::B1T_INT,	{ B1Types::B1T_STRING, B1Types::B1T_BYTE },	L"__LIB_DT_DTP"),
			B1_CMP_FN(L"DATE$",		B1Types::B1T_STRING,	{ B1_CMP_FN_ARG(B1Types::B1T_STRING), B1_CMP_FN_ARG(B1Types::B1T_WORD), B1_CMP_FN_ARG(B1Types::B1T_STRING, true, L"\"-\"") }, L"__LIB_DT_DTTM"),
			B1_CMP_FN(L"TIME$",		B1Types::B1T_STRING,	{ B1_CMP_FN_ARG(B1Types::B1T_STRING), B1_CMP_FN_ARG(B1Types::B1T_WORD), B1_CMP_FN_ARG(B1Types::B1T_STRING, true, L"\":\"") }, L"__LIB_DT_DTTM"),

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

			// IOCTL function: the records are used just for keyword identification as a function,
			// it's a special function so argument types validation is performed in another way
			B1_CMP_FN(L"IOCTL",		B1Types::B1T_UNKNOWN,	{ B1Types::B1T_STRING, B1Types::B1T_STRING },	L""),
			B1_CMP_FN(L"IOCTL$",	B1Types::B1T_UNKNOWN,	{ B1Types::B1T_STRING, B1Types::B1T_STRING },	L""),

			B1_CMP_FN(L"XORIN",		B1Types::B1T_BYTE,		{ B1Types::B1T_BYTE },				L""),
			B1_CMP_FN(L"XOROUT",	B1Types::B1T_BYTE,		{ B1Types::B1T_BYTE },				L""),

			// the last empty record, used to get records number
			B1_CMP_FN(L"",			B1Types::B1T_UNKNOWN,	std::initializer_list<B1Types>(),	L"")
		};
#endif

		// YYYY - 3 * 10 + 0 = 30, YY - 1 * 10 + 2 = 12, MM - 1 * 10 + 5 = 15, DD - 1 * 10 + 8 = 18, M - 25, D - 28
		// constants for DATEPART function (b1dpYYYY stands for 4-digit year, b1dpYY - 2-digit year, b1dpMM - month, b1dpDD - day)
		_B1C_consts.emplace(L"B1DPYYYY", std::make_pair((int32_t)30, B1Types::B1T_BYTE));
		_B1C_consts.emplace(L"B1DPYY", std::make_pair((int32_t)12, B1Types::B1T_BYTE));
		_B1C_consts.emplace(L"B1DPMM", std::make_pair((int32_t)15, B1Types::B1T_BYTE));
		_B1C_consts.emplace(L"B1DPDD", std::make_pair((int32_t)18, B1Types::B1T_BYTE));

		// constants for DATE$ function (date formats)
		_B1C_consts.emplace(L"B1DFDDMMYYYY", std::make_pair((int32_t)48618, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFDDMMYY", std::make_pair((int32_t)19818, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFMMDDYYYY", std::make_pair((int32_t)48735, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFMMDDYY", std::make_pair((int32_t)19935, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFYYYYMMDD", std::make_pair((int32_t)29430, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFYYMMDD", std::make_pair((int32_t)29412, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFDMYYYY", std::make_pair((int32_t)49028, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFDMYY", std::make_pair((int32_t)20228, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFMDYYYY", std::make_pair((int32_t)49145, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFMDYY", std::make_pair((int32_t)20345, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFYYYYMD", std::make_pair((int32_t)45830, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFYYMD", std::make_pair((int32_t)45812, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFYYYY", std::make_pair((int32_t)30, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFYY", std::make_pair((int32_t)12, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFMM", std::make_pair((int32_t)15, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFDD", std::make_pair((int32_t)18, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFM", std::make_pair((int32_t)25, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1DFD", std::make_pair((int32_t)28, B1Types::B1T_WORD));

		// HH - 1 * 10 + 0 = 10, MM - 1 * 10 + 3 = 13, SS - 1 * 10 + 6 = 16, H - 20
		// constants for TIMEPART function (b1tpHH stands for hour, b1tpMM - minute, b1tpSS - second)
		_B1C_consts.emplace(L"B1TPHH", std::make_pair((int32_t)10, B1Types::B1T_BYTE));
		_B1C_consts.emplace(L"B1TPMM", std::make_pair((int32_t)13, B1Types::B1T_BYTE));
		_B1C_consts.emplace(L"B1TPSS", std::make_pair((int32_t)16, B1Types::B1T_BYTE));

		// constants for TIME$ function (time formats)
		_B1C_consts.emplace(L"B1TFHHMMSS", std::make_pair((int32_t)26130, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1TFHHMM", std::make_pair((int32_t)530, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1TFHMMSS", std::make_pair((int32_t)26140, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1TFHMM", std::make_pair((int32_t)540, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1TFHH", std::make_pair((int32_t)10, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1TFH", std::make_pair((int32_t)20, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1TFMM", std::make_pair((int32_t)13, B1Types::B1T_WORD));
		_B1C_consts.emplace(L"B1TFSS", std::make_pair((int32_t)16, B1Types::B1T_WORD));

		return true;
	}
#endif

#if defined(B1_TARGET_RV32) || defined(B1_TARGET_ALL)
	if(settings.GetTargetName() == "RV32")
	{
		// default values: 2 kB of RAM, 16 kB of FLASH, 512 bytes of stack
		settings.Init(0x20000000, 0x0800, 0x0, 0x4000, 0x200, 0x0, 0);

#ifdef B1_DEF_STD_FNS
		// standard functions definition
		B1_CMP_FNS::_fns =
		{
			//			name,		ret. type				arg. types(def. values)				fn. name in std. library
			// standard functions
			B1_CMP_FN(L"LEN",		B1Types::B1T_BYTE,		{ B1Types::B1T_STRING },			L"__LIB_STR_LEN"),
			B1_CMP_FN(L"ASC",		B1Types::B1T_BYTE,		{ B1Types::B1T_STRING },			L"__LIB_STR_ASC"),
			B1_CMP_FN(L"CHR$",		B1Types::B1T_STRING,	{ B1Types::B1T_BYTE },				L"__LIB_STR_CHR"),
			B1_CMP_FN(L"STR$",		B1Types::B1T_STRING,	{ B1Types::B1T_BYTE },				L"__LIB_STR_STR"),
			B1_CMP_FN(L"STR$",		B1Types::B1T_STRING,	{ B1Types::B1T_INT },				L"__LIB_STR_STR"),
			B1_CMP_FN(L"STR$",		B1Types::B1T_STRING,	{ B1Types::B1T_WORD },				L"__LIB_STR_STR"),
			B1_CMP_FN(L"STR$",		B1Types::B1T_STRING,	{ B1Types::B1T_LONG },				L"__LIB_STR_STR"),
			B1_CMP_FN(L"VAL",		B1Types::B1T_LONG,		{ B1Types::B1T_STRING },			L"__LIB_STR_VAL"),
			B1_CMP_FN(L"CBYTE",		B1Types::B1T_BYTE,		{ B1Types::B1T_STRING },			L"__LIB_STR_CBYTE"),
			B1_CMP_FN(L"CINT",		B1Types::B1T_INT,		{ B1Types::B1T_STRING },			L"__LIB_STR_CINT"),
			B1_CMP_FN(L"CWRD",		B1Types::B1T_WORD,		{ B1Types::B1T_STRING },			L"__LIB_STR_CWRD"),
			B1_CMP_FN(L"CLNG",		B1Types::B1T_LONG,		{ B1Types::B1T_STRING },			L"__LIB_STR_VAL"),
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

			// IOCTL function: the records are used just for keyword identification as a function,
			// it's a special function so argument types validation is performed in another way
			B1_CMP_FN(L"IOCTL",		B1Types::B1T_UNKNOWN,	{ B1Types::B1T_STRING, B1Types::B1T_STRING },	L""),
			B1_CMP_FN(L"IOCTL$",	B1Types::B1T_UNKNOWN,	{ B1Types::B1T_STRING, B1Types::B1T_STRING },	L""),

			B1_CMP_FN(L"XORIN",		B1Types::B1T_BYTE,		{ B1Types::B1T_BYTE },				L""),
			B1_CMP_FN(L"XOROUT",	B1Types::B1T_BYTE,		{ B1Types::B1T_BYTE },				L""),

			// the last empty record, used to get records number
			B1_CMP_FN(L"",			B1Types::B1T_UNKNOWN,	std::initializer_list<B1Types>(),	L"")
		};
#endif

		// change RAM address type
		_B1AT_consts[L"__STACK_START"] = B1Types::B1T_LONG;
		_B1AT_consts[L"__STACK_SIZE"] = B1Types::B1T_LONG;
		_B1AT_consts[L"__HEAP_START"] = B1Types::B1T_LONG;
		_B1AT_consts[L"__HEAP_SIZE"] = B1Types::B1T_LONG;
		_B1AT_consts[L"__DATA_START"] = B1Types::B1T_LONG;
		_B1AT_consts[L"__DATA_SIZE"] = B1Types::B1T_LONG;
		_B1AT_consts[L"__DATA_TOTAL_SIZE"] = B1Types::B1T_LONG;

		return true;
	}
#endif

	return false;
}

std::string get_MCU_config_name(const std::string &MCU_name)
{
	auto uc_name = Utils::str_toupper(MCU_name);

#if defined(B1_TARGET_STM8) || defined(B1_TARGET_ALL)
	if(uc_name.substr(0, 4) == "STM8")
	{
		return uc_name.substr(0, 10);
	}
#endif

#if defined(B1_TARGET_RV32) || defined(B1_TARGET_ALL)
        // WCH's CH32 RISC-V microcontrollers
	if(uc_name.substr(0, 4) == "CH32")
	{
		return uc_name.substr(0, 10);
	}
#endif

	return uc_name;
}
