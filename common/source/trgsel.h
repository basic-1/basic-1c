/*
 BASIC1 compiler
 Copyright (c) 2021-2025 Nikolay Pletnev
 MIT license

 trgsel.h: select target
*/


#pragma once

#include "b1cmp.h"
#include "moresym.h"


#if defined(B1_TARGET_STM8) || defined(B1_TARGET_ALL)
#define STM8_PAGE0_SIZE 0x100

#define STM8_PAGE0_SECTION_TYPE_MOD L"PAGE0"


#define STM8_RET_ADDR_SIZE_MM_SMALL 2
#define STM8_RET_ADDR_SIZE_MM_LARGE 3


class STM8Settings: virtual public Settings
{
public:
	STM8Settings()
	: Settings()
	{
	}
};
#endif


extern bool select_target(Settings &settings);
extern std::string get_c1_compiler_name(const Settings &settings);
