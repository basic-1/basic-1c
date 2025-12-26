/*
 RISC-V 32-bit assembler
 Copyright (c) 2024 Nikolay Pletnev
 MIT license

 a1rv32.cpp: RISC-V 32-bit assembler
*/

#pragma once

#include "../../common/source/a1.h"


class A1RV32Settings: public RV32Settings, public A1Settings
{
protected:
	// use compressed instructions instead of 32-bit ones if possibly
	// setting the variable to false does not forbid compressed instructions themselves and
	// using them in pseudo-instructions if C extension is enabled
	bool _auto_comp_inst;
	// align .DATA section to 4 bytes boundary, .STACK section to 16 bytes boundary, .CODE
	// sections to 2 or 4 bytes boundaries (depending on C extension presence)
	bool _auto_align;


public:
	A1RV32Settings()
	: RV32Settings()
	, A1Settings()
	, _auto_comp_inst(true)
	, _auto_align(false)
	{
	}

	B1_T_ERROR ProcessNumPostfix(const std::wstring &postfix, int32_t &n) const override;

	A1_T_ERROR GetInstructions(const std::wstring &inst_sign, std::vector<const Inst *> &insts, int line_num, const std::string &file_name) const override;

	void SetAutoCompInst(bool auto_comp_inst = true) { _auto_comp_inst = auto_comp_inst; }
	bool GetAutoCompInst() const { return _auto_comp_inst; }

	void SetAutoAlign(bool auto_align = true) { _auto_align = auto_align; }
	bool GetAutoAlign() const { return _auto_align; }
};
