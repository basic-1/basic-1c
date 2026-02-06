/*
 RISC-V 32-bit assembler
 Copyright (c) 2024-2026 Nikolay Pletnev
 MIT license

 a1rv32.cpp: RISC-V 32-bit assembler
*/


#include <cstdio>
#include <clocale>
#include <cstring>
#include <cwctype>
#include <memory>
#include <fstream>
#include <algorithm>

#include "../../common/source/trgsel.h"
#include "../../common/source/version.h"
#include "../../common/source/gitrev.h"

#include "a1rv32.h"


static const char *version = B1_CMP_VERSION;


class RV32Inst: public Inst
{
protected:
	// some id
	int _inst_id;


public:
	RV32Inst(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type = ArgType::AT_NONE, const ArgType &arg2type = ArgType::AT_NONE, const ArgType &arg3type = ArgType::AT_NONE)
	: Inst(code, speed, arg1type, arg2type, arg3type)
	, _inst_id(inst_id)
	{
	}

	int GetId() const
	{
		return _inst_id;
	}
};


static std::multimap<std::wstring, std::unique_ptr<RV32Inst>> _instructions;


B1_T_ERROR A1RV32Settings::ProcessNumPostfix(const std::wstring &postfix, int32_t &n) const
{
	return RV32Settings::ProcessNumPostfix(postfix, n);
}

A1_T_ERROR A1RV32Settings::GetInstructions(const std::wstring &inst_sign, std::vector<const Inst *> &insts, int line_num, const std::string &file_name) const
{
	// replace instructions with relative addressing if their addresses are out of range
	const Inst *last_inst = nullptr;
	int next_inst_id = -1;

	if(GetFixAddresses() && (inst_sign == L"JV" || inst_sign == L"CALLV" || inst_sign == L"BEQXV,XV,V" || inst_sign == L"BNEXV,XV,V"))
	{
		next_inst_id = 0;

		if(IsInstToReplace(line_num, file_name, &last_inst))
		{
			next_inst_id = dynamic_cast<const RV32Inst *>(last_inst)->GetId() + 1;
		}
	}

	auto inst_sign_wo_pref = inst_sign;
	bool i32_inst = false;

	if(inst_sign.find(L"I32.") == 0)
	{
		inst_sign_wo_pref = inst_sign.substr(4);
		i32_inst = true;
	}

	auto inst_num = _instructions.count(inst_sign_wo_pref);

	if(inst_num == 0)
	{
		return A1_T_ERROR::A1_RES_EINVINST;
	}

	auto mi = _instructions.equal_range(inst_sign_wo_pref);
	for(auto inst = mi.first; inst != mi.second; inst++)
	{
		if(i32_inst && (inst->second->_speed != 1 || inst->second->_size != 4))
		{
			continue;
		}

		if(next_inst_id < 0 || next_inst_id == inst->second->GetId())
		{
			insts.push_back(inst->second.get());
		}
	}

	// sort the instructions by speed and size in ascending order
	for(auto i = 0; i < insts.size(); i++)
	{
		auto imin = i;
		auto min = insts[i]->_speed * 256 + insts[i]->_size;
		auto min_nxt = 0;
		for(auto j = i + 1; j < insts.size(); j++)
		{
			min_nxt = insts[j]->_speed * 256 + insts[j]->_size;
			if(min_nxt < min)
			{
				imin = j;
				min = min_nxt;
			}
		}
		if(imin != i)
		{
			std::swap(insts[i], insts[imin]);
		}
	}

	if(insts.size() == 0)
	{
		if(next_inst_id >= 0)
		{
			return A1_T_ERROR::A1_RES_ERELOUTRANGE;
		}
		else
		{
			return A1_T_ERROR::A1_RES_EINVINST;
		}
	}

	return A1_T_ERROR::A1_RES_OK;
}


A1RV32Settings global_settings;
A1Settings &_global_settings = global_settings;


static void b1_print_version(FILE *fstr)
{
	std::fputs("RISC-V 32-bit assembler\n", fstr);
	std::fputs("MIT license\n", fstr);
	std::fputs("Version: ", fstr);
	std::fputs(version, fstr);
#ifdef B1_GIT_REVISION
	std::fputs(" (", fstr);
	std::fputs(B1_GIT_REVISION, fstr);
	std::fputs(")", fstr);
#endif
	std::fputs("\n", fstr);
}

static std::wstring get_size_kB(int64_t size)
{
	size *= 1000;
	size /= 1024;

	auto size_int = size / 1000;
	size %= 1000;

	if (size % 10 >= 5) size = size - (size % 10) + 10;
	if (size % 100 >= 50) size = size - (size % 100) + 100;

	if (size >= 1000)
	{
		size_int++;
		size = 0;
	}
	else
	{
		size /= 100;
	}

	return std::to_wstring(size_int) + (size == 0 ? L"" : (L"." + std::to_wstring(size)));
}


class RV32ArgType: public ArgType
{
protected:
	std::vector<int32_t> _exclude;

public:
	static const RV32ArgType AT_RV32_REG; // 0..31
	static const RV32ArgType AT_RV32_REG_NZ; // 1..31
	static const RV32ArgType AT_RV32_REG_NZ_NSP; // 1, 3..31
	static const RV32ArgType AT_RV32_COMP_REG; // 8..15
	static const RV32ArgType AT_RV32_REG_Z; // 0
	static const RV32ArgType AT_RV32_REG_SP; // 2

	static const RV32ArgType AT_RV32_5BIT_UVAL; // 0..31
	
	static const RV32ArgType AT_RV32_12BIT_VAL; // -2048..2047
	static const RV32ArgType AT_RV32_20BIT_VAL; // -524288..524287

	// RISC-V offset types (e.g. AT_RV32_13BIT_OFF) are PC-relative offsets, they are encoded in multiples of 2 bytes
	// to take advantage of one "extra" bit
	static const RV32ArgType AT_RV32_13BIT_OFF; // -4096..4094
	static const RV32ArgType AT_RV32_12BIT_OFF; // -2048..2046
	static const RV32ArgType AT_RV32_21BIT_OFF; // -1048576..1048574
	static const RV32ArgType AT_RV32_9BIT_OFF; // -256..254

	static const RV32ArgType AT_RV32_7BIT_UVAL4; // 0..127
	static const RV32ArgType AT_RV32_8BIT_UVAL4; // 0..255
	static const RV32ArgType AT_RV32_10BIT_UVAL4; // 1..1023
	static const RV32ArgType AT_RV32_10BITNZ_VAL16; // -512..511
	static const RV32ArgType AT_RV32_5BITNZ_UVAL; // 1..31
	static const RV32ArgType AT_RV32_6BIT_VAL; // -32..31
	static const RV32ArgType AT_RV32_6BITNZ_VAL; // -32..31
	
	static const RV32ArgType AT_RV32_4BYTE_VAL; // INT32_MIN..INT32_MAX


	RV32ArgType() = delete;

	RV32ArgType(int size, int32_t minval, int32_t maxval, int32_t multipleof = 1, const std::vector<int32_t> &exclude = std::vector<int32_t>())
	: ArgType(size, minval, maxval, multipleof)
	, _exclude(exclude)
	{
	}

	bool IsValidValue(int32_t value) const override
	{
		return (*this == AT_NONE) || (ArgType::IsValidValue(value) && (std::find(_exclude.cbegin(), _exclude.cend(), value) == _exclude.cend()));
	}

	bool IsRelOffset() const override
	{
		return ArgType::IsRelOffset() || (*this == AT_RV32_13BIT_OFF) || (*this == AT_RV32_12BIT_OFF) || (*this == AT_RV32_21BIT_OFF) || (*this == AT_RV32_9BIT_OFF);
	}
};

const RV32ArgType RV32ArgType::AT_RV32_REG(1, 0, 31); // 0..31
const RV32ArgType RV32ArgType::AT_RV32_REG_NZ(1, 1, 31); // 1..31
const RV32ArgType RV32ArgType::AT_RV32_REG_NZ_NSP(1, 1, 31, 1, { 2 }); // 1, 3..31
const RV32ArgType RV32ArgType::AT_RV32_COMP_REG(1, 8, 15); // 8..15
const RV32ArgType RV32ArgType::AT_RV32_REG_SP(1, 2, 2); // 2
const RV32ArgType RV32ArgType::AT_RV32_REG_Z(1, 0, 0); // 0

const RV32ArgType RV32ArgType::AT_RV32_5BIT_UVAL(1, 0, 31); // 0..31

const RV32ArgType RV32ArgType::AT_RV32_12BIT_VAL(2, -2048, 2047); // -2048..2047
const RV32ArgType RV32ArgType::AT_RV32_20BIT_VAL(3, -524288, 524287); // -524288..524287

const RV32ArgType RV32ArgType::AT_RV32_13BIT_OFF(2, -4096, 4094); // -4096..4094
const RV32ArgType RV32ArgType::AT_RV32_12BIT_OFF(2, -2048, 2046); // -2048..2046
const RV32ArgType RV32ArgType::AT_RV32_21BIT_OFF(3, -1048576, 1048574); // -1048576..1048574
const RV32ArgType RV32ArgType::AT_RV32_9BIT_OFF(2, -256, 254); // -256..254

const RV32ArgType RV32ArgType::AT_RV32_7BIT_UVAL4(1, 0, 127, 4); // 0..127
const RV32ArgType RV32ArgType::AT_RV32_8BIT_UVAL4(1, 0, 255, 4); // 0..255
const RV32ArgType RV32ArgType::AT_RV32_10BIT_UVAL4(2, 1, 1023, 4); // 1..1023
const RV32ArgType RV32ArgType::AT_RV32_10BITNZ_VAL16(2, -512, 511, 16, { 0 }); // -512..511
const RV32ArgType RV32ArgType::AT_RV32_5BITNZ_UVAL(1, 1, 31); // 1..31
const RV32ArgType RV32ArgType::AT_RV32_6BIT_VAL(1, -32, 31); // -32..31
const RV32ArgType RV32ArgType::AT_RV32_6BITNZ_VAL(1, -32, 31, 1, { 0 }); // -32..31

const RV32ArgType RV32ArgType::AT_RV32_4BYTE_VAL(4, INT32_MIN, INT32_MAX); // INT32_MIN..INT32_MAX


class RV32InstFence: public RV32Inst
{
private:
	mutable std::map<std::wstring, MemRef> _fence_op_arg_values;


	const std::map<std::wstring, MemRef> &get_fence_op_arg_values() const
	{
		if(_fence_op_arg_values.empty())
		{
			MemRef mr;
			const std::wstring allbits = L"IORW";

			for(auto i = 1; i < 16; i++)
			{
				std::wstring sign;
				auto v = i << 28;
				for(auto b = 0; b < 4; b++)
				{
					if(v < 0)
					{
						sign += allbits[b];
					}
					v <<= 1;
				}
				mr.SetName(sign);
				mr.SetAddress(i);
				_fence_op_arg_values.emplace(std::make_pair(sign, mr));
			}
		}

		return _fence_op_arg_values;
	}


public:
	RV32InstFence(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type = ArgType::AT_NONE)
	: RV32Inst(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		return (a1 >= 1 && a1 <= 15) && (a2 >= 1 && a2 <= 15);
	}

	A1_T_ERROR GetSpecArg(int arg_num, std::pair<std::reference_wrapper<const ArgType>, Exp> &ref, int32_t &val) const override
	{
		auto err = ref.second.Eval(val, get_fence_op_arg_values());
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
		ref.first = ArgType::AT_1BYTE_ADDR;
		ref.second.Clear();
		ref.second.AddVal(val);

		return A1_T_ERROR::A1_RES_OK;
	}
};

class RV32InstCSR: public RV32Inst
{
private:
	mutable std::map<std::wstring, MemRef> _CSR_op_arg_values;


	const std::map<std::wstring, MemRef> &get_CSR_op_arg_values() const
	{
		if(_CSR_op_arg_values.empty())
		{
			MemRef mr;

			// some known CSRs
			mr.SetName(L"MARCHID");
			mr.SetAddress(0xF12);
			_CSR_op_arg_values.emplace(std::make_pair(mr.GetName(), mr));

			mr.SetName(L"MIMPID");
			mr.SetAddress(0xF13);
			_CSR_op_arg_values.emplace(std::make_pair(mr.GetName(), mr));

			mr.SetName(L"MSTATUS");
			mr.SetAddress(0x300);
			_CSR_op_arg_values.emplace(std::make_pair(mr.GetName(), mr));

			mr.SetName(L"MISA");
			mr.SetAddress(0x301);
			_CSR_op_arg_values.emplace(std::make_pair(mr.GetName(), mr));

			mr.SetName(L"MTVEC");
			mr.SetAddress(0x305);
			_CSR_op_arg_values.emplace(std::make_pair(mr.GetName(), mr));

			mr.SetName(L"MSCRATCH");
			mr.SetAddress(0x340);
			_CSR_op_arg_values.emplace(std::make_pair(mr.GetName(), mr));

			mr.SetName(L"MEPC");
			mr.SetAddress(0x341);
			_CSR_op_arg_values.emplace(std::make_pair(mr.GetName(), mr));

			mr.SetName(L"MCAUSE");
			mr.SetAddress(0x342);
			_CSR_op_arg_values.emplace(std::make_pair(mr.GetName(), mr));
		}

		return _CSR_op_arg_values;
	}


public:
	RV32InstCSR(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		return (a2 >= 0 && a2 <= 4095) && _argtypes[0].get().IsValidValue(a1) && _argtypes[2].get().IsValidValue(a3);
	}

	A1_T_ERROR GetSpecArg(int arg_num, std::pair<std::reference_wrapper<const ArgType>, Exp> &ref, int32_t &val) const override
	{
		auto err = ref.second.Eval(val, get_CSR_op_arg_values());
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
		ref.first = ArgType::AT_2BYTE_ADDR;
		ref.second.Clear();
		ref.second.AddVal(val);

		return A1_T_ERROR::A1_RES_OK;
	}
};

class RV32Inst12Eq: public RV32Inst
{
public:
	RV32Inst12Eq(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type = ArgType::AT_NONE)
	: RV32Inst(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		return (a1 == a2) && RV32Inst::CheckArgs(a1, a2, a3);
	}
};

class RV32Inst12EqNT0: public RV32Inst12Eq
{
public:
	RV32Inst12EqNT0(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type = ArgType::AT_NONE)
	: RV32Inst12Eq(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		// a1 and a2 must not be T0 register
		return (a1 != 5) && RV32Inst12Eq::CheckArgs(a1, a2, a3);
	}
};

class RV32Inst12Ne : public RV32Inst
{
public:
	RV32Inst12Ne(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type = ArgType::AT_NONE)
	: RV32Inst(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		return (a1 != a2) && RV32Inst::CheckArgs(a1, a2, a3);
	}
};

// arg1 != arg2 && arg3.L12 == 0
class RV32Inst12Ne3L0 : public RV32Inst12Ne
{
public:
	RV32Inst12Ne3L0(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst12Ne(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		return ((a3 & 0xFFF) == 0) && RV32Inst12Ne::CheckArgs(a1, a2, a3);
	}
};

// arg1 != arg2 && arg3.L12 >= -32 && arg3.L12 <= 31
class RV32Inst12Ne3L6 : public RV32Inst12Ne
{
public:
	RV32Inst12Ne3L6(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst12Ne(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a3l = a3;

		global_settings.ProcessNumPostfix(L"L12", a3l);
		return (a3l >= -32) && (a3l <= 31) && RV32Inst12Ne::CheckArgs(a1, a2, a3);
	}
};

// arg1 != arg2 && arg3.H20 >= -32 && arg3.H20 <= 31 && arg3.L12 >= -32 && arg3.L12 <= 31
class RV32Inst12Ne3H6NZL6 : public RV32Inst12Ne3L6
{
public:
	RV32Inst12Ne3H6NZL6(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst12Ne3L6(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a3h = a3;

		global_settings.ProcessNumPostfix(L"H20", a3h);
		return (a3h >= -32) && (a3h <= 31) && (a3h != 0) &&  RV32Inst12Ne3L6::CheckArgs(a1, a2, a3);
	}
};

// arg1 != arg2 && arg3.H20 >= -32 && arg3.H20 <= 31 && arg3.L12 == 0
class RV32Inst12Ne3H6NZL0 : public RV32Inst12Ne3L0
{
public:
	RV32Inst12Ne3H6NZL0(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst12Ne3L0(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a3h = a3;

		global_settings.ProcessNumPostfix(L"H20", a3h);
		return (a3h >= -32) && (a3h <= 31) && (a3h != 0) && RV32Inst12Ne3L0::CheckArgs(a1, a2, a3);
	}
};

// arg1 != arg2 && arg3.H20 >= -32 && arg3.H20 <= 31
class RV32Inst12Ne3H6NZ : public RV32Inst12Ne
{
public:
	RV32Inst12Ne3H6NZ(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst12Ne(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a3h = a3;

		global_settings.ProcessNumPostfix(L"H20", a3h);
		return (a3h >= -32) && (a3h <= 31) && (a3h != 0) && RV32Inst12Ne::CheckArgs(a1, a2, a3);
	}
};

class RV32Inst1NeT0: public RV32Inst
{
public:
	RV32Inst1NeT0(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type = ArgType::AT_NONE, const ArgType &arg3type = ArgType::AT_NONE)
	: RV32Inst(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		// a1 must not be T0 register
		return (a1 != 5) && RV32Inst::CheckArgs(a1, a2, a3);
	}
};

class RV32Inst12NeT0: public RV32Inst
{
public:
	RV32Inst12NeT0(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type = ArgType::AT_NONE)
	: RV32Inst(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		// a1 and a2 must not be T0 register
		return (a1 != 5) && (a2 != 5) && RV32Inst::CheckArgs(a1, a2, a3);
	}
};

// arg1 == arg2 && arg3.L12 == 0
class RV32Inst12Eq3L0 : public RV32Inst12EqNT0
{
public:
	RV32Inst12Eq3L0(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst12EqNT0(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		return ((a3 & 0xFFF) == 0) && RV32Inst12EqNT0::CheckArgs(a1, a2, a3);
	}
};

// arg1 == arg2 && arg3.L12 >= -32 && arg3.L12 <= 31
class RV32Inst12Eq3L6 : public RV32Inst12EqNT0
{
public:
	RV32Inst12Eq3L6(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst12EqNT0(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a3l = a3;

		global_settings.ProcessNumPostfix(L"L12", a3l);
		return (a3l >= -32) && (a3l <= 31) && RV32Inst12EqNT0::CheckArgs(a1, a2, a3);
	}
};

// arg1 == arg2 && arg3.H20 >= -32 && arg3.H20 <= 31 && arg3.L12 == 0
class RV32Inst12Eq3H6NZL0 : public RV32Inst12Eq3L0
{
public:
	RV32Inst12Eq3H6NZL0(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst12Eq3L0(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a3h = a3;

		global_settings.ProcessNumPostfix(L"H20", a3h);
		return (a3h >= -32) && (a3h <= 31) && (a3h != 0) && RV32Inst12Eq3L0::CheckArgs(a1, a2, a3);
	}
};

// arg1 == arg2 && arg3.H20 >= -32 && arg3.H20 <= 31 && arg3.L12 >= -32 && arg3.L12 <= 31
class RV32Inst12Eq3H6NZL6 : public RV32Inst12Eq3L6
{
public:
	RV32Inst12Eq3H6NZL6(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst12Eq3L6(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a3h = a3;

		global_settings.ProcessNumPostfix(L"H20", a3h);
		return (a3h >= -32) && (a3h <= 31) && (a3h != 0) && RV32Inst12Eq3L6::CheckArgs(a1, a2, a3);
	}
};

// arg1 == arg2 && arg3.H20 >= -32 && arg3.H20 <= 31
class RV32Inst12Eq3H6NZ : public RV32Inst12EqNT0
{
public:
	RV32Inst12Eq3H6NZ(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst12EqNT0(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a3h = a3;

		global_settings.ProcessNumPostfix(L"H20", a3h);
		return (a3h >= -32) && (a3h <= 31) && (a3h != 0) && RV32Inst12EqNT0::CheckArgs(a1, a2, a3);
	}
};

class RV32Inst13Eq: public RV32Inst
{
public:
	RV32Inst13Eq(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		return (a1 == a3) && RV32Inst::CheckArgs(a1, a2, a3);
	}
};

// arg2.L12 >= 0 && arg2.L12 <= 127 && (arg2.L12 % 4) == 0
class RV32Inst2LU7M4 : public RV32Inst
{
public:
	RV32Inst2LU7M4(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type = ArgType::AT_NONE)
	: RV32Inst(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a2l = a2;

		global_settings.ProcessNumPostfix(L"L12", a2l);
		return (a2l >= 0) && (a2l <= 127) && (a2l %4 == 0) && RV32Inst::CheckArgs(a1, a2, a3);
	}
};

// arg1 == arg3 && arg2.L12 >= 0 && arg2.L12 <= 127 && (arg2.L12 % 4) == 0
class RV32Inst13Eq2LU7M4 : public RV32Inst13Eq
{
public:
	RV32Inst13Eq2LU7M4(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst13Eq(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a2l = a2;

		global_settings.ProcessNumPostfix(L"L12", a2l);
		return (a2l >= 0) && (a2l <= 127) && (a2l %4 == 0) && RV32Inst13Eq::CheckArgs(a1, a2, a3);
	}
};

class RV32Inst13Ne : public RV32Inst
{
public:
	RV32Inst13Ne(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		return (a1 != a3) && RV32Inst::CheckArgs(a1, a2, a3);
	}
};

class RV32Inst13NeNT0 : public RV32Inst13Ne
{
public:
	RV32Inst13NeNT0(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst13Ne(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		return (a1 != 5) && (a3 != 5) && RV32Inst13Ne::CheckArgs(a1, a2, a3);
	}
};

class RV32Inst13EqNT0 : public RV32Inst13Eq
{
public:
	RV32Inst13EqNT0(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst13Eq(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		return (a1 != 5) && RV32Inst13Eq::CheckArgs(a1, a2, a3);
	}
};

// arg1 != arg3 && arg2.L12 >= 0 && arg2.L12 <= 127 && (arg2.L12 % 4) == 0
class RV32Inst13Ne2LU7M4 : public RV32Inst13Ne
{
public:
	RV32Inst13Ne2LU7M4(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type)
	: RV32Inst13Ne(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a2l = a2;

		global_settings.ProcessNumPostfix(L"L12", a2l);
		return (a2l >= 0) && (a2l <= 127) && (a2l %4 == 0) && RV32Inst13Ne::CheckArgs(a1, a2, a3);
	}
};

// arg2.L12 == 0
class RV32Inst2L0 : public RV32Inst
{
public:
	RV32Inst2L0(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type = ArgType::AT_NONE)
	: RV32Inst(inst_id, code, speed, arg1type, arg2type, arg3type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		return ((a2 & 0xFFF) == 0) && RV32Inst::CheckArgs(a1, a2, a3);
	}
};

class RV32Inst2L6: public RV32Inst
{
public:
	RV32Inst2L6(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type)
	: RV32Inst(inst_id, code, speed, arg1type, arg2type, ArgType::AT_NONE)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a2l = a2;

		global_settings.ProcessNumPostfix(L"L12", a2l);
		return (a2l >= -32) && (a2l <= 31) && RV32Inst::CheckArgs(a1, a2, a3);
	}
};

class RV32Inst2H6NZ: public RV32Inst
{
public:
	RV32Inst2H6NZ(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type)
	: RV32Inst(inst_id, code, speed, arg1type, arg2type, ArgType::AT_NONE)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a2h = a2;

		global_settings.ProcessNumPostfix(L"H20", a2h);
		return (a2h >= -32) && (a2h <= 31) && (a2h != 0) && RV32Inst::CheckArgs(a1, a2, a3);
	}
};

class RV32Inst2H6NZL0: public RV32Inst2H6NZ
{
public:
	RV32Inst2H6NZL0(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type)
	: RV32Inst2H6NZ(inst_id, code, speed, arg1type, arg2type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		return ((a2 & 0xFFF) == 0) && RV32Inst2H6NZ::CheckArgs(a1, a2, a3);
	}
};

class RV32Inst2H6NZL6: public RV32Inst2H6NZ
{
public:
	RV32Inst2H6NZL6(int inst_id, const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type)
	: RV32Inst2H6NZ(inst_id, code, speed, arg1type, arg2type)
	{
	}

	bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const override
	{
		int32_t a2l = a2;

		global_settings.ProcessNumPostfix(L"L12", a2l);
		return (a2l >= -32) && (a2l <= 31) && RV32Inst2H6NZ::CheckArgs(a1, a2, a3);
	}
};


// add RV32Inst instruction definition
#define ADD_INST(SIGN, OPCODE, ...) _instructions.emplace(SIGN, new RV32Inst(-1, (OPCODE), 1, ##__VA_ARGS__))
#define ADD_INSTI(ID, SIGN, OPCODE, ...) _instructions.emplace(SIGN, new RV32Inst((ID), (OPCODE), 1, ##__VA_ARGS__))
// add RV32Inst compressed instruction to use instead of a full-length one
#define ADD_INSTC(SIGN, OPCODE, ...) if(global_settings.GetAutoCompInst()) { _instructions.emplace(SIGN, new RV32Inst(-1, (OPCODE), 1, ##__VA_ARGS__)); }
#define ADD_INSTCI(ID, SIGN, OPCODE, ...) if(global_settings.GetAutoCompInst()) { _instructions.emplace(SIGN, new RV32Inst((ID), (OPCODE), 1, ##__VA_ARGS__)); }
// add RV32Inst pseudo-instruction definition (consisting of two instructions)
#define ADD_INST2(SIGN, OPCODE, ...) _instructions.emplace(SIGN, new RV32Inst(-1, (OPCODE), 2, ##__VA_ARGS__))
#define ADD_INST2I(ID, SIGN, OPCODE, ...) _instructions.emplace(SIGN, new RV32Inst((ID), (OPCODE), 2, ##__VA_ARGS__))
// add instruction derived from RV32Inst
#define ADD_IDER(CLS_NAME, SPEED, SIGN, OPCODE, ...) _instructions.emplace(SIGN, new CLS_NAME(-1, (OPCODE), (SPEED), ##__VA_ARGS__))
// add compressed instruction derived from RV32Inst to use instead of a full-length one
#define ADD_IDERC(CLS_NAME, SPEED, SIGN, OPCODE, ...) if(global_settings.GetAutoCompInst()) { _instructions.emplace(SIGN, new CLS_NAME(-1, (OPCODE), (SPEED), ##__VA_ARGS__)); }

static void load_RV32_instructions()
{
	if(_global_settings.GetCompressed())
	{
		ADD_INST(L"C.ADDI4SPNXV,XV,V", L"0:3 {3:5:2} {3:9:4} {3:2:1} {3:3:1} {1:2:3} 0:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_REG_SP, RV32ArgType::AT_RV32_10BIT_UVAL4);
		// ADDI rd', SP, <nzuimm10> (<nzuimm10> is a multiple of 4)
		ADD_INSTC(L"ADDIXV,XV,V", L"0:3 {3:5:2} {3:9:4} {3:2:1} {3:3:1} {1:2:3} 0:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_REG_SP, RV32ArgType::AT_RV32_10BIT_UVAL4);

		ADD_INST(L"C.LWXV,V(XV)", L"2:3 {2:5:3} {3:2:3} {2:2:1} {2:6:1} {1:2:3} 0:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_7BIT_UVAL4, RV32ArgType::AT_RV32_COMP_REG);
		// LW rd', <uimm7>(rs') (uimm7 is a multiple of 4)
		ADD_INSTC(L"LWXV,V(XV)", L"2:3 {2:5:3} {3:2:3} {2:2:1} {2:6:1} {1:2:3} 0:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_7BIT_UVAL4, RV32ArgType::AT_RV32_COMP_REG);
		ADD_INST(L"C.SWXV,V(XV)", L"6:3 {2:5:3} {3:2:3} {2:2:1} {2:6:1} {1:2:3} 0:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_7BIT_UVAL4, RV32ArgType::AT_RV32_COMP_REG);
		// SW rs1', <uimm7>(rs2') (uimm7 is a multiple of 4)
		ADD_INSTC(L"SWXV,V(XV)", L"6:3 {2:5:3} {3:2:3} {2:2:1} {2:6:1} {1:2:3} 0:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_7BIT_UVAL4, RV32ArgType::AT_RV32_COMP_REG);


		ADD_INST(L"C.NOP", L"0:3 0:1 0:5 0:5 1:2");
		// NOP
		ADD_INSTC(L"NOP", L"0:3 0:1 0:5 0:5 1:2");
		ADD_INST(L"C.ADDIXV,V", L"0:3 {2:5:1} {1:4:5} {2:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_6BITNZ_VAL);
		// ADDI r, r, <nzimm6>
		ADD_IDERC(RV32Inst12Eq, 1, L"ADDIXV,XV,V", L"0:3 {3.L12:5:1} {1:4:5} {3.L12:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_6BITNZ_VAL);

		ADD_INST(L"C.JALV", L"1:3 {1:B:1} {1:4:1} {1:9:2} {1:A:1} {1:6:1} {1:7:1} {1:3:3} {1:5:1} 1:2", RV32ArgType::AT_RV32_12BIT_OFF);

		ADD_INST(L"C.LIXV,V", L"2:3 {2:5:1} {1:4:5} {2:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_6BIT_VAL);
		// LI rd, <imm6>
		ADD_INSTC(L"LIXV,V", L"2:3 {2:5:1} {1:4:5} {2:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_6BIT_VAL);
		ADD_INST(L"C.ADDI16SPXV,V", L"3:3 {2:9:1} 2:5 {2:4:1} {2:6:1} {2:8:2} {2:5:1} 1:2", RV32ArgType::AT_RV32_REG_SP, RV32ArgType::AT_RV32_10BITNZ_VAL16);
		// ADDI SP, SP, <nzimm10> (nzimm10 is a multiple of 16)
		ADD_INSTC(L"ADDIXV,XV,V", L"3:3 {3:9:1} 2:5 {3:4:1} {3:6:1} {3:8:2} {3:5:1} 1:2", RV32ArgType::AT_RV32_REG_SP, RV32ArgType::AT_RV32_REG_SP, RV32ArgType::AT_RV32_10BITNZ_VAL16);
		ADD_INST(L"C.LUIXV,V", L"3:3 {2:5:1} {1:4:5} {2:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ_NSP, RV32ArgType::AT_RV32_6BITNZ_VAL);
		// LUI rd, <nzimm6>
		ADD_INSTC(L"LUIXV,V", L"3:3 {2:5:1} {1:4:5} {2:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ_NSP, RV32ArgType::AT_RV32_6BITNZ_VAL);

		ADD_INST(L"C.SRLIXV,V", L"4:3 0:1 0:2 {1:2:3} {2:4:5} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_5BITNZ_UVAL);
		// SRLI r', r', <nzuimm5>
		ADD_IDERC(RV32Inst12Eq, 1, L"SRLIXV,XV,V", L"4:3 0:1 0:2 {1:2:3} {3:4:5} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_5BITNZ_UVAL);
		ADD_INST(L"C.SRAIXV,V", L"4:3 0:1 1:2 {1:2:3} {2:4:5} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_5BITNZ_UVAL);
		// SRAI r', r', <nzuimm5>
		ADD_IDERC(RV32Inst12Eq, 1, L"SRAIXV,XV,V", L"4:3 0:1 1:2 {1:2:3} {3:4:5} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_5BITNZ_UVAL);
		ADD_INST(L"C.ANDIXV,V", L"4:3 {2:5:1} 2:2 {1:2:3} {2:4:5} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_6BIT_VAL);
		// ANDI r', r', <imm6>
		ADD_IDERC(RV32Inst12Eq, 1, L"ANDIXV,XV,V", L"4:3 {3:5:1} 2:2 {1:2:3} {3:4:5} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_6BIT_VAL);

		ADD_INST(L"C.SUBXV,XV", L"4:3 0:1 3:2 {1:2:3} 0:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG);
		// SUB rd', rd', rs'
		ADD_IDERC(RV32Inst12Eq, 1, L"SUBXV,XV,XV", L"4:3 0:1 3:2 {1:2:3} 0:2 {3:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG);
		ADD_INST(L"C.XORXV,XV", L"4:3 0:1 3:2 {1:2:3} 1:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG);
		// XOR rd', rd', rs'
		ADD_IDERC(RV32Inst12Eq, 1, L"XORXV,XV,XV", L"4:3 0:1 3:2 {1:2:3} 1:2 {3:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG);
		// XOR rd', rs', rd'
		ADD_IDERC(RV32Inst13Eq, 1, L"XORXV,XV,XV", L"4:3 0:1 3:2 {1:2:3} 1:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG);
		ADD_INST(L"C.ORXV,XV", L"4:3 0:1 3:2 {1:2:3} 2:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG);
		// OR rd', rd', rs'
		ADD_IDERC(RV32Inst12Eq, 1, L"ORXV,XV,XV", L"4:3 0:1 3:2 {1:2:3} 2:2 {3:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG);
		// OR rd', rs', rd'
		ADD_IDERC(RV32Inst13Eq, 1, L"ORXV,XV,XV", L"4:3 0:1 3:2 {1:2:3} 2:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG);
		ADD_INST(L"C.ANDXV,XV", L"4:3 0:1 3:2 {1:2:3} 3:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG);
		// AND rd', rd', rs'
		ADD_IDERC(RV32Inst12Eq, 1, L"ANDXV,XV,XV", L"4:3 0:1 3:2 {1:2:3} 3:2 {3:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG);
		// AND rd', rs', rd'
		ADD_IDERC(RV32Inst13Eq, 1, L"ANDXV,XV,XV", L"4:3 0:1 3:2 {1:2:3} 3:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG);


		ADD_INST(L"C.JV", L"5:3 {1:B:1} {1:4:1} {1:9:2} {1:A:1} {1:6:1} {1:7:1} {1:3:3} {1:5:1} 1:2", RV32ArgType::AT_RV32_12BIT_OFF);
		ADD_INST(L"C.BEQZXV,V", L"6:3 {2:8:1} {2:4:2} {1:2:3} {2:7:2} {2:2:2} {2:5:1} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_9BIT_OFF);
		ADD_INST(L"C.BNEZXV,V", L"7:3 {2:8:1} {2:4:2} {1:2:3} {2:7:2} {2:2:2} {2:5:1} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_9BIT_OFF);


		ADD_INST(L"C.SLLIXV,V", L"0:3 0:1 {1:4:5} {2:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_5BITNZ_UVAL);
		// SLLI r, r, <nzuimm5>
		ADD_IDERC(RV32Inst12Eq, 1, L"SLLIXV,XV,V", L"0:3 0:1 {1:4:5} {3:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_5BITNZ_UVAL);

		ADD_INST(L"C.LWSPXV,V(XV)", L"2:3 {2:5:1} {1:4:5} {2:4:3} {2:7:2} 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_8BIT_UVAL4, RV32ArgType::AT_RV32_REG_SP);
		// LW rd, <uimm8>(SP)
		ADD_INSTC(L"LWXV,V(XV)", L"2:3 {2:5:1} {1:4:5} {2:4:3} {2:7:2} 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_8BIT_UVAL4, RV32ArgType::AT_RV32_REG_SP);

		ADD_INST(L"C.JRXV", L"4:3 0:1 {1:4:5} 0:5 2:2", RV32ArgType::AT_RV32_REG_NZ);
		ADD_INST(L"C.MVXV,XV", L"4:3 0:1 {1:4:5} {2:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ);
		// MV rd, rs
		ADD_INSTC(L"MVXV,XV", L"4:3 0:1 {1:4:5} {2:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ);
		ADD_INST(L"C.EBREAK", L"4:3 1:1 0:5 0:5 2:2");
		// EBREAK
		ADD_INSTC(L"EBREAK", L"4:3 1:1 0:5 0:5 2:2");
		ADD_INST(L"C.JALRXV", L"4:3 1:1 {1:4:5} 0:5 2:2", RV32ArgType::AT_RV32_REG_NZ);
		ADD_INST(L"C.ADDXV,XV", L"4:3 1:1 {1:4:5} {2:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ);
		// ADD rd, rd, rs
		ADD_IDERC(RV32Inst12Eq, 1, L"ADDXV,XV,XV", L"4:3 1:1 {1:4:5} {3:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ);
		// ADD rd, rs, rd
		ADD_IDERC(RV32Inst13Eq, 1, L"ADDXV,XV,XV", L"4:3 1:1 {1:4:5} {2:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ);

		ADD_INST(L"C.SWSPXV,V(XV)", L"6:3 {2:5:4} {2:7:2} {1:4:5} 2:2", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_8BIT_UVAL4, RV32ArgType::AT_RV32_REG_SP);
		// SW rs, <uimm8>(SP)
		ADD_INSTC(L"SWXV,V(XV)", L"6:3 {2:5:4} {2:7:2} {1:4:5} 2:2", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_8BIT_UVAL4, RV32ArgType::AT_RV32_REG_SP);
	}

	ADD_INST(L"LUIXV,V",		L"{2:13:8} {2:B:8} {2:3:4} {1:4:5} 37:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_20BIT_VAL);

	ADD_INST(L"AUIPCXV,V",		L"{2:13:8} {2:B:8} {2:3:4} {1:4:5} 17:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_20BIT_VAL);


	ADD_INST(L"JALXV,V",		L"{2:14:1} {2:A:A} {2:B:1} {2:13:8} {1:4:5} 6F:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_21BIT_OFF);


	ADD_INST(L"JALRXV,V(XV)",	L"{2:B:4} {2:7:8} {3:4:5} 0:3 {1:4:5} 67:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL, RV32ArgType::AT_RV32_REG);


	ADD_INSTI(1, L"BEQXV,XV,V",	L"{3:C:1} {3:A:6} {2:4:5} {1:4:5} 0:3 {3:4:4} {3:B:1} 63:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_13BIT_OFF);
	ADD_INSTI(1, L"BNEXV,XV,V",	L"{3:C:1} {3:A:6} {2:4:5} {1:4:5} 1:3 {3:4:4} {3:B:1} 63:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_13BIT_OFF);
	ADD_INST(L"BLTXV,XV,V",		L"{3:C:1} {3:A:6} {2:4:5} {1:4:5} 4:3 {3:4:4} {3:B:1} 63:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_13BIT_OFF);
	ADD_INST(L"BGEXV,XV,V",		L"{3:C:1} {3:A:6} {2:4:5} {1:4:5} 5:3 {3:4:4} {3:B:1} 63:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_13BIT_OFF);
	ADD_INST(L"BLTUXV,XV,V",	L"{3:C:1} {3:A:6} {2:4:5} {1:4:5} 6:3 {3:4:4} {3:B:1} 63:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_13BIT_OFF);
	ADD_INST(L"BGEUXV,XV,V",	L"{3:C:1} {3:A:6} {2:4:5} {1:4:5} 7:3 {3:4:4} {3:B:1} 63:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_13BIT_OFF);


	ADD_INST(L"LBXV,V(XV)",		L"{2:B:4} {2:7:8} {3:4:5} 0:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"LHXV,V(XV)",		L"{2:B:4} {2:7:8} {3:4:5} 1:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"LWXV,V(XV)",		L"{2:B:4} {2:7:8} {3:4:5} 2:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"LBUXV,V(XV)",	L"{2:B:4} {2:7:8} {3:4:5} 4:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"LHUXV,V(XV)",	L"{2:B:4} {2:7:8} {3:4:5} 5:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL, RV32ArgType::AT_RV32_REG);

	ADD_INST(L"SBXV,V(XV)",		L"{2:B:7} {1:4:5} {3:4:5} 0:3 {2:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"SHXV,V(XV)",		L"{2:B:7} {1:4:5} {3:4:5} 1:3 {2:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"SWXV,V(XV)",		L"{2:B:7} {1:4:5} {3:4:5} 2:3 {2:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL, RV32ArgType::AT_RV32_REG);


	ADD_INST(L"ADDIXV,XV,V",	L"{3:B:4} {3:7:8} {2:4:5} 0:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	ADD_INST(L"SLTIXV,XV,V",	L"{3:B:4} {3:7:8} {2:4:5} 2:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	ADD_INST(L"SLTIUXV,XV,V",	L"{3:B:4} {3:7:8} {2:4:5} 3:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	ADD_INST(L"XORIXV,XV,V",	L"{3:B:4} {3:7:8} {2:4:5} 4:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	ADD_INST(L"ORIXV,XV,V",		L"{3:B:4} {3:7:8} {2:4:5} 6:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	ADD_INST(L"ANDIXV,XV,V",	L"{3:B:4} {3:7:8} {2:4:5} 7:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);


	ADD_INST(L"SLLIXV,XV,V",	L"0:7 {3:4:5} {2:4:5} 1:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, ArgType::AT_1BYTE_VAL);
	ADD_INST(L"SRLIXV,XV,V",	L"0:7 {3:4:5} {2:4:5} 5:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, ArgType::AT_1BYTE_VAL);
	ADD_INST(L"SRAIXV,XV,V",	L"20:7 {3:4:5} {2:4:5} 5:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, ArgType::AT_1BYTE_VAL);


	ADD_INST(L"ADDXV,XV,XV",	L"0:7 {3:4:5} {2:4:5} 0:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"SUBXV,XV,XV",	L"20:7 {3:4:5} {2:4:5} 0:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"SLLXV,XV,XV",	L"0:7 {3:4:5} {2:4:5} 1:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"SLTXV,XV,XV",	L"0:7 {3:4:5} {2:4:5} 2:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"SLTUXV,XV,XV",	L"0:7 {3:4:5} {2:4:5} 3:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"XORXV,XV,XV",	L"0:7 {3:4:5} {2:4:5} 4:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"SRLXV,XV,XV",	L"0:7 {3:4:5} {2:4:5} 5:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"SRAXV,XV,XV",	L"20:7 {3:4:5} {2:4:5} 5:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"ORXV,XV,XV",		L"0:7 {3:4:5} {2:4:5} 6:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	ADD_INST(L"ANDXV,XV,XV",	L"0:7 {3:4:5} {2:4:5} 7:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);


	ADD_INST(L"ECALL",			L"0:C 0:5 0:3 0:5 73:7");
	ADD_INST(L"EBREAK",			L"1:C 0:5 0:3 0:5 73:7");

	ADD_INST(L"FENCE",			L"0:4 F:4 F:4 0:5 0:3 0:5 F:7");
	ADD_INST(L"FENCE.TSO",		L"8:4 3:4 3:4 0:5 0:3 0:5 F:7");
	ADD_IDER(RV32InstFence, 1,	L"FENCEV,V",	L"0:4 {1:3:4} {2:3:4} 0:5 0:3 0:5 F:7", ArgType::AT_SPEC_TYPE, ArgType::AT_SPEC_TYPE);


	ADD_INST(L"MRET",			L"302:C 0:5 0:3 0:5 73:7");
	ADD_INST(L"SRET",			L"102:C 0:5 0:3 0:5 73:7");

	ADD_INST(L"WFI",			L"105:C 0:5 0:3 0:5 73:7");


	// Zicsr
	ADD_IDER(RV32InstCSR, 1,	L"CSRRWXV,V,XV", L"{2:B:C} {3:4:5} 1:3 {1:4:5} 73:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_SPEC_TYPE, RV32ArgType::AT_RV32_REG);
	ADD_IDER(RV32InstCSR, 1,	L"CSRRSXV,V,XV", L"{2:B:C} {3:4:5} 2:3 {1:4:5} 73:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_SPEC_TYPE, RV32ArgType::AT_RV32_REG);
	ADD_IDER(RV32InstCSR, 1,	L"CSRRCXV,V,XV", L"{2:B:C} {3:4:5} 3:3 {1:4:5} 73:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_SPEC_TYPE, RV32ArgType::AT_RV32_REG);

	ADD_IDER(RV32InstCSR, 1,	L"CSRRWIXV,V,V", L"{2:B:C} {3:4:5} 5:3 {1:4:5} 73:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_SPEC_TYPE, RV32ArgType::AT_RV32_5BIT_UVAL);
	ADD_IDER(RV32InstCSR, 1,	L"CSRRSIXV,V,V", L"{2:B:C} {3:4:5} 6:3 {1:4:5} 73:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_SPEC_TYPE, RV32ArgType::AT_RV32_5BIT_UVAL);
	ADD_IDER(RV32InstCSR, 1,	L"CSRRCIXV,V,V", L"{2:B:C} {3:4:5} 7:3 {1:4:5} 73:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_SPEC_TYPE, RV32ArgType::AT_RV32_5BIT_UVAL);


	// Zmmul or M
	if(_global_settings.GetMultiplication())
	{
		ADD_INST(L"MULXV,XV,XV", L"1:7 {3:4:5} {2:4:5} 0:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
		ADD_INST(L"MULHXV,XV,XV", L"1:7 {3:4:5} {2:4:5} 1:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
		ADD_INST(L"MULHUXV,XV,XV", L"1:7 {3:4:5} {2:4:5} 3:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
		ADD_INST(L"MULHSUXV,XV,XV", L"1:7 {3:4:5} {2:4:5} 2:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	}

	if(_global_settings.GetDivision())
	{
		ADD_INST(L"DIVXV,XV,XV", L"1:7 {3:4:5} {2:4:5} 4:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
		ADD_INST(L"DIVUXV,XV,XV", L"1:7 {3:4:5} {2:4:5} 5:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
		ADD_INST(L"REMXV,XV,XV", L"1:7 {3:4:5} {2:4:5} 6:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
		ADD_INST(L"REMUXV,XV,XV", L"1:7 {3:4:5} {2:4:5} 7:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	}


	// compressed instructions to use instead of 32-bit ones
	if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst() && _global_settings.GetFixAddresses())
	{
		// C.J
		ADD_INSTI(0, L"JV", L"5:3 {1:B:1} {1:4:1} {1:9:2} {1:A:1} {1:6:1} {1:7:1} {1:3:3} {1:5:1} 1:2", RV32ArgType::AT_RV32_12BIT_OFF);
		// C.BEQZ
		ADD_INSTI(0, L"BEQXV,XV,V", L"6:3 {3:8:1} {3:4:2} {1:2:3} {3:7:2} {3:2:2} {3:5:1} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_9BIT_OFF);
		ADD_INSTI(0, L"BEQXV,XV,V", L"6:3 {3:8:1} {3:4:2} {2:2:3} {3:7:2} {3:2:2} {3:5:1} 1:2", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_9BIT_OFF);
		// C.BNEZ
		ADD_INSTI(0, L"BNEXV,XV,V", L"7:3 {3:8:1} {3:4:2} {1:2:3} {3:7:2} {3:2:2} {3:5:1} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_9BIT_OFF);
		ADD_INSTI(0, L"BNEXV,XV,V", L"7:3 {3:8:1} {3:4:2} {2:2:3} {3:7:2} {3:2:2} {3:5:1} 1:2", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_9BIT_OFF);
	}


	// pseudo-instructions
	// J <symbol20>: JAL X0, <symbol20>
	ADD_INSTI(1, L"JV",			L"{1:14:1} {1:A:A} {1:B:1} {1:13:8} 0:5 6F:7", RV32ArgType::AT_RV32_21BIT_OFF);
	// CALL <symbol32>: LUI X1, <symbol32>.H20 + JALR X1, <symbol32>.L12(X1)
	ADD_INST2I(2, L"CALLV",		L"{1.H20:13:8} {1.H20:B:8} {1.H20:3:4} 1:5 37:7 | {1.L12:B:4} {1.L12:7:8} 1:5 0:3 1:5 67:7", RV32ArgType::AT_RV32_4BYTE_VAL);
	// CALL <symbol20>: JAL X1, <symbol20>
	ADD_INSTI(1, L"CALLV",		L"{1:14:1} {1:A:A} {1:B:1} {1:13:8} 1:5 6F:7", RV32ArgType::AT_RV32_21BIT_OFF);
	// RET: JALR X0, 0(X1)
	ADD_INST(L"RET",			L"0:4 0:8 1:5 0:3 0:5 67:7");
	// LA rd, <symbol12>: ADDI rd, X0, <symbol12>
	ADD_INST(L"LAXV,V",			L"{2:B:4} {2:7:8} 0:5 0:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	// LA rd, <symbol32>: LUI rd, <symbol32>.H20 + ADDI rd, rd, <symbol32>.L12
	ADD_INST2(L"LAXV,V",		L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
	// (for <value32>.L12 == 0) LA rd, <value32>: LUI rd, <value32>.H20
	ADD_IDER(RV32Inst2L0, 1, L"LAXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
	// LI rd, <value12>: ADDI rd, X0, <value12>
	ADD_INST(L"LIXV,V",			L"{2:B:4} {2:7:8} 0:5 0:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	// LI rd, <value32>: LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12
	ADD_INST2(L"LIXV,V",		L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
	// (for <value32>.L12 == 0) LI rd, <value32>: LUI rd, <value32>.H20
	ADD_IDER(RV32Inst2L0, 1, L"LIXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
	if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst())
	{
		// (for <value32>.L12 == [-32..31]) LA rd, <value32>: LUI rd, <value32>.H20 + C.ADDI rd, <value32>.L12
		ADD_IDER(RV32Inst2L6, 2, L"LAXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 0:3 {2.L12:5:1} {1:4:5} {2.L12:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for <value32>.L12 == [-32..31]) LI rd, <value32>: LUI rd, <value32>.H20 + C.ADDI rd, <value32>.L12
		ADD_IDER(RV32Inst2L6, 2, L"LIXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 0:3 {2.L12:5:1} {1:4:5} {2.L12:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for <value32>.H20 == [-32..31]) LA rd, <value32>: C.LUI rd, <value32>.H20 + ADDI rd, <value32>.L12
		ADD_IDER(RV32Inst2H6NZ, 2, L"LAXV,V", L"3:3 {2.H20:5:1} {1:4:5} {2.H20:4:5} 1:2 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG_NZ_NSP, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for <value32>.H20 == [-32..31]) LI rd, <value32>: C.LUI rd, <value32>.H20 + ADDI rd, <value32>.L12
		ADD_IDER(RV32Inst2H6NZ, 2, L"LIXV,V", L"3:3 {2.H20:5:1} {1:4:5} {2.H20:4:5} 1:2 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG_NZ_NSP, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for <value32>.H20 == [-32..31], <value32>.L12 == 0) LA rd, <value32>: C.LUI rd, <value32>.H20
		ADD_IDER(RV32Inst2H6NZL0, 1, L"LAXV,V", L"3:3 {2.H20:5:1} {1:4:5} {2.H20:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ_NSP, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for <value32>.H20 == [-32..31], <value32>.L12 == 0) LI rd, <value32>: C.LUI rd, <value32>.H20
		ADD_IDER(RV32Inst2H6NZL0, 1, L"LIXV,V", L"3:3 {2.H20:5:1} {1:4:5} {2.H20:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ_NSP, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for <value32>.L12 == [-32..31], <value32>.H20 == [-32..31]) LA rd, <value32>: C.LUI rd, <value32>.H20 + C.ADDI rd, <value32>.L12
		ADD_IDER(RV32Inst2H6NZL6, 2, L"LAXV,V", L"3:3 {2.H20:5:1} {1:4:5} {2.H20:4:5} 1:2 | 0:3 {2.L12:5:1} {1:4:5} {2.L12:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ_NSP, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for <value32>.L12 == [-32..31], <value32>.H20 == [-32..31]) LI rd, <value32>: C.LUI rd, <value32>.H20 + C.ADDI rd, <value32>.L12
		ADD_IDER(RV32Inst2H6NZL6, 2, L"LIXV,V", L"3:3 {2.H20:5:1} {1:4:5} {2.H20:4:5} 1:2 | 0:3 {2.L12:5:1} {1:4:5} {2.L12:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ_NSP, RV32ArgType::AT_RV32_4BYTE_VAL);
	}
	// MV rd, rs: ADDI rd, rs, 0
	ADD_INST(L"MVXV,XV",		L"0:4 0:8 {2:4:5} 0:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	// NOP: ADDI X0, X0, 0
	ADD_INST(L"NOP",			L"0:4 0:8 0:5 0:3 0:5 13:7");
	// NOT rd, rs: XORI rd, rs, -1
	ADD_INST(L"NOTXV,XV",		L"F:B:4 FF:7:8 {2:4:5} 4:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	// NOT rs/rd: XORI rs/rd, rs/rd, -1
	ADD_INST(L"NOTXV",			L"F:B:4 FF:7:8 {1:4:5} 4:3 {1:4:5} 13:7", RV32ArgType::AT_RV32_REG);
	// NEG rd, rs: SUB rd, X0, rs
	ADD_INST(L"NEGXV,XV",		L"20:7 {2:4:5} 0:5 0:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG);
	// NEG rs/rd: SUB rs/rd, X0, rs/rd
	ADD_INST(L"NEGXV",			L"20:7 {1:4:5} 0:5 0:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG);
	// LB/LH/LW/LBU/LHU rd, <address12>: LB/LH/LW/LBU/LHU rd, <address12>(X0)
	ADD_INST(L"LBXV,V", L"{2:B:4} {2:7:8} 0:5 0:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	ADD_INST(L"LHXV,V", L"{2:B:4} {2:7:8} 0:5 1:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	ADD_INST(L"LWXV,V", L"{2:B:4} {2:7:8} 0:5 2:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	ADD_INST(L"LBUXV,V", L"{2:B:4} {2:7:8} 0:5 4:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	ADD_INST(L"LHUXV,V", L"{2:B:4} {2:7:8} 0:5 5:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	// SB/SH/SW rs, <address12>: SB/SH/SW rs, <address12>(X0)
	ADD_INST(L"SBXV,V", L"{2:B:7} {1:4:5} 0:5 0:3 {2:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	ADD_INST(L"SHXV,V", L"{2:B:7} {1:4:5} 0:5 1:3 {2:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);
	ADD_INST(L"SWXV,V", L"{2:B:7} {1:4:5} 0:5 2:3 {2:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_12BIT_VAL);

	if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst())
	{
		// RET: C.JR X1
		ADD_INST(L"RET", L"4:3 0:1 1:5 0:5 2:2");
		// CALL <offset12>: C.JAL <offset12>
		ADD_INSTCI(0, L"CALLV", L"1:3 {1:B:1} {1:4:1} {1:9:2} {1:A:1} {1:6:1} {1:7:1} {1:3:3} {1:5:1} 1:2", RV32ArgType::AT_RV32_12BIT_OFF);


		// LA rd, <imm6>: C.LI rd, <imm6>
		ADD_INSTC(L"LAXV,V", L"2:3 {2:5:1} {1:4:5} {2:4:5} 1:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_6BIT_VAL);
	}


	if(_global_settings.GetFixAddresses())
	{
		// (for rd != rs) ADDI rd, rs, <value32>: LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12 + ADD rd, rd, rs
		ADD_IDER(RV32Inst12Ne, 3, L"ADDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 0:7 {1:4:5} {2:4:5} 0:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for rd != rs, <value32>.L12 == 0) ADDI rd, rs, <value32>: LUI rd, <value32>.H20 + ADD rd, rd, rs
		ADD_IDER(RV32Inst12Ne3L0, 2, L"ADDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | 0:7 {1:4:5} {2:4:5} 0:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);

		if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst())
		{
			// (for rd != rs) ADDI rd, rs, <value32>: LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12 + C.ADD rd, rs
			ADD_IDER(RV32Inst12Ne, 3, L"ADDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 4:3 1:1 {1:4:5} {2:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.L12 == 0) ADDI rd, rs, <value32>: LUI rd, <value32>.H20 + C.ADD rd, rs
			ADD_IDER(RV32Inst12Ne3L0, 2, L"ADDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | 4:3 1:1 {1:4:5} {2:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);

			// (for rd != rs, <value32>.H20 == [-32..31], <value32>.L12 == 0) ADDI rd, rs, <value32>: C.LUI rd, <value32>.H20 + C.ADD rd, rs
			ADD_IDER(RV32Inst12Ne3H6NZL0, 2, L"ADDIXV,XV,V", L"3:3 {3.H20:5:1} {1:4:5} {3.H20:4:5} 1:2 | 4:3 1:1 {1:4:5} {2:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ_NSP, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.H20 == [-32..31], <value32>.L12 == [-32..31]) ADDI rd, rs, <value32>: C.LUI rd, <value32>.H20 + C.ADDI rd, <value32>.L12 + C.ADD rd, rs
			ADD_IDER(RV32Inst12Ne3H6NZL6, 3, L"ADDIXV,XV,V", L"3:3 {3.H20:5:1} {1:4:5} {3.H20:4:5} 1:2 | 0:3 {3.L12:5:1} {1:4:5} {3.L12:4:5} 1:2 | 4:3 1:1 {1:4:5} {2:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ_NSP, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.L12 == [-32..31]) ADDI rd, rs, <value32>: LUI rd, <value32>.H20 + C.ADDI rd, <value32>.L12 + C.ADD rd, rs
			ADD_IDER(RV32Inst12Ne3L6, 3, L"ADDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | 0:3 {3.L12:5:1} {1:4:5} {3.L12:4:5} 1:2 | 4:3 1:1 {1:4:5} {2:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.H20 == [-32..31]) ADDI rd, rs, <value32>: C.LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12 + C.ADD rd, rs
			ADD_IDER(RV32Inst12Ne3H6NZ, 3, L"ADDIXV,XV,V", L"3:3 {3.H20:5:1} {1:4:5} {3.H20:4:5} 1:2 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 4:3 1:1 {1:4:5} {2:4:5} 2:2", RV32ArgType::AT_RV32_REG_NZ_NSP, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
		}


		// (for rd != rs) ORI rd, rs, <value32>: LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12 + OR rd, rd, rs
		ADD_IDER(RV32Inst12Ne, 3, L"ORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 0:7 {2:4:5} {1:4:5} 6:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for rd != rs, <value32>.L12 == 0) ORI rd, rs, <value32>: LUI rd, <value32>.H20 + OR rd, rd, rs
		ADD_IDER(RV32Inst12Ne3L0, 2, L"ORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | 0:7 {2:4:5} {1:4:5} 6:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);

		if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst())
		{
			// (for rd != rs) ORI rd, rs, <value32>: LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12 + C.OR rd, rs
			ADD_IDER(RV32Inst12Ne, 3, L"ORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 4:3 0:1 3:2 {1:2:3} 2:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.L12 == 0) ORI rd, rs, <value32>: LUI rd, <value32>.H20 + C.OR rd, rs
			ADD_IDER(RV32Inst12Ne3L0, 2, L"ORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | 4:3 0:1 3:2 {1:2:3} 2:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);

			// (for rd != rs, <value32>.H20 == [-32..31], <value32>.L12 == 0) ORI rd, rs, <value32>: C.LUI rd, <value32>.H20 + C.OR rd, rs
			ADD_IDER(RV32Inst12Ne3H6NZL0, 2, L"ORIXV,XV,V", L"3:3 {3.H20:5:1} {1:4:5} {3.H20:4:5} 1:2 | 4:3 0:1 3:2 {1:2:3} 2:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.H20 == [-32..31], <value32>.L12 == [-32..31]) ORI rd, rs, <value32>: C.LUI rd, <value32>.H20 + C.ADDI rd, <value32>.L12 + C.OR rd, rs
			ADD_IDER(RV32Inst12Ne3H6NZL6, 3, L"ORIXV,XV,V", L"3:3 {3.H20:5:1} {1:4:5} {3.H20:4:5} 1:2 | 0:3 {3.L12:5:1} {1:4:5} {3.L12:4:5} 1:2 | 4:3 0:1 3:2 {1:2:3} 2:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.L12 == [-32..31]) ORI rd, rs, <value32>: LUI rd, <value32>.H20 + C.ADDI rd, <value32>.L12 + C.OR rd, rs
			ADD_IDER(RV32Inst12Ne3L6, 3, L"ORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | 0:3 {3.L12:5:1} {1:4:5} {3.L12:4:5} 1:2 | 4:3 0:1 3:2 {1:2:3} 2:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.H20 == [-32..31]) ORI rd, rs, <value32>: C.LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12 + C.OR rd, rs
			ADD_IDER(RV32Inst12Ne3H6NZ, 3, L"ORIXV,XV,V", L"3:3 {3.H20:5:1} {1:4:5} {3.H20:4:5} 1:2 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 4:3 0:1 3:2 {1:2:3} 2:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		}


		// (for rd != rs) XORI rd, rs, <value32>: LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12 + XOR rd, rd, rs
		ADD_IDER(RV32Inst12Ne, 3, L"XORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 0:7 {2:4:5} {1:4:5} 4:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for rd != rs, <value32>.L12 == 0) XORI rd, rs, <value32>: LUI rd, <value32>.H20 + XOR rd, rd, rs
		ADD_IDER(RV32Inst12Ne3L0, 2, L"XORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | 0:7 {2:4:5} {1:4:5} 4:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);

		if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst())
		{
			// (for rd != rs) XORI rd, rs, <value32>: LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12 + C.XOR rd, rs
			ADD_IDER(RV32Inst12Ne, 3, L"XORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 4:3 0:1 3:2 {1:2:3} 1:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.L12 == 0) XORI rd, rs, <value32>: LUI rd, <value32>.H20 + C.XOR rd, rs
			ADD_IDER(RV32Inst12Ne3L0, 2, L"XORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | 4:3 0:1 3:2 {1:2:3} 1:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);

			// (for rd != rs, <value32>.H20 == [-32..31], <value32>.L12 == 0) XORI rd, rs, <value32>: C.LUI rd, <value32>.H20 + C.XOR rd, rs
			ADD_IDER(RV32Inst12Ne3H6NZL0, 2, L"XORIXV,XV,V", L"3:3 {3.H20:5:1} {1:4:5} {3.H20:4:5} 1:2 | 4:3 0:1 3:2 {1:2:3} 1:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.H20 == [-32..31], <value32>.L12 == [-32..31]) XORI rd, rs, <value32>: C.LUI rd, <value32>.H20 + C.ADDI rd, <value32>.L12 + C.XOR rd, rs
			ADD_IDER(RV32Inst12Ne3H6NZL6, 3, L"XORIXV,XV,V", L"3:3 {3.H20:5:1} {1:4:5} {3.H20:4:5} 1:2 | 0:3 {3.L12:5:1} {1:4:5} {3.L12:4:5} 1:2 | 4:3 0:1 3:2 {1:2:3} 1:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.L12 == [-32..31]) XORI rd, rs, <value32>: LUI rd, <value32>.H20 + C.ADDI rd, <value32>.L12 + C.XOR rd, rs
			ADD_IDER(RV32Inst12Ne3L6, 3, L"XORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | 0:3 {3.L12:5:1} {1:4:5} {3.L12:4:5} 1:2 | 4:3 0:1 3:2 {1:2:3} 1:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.H20 == [-32..31]) XORI rd, rs, <value32>: C.LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12 + C.XOR rd, rs
			ADD_IDER(RV32Inst12Ne3H6NZ, 3, L"XORIXV,XV,V", L"3:3 {3.H20:5:1} {1:4:5} {3.H20:4:5} 1:2 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 4:3 0:1 3:2 {1:2:3} 1:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		}


		// (for rd != rs) ANDI rd, rs, <value32>: LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12 + AND rd, rd, rs
		ADD_IDER(RV32Inst12Ne, 3, L"ANDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 0:7 {2:4:5} {1:4:5} 7:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for rd != rs, <value32>.L12 == 0) ANDI rd, rs, <value32>: LUI rd, <value32>.H20 + AND rd, rd, rs
		ADD_IDER(RV32Inst12Ne3L0, 2, L"ANDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | 0:7 {2:4:5} {1:4:5} 7:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);

		if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst())
		{
			// (for rd != rs) ANDI rd, rs, <value32>: LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12 + C.AND rd, rs
			ADD_IDER(RV32Inst12Ne, 3, L"ANDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 4:3 0:1 3:2 {1:2:3} 3:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.L12 == 0) ANDI rd, rs, <value32>: LUI rd, <value32>.H20 + C.AND rd, rs
			ADD_IDER(RV32Inst12Ne3L0, 2, L"ANDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | 4:3 0:1 3:2 {1:2:3} 3:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);

			// (for rd != rs, <value32>.H20 == [-32..31], <value32>.L12 == 0) ANDI rd, rs, <value32>: C.LUI rd, <value32>.H20 + C.AND rd, rs
			ADD_IDER(RV32Inst12Ne3H6NZL0, 2, L"ANDIXV,XV,V", L"3:3 {3.H20:5:1} {1:4:5} {3.H20:4:5} 1:2 | 4:3 0:1 3:2 {1:2:3} 3:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.H20 == [-32..31], <value32>.L12 == [-32..31]) ANDI rd, rs, <value32>: C.LUI rd, <value32>.H20 + C.ADDI rd, <value32>.L12 + C.AND rd, rs
			ADD_IDER(RV32Inst12Ne3H6NZL6, 3, L"ANDIXV,XV,V", L"3:3 {3.H20:5:1} {1:4:5} {3.H20:4:5} 1:2 | 0:3 {3.L12:5:1} {1:4:5} {3.L12:4:5} 1:2 | 4:3 0:1 3:2 {1:2:3} 3:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.L12 == [-32..31]) ANDI rd, rs, <value32>: LUI rd, <value32>.H20 + C.ADDI rd, <value32>.L12 + C.AND rd, rs
			ADD_IDER(RV32Inst12Ne3L6, 3, L"ANDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} {1:4:5} 37:7 | 0:3 {3.L12:5:1} {1:4:5} {3.L12:4:5} 1:2 | 4:3 0:1 3:2 {1:2:3} 3:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd != rs, <value32>.H20 == [-32..31]) ANDI rd, rs, <value32>: C.LUI rd, <value32>.H20 + ADDI rd, rd, <value32>.L12 + C.AND rd, rs
			ADD_IDER(RV32Inst12Ne3H6NZ, 3, L"ANDIXV,XV,V", L"3:3 {3.H20:5:1} {1:4:5} {3.H20:4:5} 1:2 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 4:3 0:1 3:2 {1:2:3} 3:2 {2:2:3} 1:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		}


		// LB rd, <symbol32>: LUI rd, <symbol32>.H20 + LB rd, <symbol32>.L12(rd)
		ADD_INST2(L"LBXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 0:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
		// LH rd, <symbol32>: LUI rd, <symbol32>.H20 + LH rd, <symbol32>.L12(rd)
		ADD_INST2(L"LHXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 1:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
		// LW rd, <symbol32>: LUI rd, <symbol32>.H20 + LW rd, <symbol32>.L12(rd)
		ADD_INST2(L"LWXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 2:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
		// LBU rd, <symbol32>: LUI rd, <symbol32>.H20 + LBU rd, <symbol32>.L12(rd)
		ADD_INST2(L"LBUXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 4:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
		// LHU rd, <symbol32>: LUI rd, <symbol32>.H20 + LHU rd, <symbol32>.L12(rd)
		ADD_INST2(L"LHUXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 5:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);

		// (for rd != rs) LB rd, <value32>(rs): LUI rd, <value32>.H20 + ADD rd, rd, rs + LB rd, <value32>.L12(rd)
		ADD_IDER(RV32Inst13Ne, 3, L"LBXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 0:7 {3:4:5} {1:4:5} 0:3 {1:4:5} 33:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 0:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
		// (for rd != rs) LH rd, <value32>(rs): LUI rd, <value32>.H20 + ADD rd, rd, rs + LH rd, <value32>.L12(rd)
		ADD_IDER(RV32Inst13Ne, 3, L"LHXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 0:7 {3:4:5} {1:4:5} 0:3 {1:4:5} 33:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 1:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
		// (for rd != rs) LW rd, <value32>(rs): LUI rd, <value32>.H20 + ADD rd, rd, rs + LW rd, <value32>.L12(rd)
		ADD_IDER(RV32Inst13Ne, 3, L"LWXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 0:7 {3:4:5} {1:4:5} 0:3 {1:4:5} 33:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 2:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
		// (for rd != rs) LBU rd, <value32>(rs): LUI rd, <value32>.H20 + ADD rd, rd, rs + LBU rd, <value32>.L12(rd)
		ADD_IDER(RV32Inst13Ne, 3, L"LBUXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 0:7 {3:4:5} {1:4:5} 0:3 {1:4:5} 33:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 4:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
		// (for rd != rs) LHU rd, <value32>(rs): LUI rd, <value32>.H20 + ADD rd, rd, rs + LHU rd, <value32>.L12(rd)
		ADD_IDER(RV32Inst13Ne, 3, L"LHUXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 0:7 {3:4:5} {1:4:5} 0:3 {1:4:5} 33:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 5:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);

		if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst())
		{
			// (for rd != rs) LB rd, <value32>(rs): LUI rd, <value32>.H20 + C.ADD rd, rs + LB rd, <value32>.L12(rd)
			ADD_IDERC(RV32Inst13Ne, 3, L"LBXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 4:3 1:1 {1:4:5} {3:4:5} 2:2 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 0:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
			// (for rd != rs) LH rd, <value32>(rs): LUI rd, <value32>.H20 + C.ADD rd, rs + LH rd, <value32>.L12(rd)
			ADD_IDERC(RV32Inst13Ne, 3, L"LHXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 4:3 1:1 {1:4:5} {3:4:5} 2:2 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 1:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
			// (for rd != rs) LW rd, <value32>(rs): LUI rd, <value32>.H20 + C.ADD rd, rs + LW rd, <value32>.L12(rd)
			ADD_IDERC(RV32Inst13Ne, 3, L"LWXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 4:3 1:1 {1:4:5} {3:4:5} 2:2 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 2:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
			// (for rd != rs) LBU rd, <value32>(rs): LUI rd, <value32>.H20 + C.ADD rd, rs + LBU rd, <value32>.L12(rd)
			ADD_IDERC(RV32Inst13Ne, 3, L"LBUXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 4:3 1:1 {1:4:5} {3:4:5} 2:2 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 4:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
			// (for rd != rs) LHU rd, <value32>(rs): LUI rd, <value32>.H20 + C.ADD rd, rs + LHU rd, <value32>.L12(rd)
			ADD_IDER(RV32Inst13Ne, 3, L"LHUXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 4:3 1:1 {1:4:5} {3:4:5} 2:2 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 5:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);

			// (for rd' != rs, <value32>.L12 == [0..127], <value32>.L12 is a multiple of 4) LW rd', <value32>(rs): LUI rd', <value32>.H20 + C.ADD rd', rs + C.LW rd', <value32>.L12(rd')
			ADD_IDER(RV32Inst13Ne2LU7M4, 3, L"LWXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 4:3 1:1 {1:4:5} {3:4:5} 2:2 | 2:3 {2.L12:5:3} {1:2:3} {2.L12:2:1} {2.L12:6:1} {1:2:3} 0:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);

			// (for <value32>.L12 == [0..127], <value32>.L12 is a multiple of 4) LW rd', <symbol32>: LUI rd', <symbol32>.H20 + C.LW rd', <symbol32>.L12(rd')
			ADD_IDER(RV32Inst2LU7M4, 2, L"LWXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} {1:4:5} 37:7 | 2:3 {2.L12:5:3} {1:2:3} {2.L12:2:1} {2.L12:6:1} {1:2:3} 0:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		}


		// pseudo-instructions below use T0 register
		// (for rd == rs) ADDI r, r, <value32>: LUI T0, <value32>.H20 + ADDI r, r, <value32>.L12 + ADD r, r, T0
		ADD_IDER(RV32Inst12EqNT0, 3, L"ADDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 0:7 5:5 {2:4:5} 0:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for rd == rs, <value32>.L12 == 0) ADDI r, r, <value32>: LUI T0, <value32>.H20 + ADD r, r, T0
		ADD_IDER(RV32Inst12Eq3L0, 2, L"ADDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | 0:7 5:5 {2:4:5} 0:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);

		if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst())
		{
			// (for rd == rs) ADDI r, r, <value32>: LUI T0, <value32>.H20 + ADDI r, r, <value32>.L12 + C.ADD r, T0
			ADD_IDER(RV32Inst12EqNT0, 3, L"ADDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 4:3 1:1 {1:4:5} 5:5 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.L12 == 0) ADDI r, r, <value32>: LUI T0, <value32>.H20 + C.ADD r, T0
			ADD_IDER(RV32Inst12Eq3L0, 2, L"ADDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | 4:3 1:1 {1:4:5} 5:5 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);

			// (for rd == rs, <value32>.H20 == [-32..31], <value32>.L12 == 0) ADDI r, r, <value32>: C.LUI T0, <value32>.H20 + C.ADD r, T0
			ADD_IDER(RV32Inst12Eq3H6NZL0, 2, L"ADDIXV,XV,V", L"3:3 {3.H20:5:1} 5:5 {3.H20:4:5} 1:2 | 4:3 1:1 {1:4:5} 5:5 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.H20 == [-32..31], <value32>.L12 == [-32..31]) ADDI r, r, <value32>: C.LUI T0, <value32>.H20 + C.ADDI r, <value32>.L12 + C.ADD r, T0
			ADD_IDER(RV32Inst12Eq3H6NZL6, 3, L"ADDIXV,XV,V", L"3:3 {3.H20:5:1} 5:5 {3.H20:4:5} 1:2 | 0:3 {3.L12:5:1} {1:4:5} {3.L12:4:5} 1:2 | 4:3 1:1 {1:4:5} 5:5 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.L12 == [-32..31]) ADDI r, r, <value32>: LUI T0, <value32>.H20 + C.ADDI r, <value32>.L12 + C.ADD r, T0
			ADD_IDER(RV32Inst12Eq3L6, 3, L"ADDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | 0:3 {3.L12:5:1} {1:4:5} {3.L12:4:5} 1:2 | 4:3 1:1 {1:4:5} 5:5 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.H20 == [-32..31]) ADDI r, r, <value32>: C.LUI T0, <value32>.H20 + ADDI r, r, <value32>.L12 + C.ADD r, T0
			ADD_IDER(RV32Inst12Eq3H6NZ, 3, L"ADDIXV,XV,V", L"3:3 {3.H20:5:1} 5:5 {3.H20:4:5} 1:2 | {3.L12:B:4} {3.L12:7:8} {1:4:5} 0:3 {1:4:5} 13:7 | 4:3 1:1 {1:4:5} 5:5 2:2", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL);
		}


		// (for rd == rs) ORI r, r, <value32>: LUI T0, <value32>.H20 + ADDI T0, T0, <value32>.L12 + OR r, r, T0
		ADD_IDER(RV32Inst12EqNT0, 3, L"ORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | {3.L12:B:4} {3.L12:7:8} 5:5 0:3 5:5 13:7 | 0:7 5:5 {1:4:5} 6:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for rd == rs, <value32>.L12 == 0) ORI r, r, <value32>: LUI T0, <value32>.H20 + OR r, r, T0
		ADD_IDER(RV32Inst12Eq3L0, 2, L"ORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | 0:7 5:5 {1:4:5} 6:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);

		if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst())
		{
			// (for rd == rs, <value32>.H20 == [-32..31], <value32>.L12 == 0) ORI r, r, <value32>: C.LUI T0, <value32>.H20 + OR r, r, T0
			ADD_IDER(RV32Inst12Eq3H6NZL0, 2, L"ORIXV,XV,V", L"3:3 {3.H20:5:1} 5:5 {3.H20:4:5} 1:2 | 0:7 5:5 {1:4:5} 6:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.H20 == [-32..31], <value32>.L12 == [-32..31]) ORI r, r, <value32>: C.LUI T0, <value32>.H20 + C.ADDI T0, <value32>.L12 + OR r, r, T0
			ADD_IDER(RV32Inst12Eq3H6NZL6, 3, L"ORIXV,XV,V", L"3:3 {3.H20:5:1} 5:5 {3.H20:4:5} 1:2 | 0:3 {3.L12:5:1} 5:5 {3.L12:4:5} 1:2 | 0:7 5:5 {1:4:5} 6:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.L12 == [-32..31]) ORI r, r, <value32>: LUI T0, <value32>.H20 + C.ADDI T0, <value32>.L12 + OR r, r, T0
			ADD_IDER(RV32Inst12Eq3L6, 3, L"ORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | 0:3 {3.L12:5:1} 5:5 {3.L12:4:5} 1:2 | 0:7 5:5 {1:4:5} 6:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.H20 == [-32..31]) ORI r, r, <value32>: C.LUI T0, <value32>.H20 + ADDI T0, T0, <value32>.L12 + OR r, r, T0
			ADD_IDER(RV32Inst12Eq3H6NZ, 3, L"ORIXV,XV,V", L"3:3 {3.H20:5:1} 5:5 {3.H20:4:5} 1:2 | {3.L12:B:4} {3.L12:7:8} 5:5 0:3 5:5 13:7 | 0:7 5:5 {1:4:5} 6:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		}


		// (for rd == rs) XORI r, r, <value32>: LUI T0, <value32>.H20 + ADDI T0, T0, <value32>.L12 + XOR r, r, T0
		ADD_IDER(RV32Inst12EqNT0, 3, L"XORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | {3.L12:B:4} {3.L12:7:8} 5:5 0:3 5:5 13:7 | 0:7 5:5 {1:4:5} 4:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for rd == rs, <value32>.L12 == 0) XORI r, r, <value32>: LUI T0, <value32>.H20 + XOR r, r, T0
		ADD_IDER(RV32Inst12Eq3L0, 2, L"XORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | 0:7 5:5 {1:4:5} 4:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);

		if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst())
		{
			// (for rd == rs, <value32>.H20 == [-32..31], <value32>.L12 == 0) XORI r, r, <value32>: C.LUI T0, <value32>.H20 + XOR r, r, T0
			ADD_IDER(RV32Inst12Eq3H6NZL0, 2, L"XORIXV,XV,V", L"3:3 {3.H20:5:1} 5:5 {3.H20:4:5} 1:2 | 0:7 5:5 {1:4:5} 4:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.H20 == [-32..31], <value32>.L12 == [-32..31]) XORI r, r, <value32>: C.LUI T0, <value32>.H20 + C.ADDI T0, <value32>.L12 + XOR r, r, T0
			ADD_IDER(RV32Inst12Eq3H6NZL6, 3, L"XORIXV,XV,V", L"3:3 {3.H20:5:1} 5:5 {3.H20:4:5} 1:2 | 0:3 {3.L12:5:1} 5:5 {3.L12:4:5} 1:2 | 0:7 5:5 {1:4:5} 4:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.L12 == [-32..31]) XORI r, r, <value32>: LUI T0, <value32>.H20 + C.ADDI T0, <value32>.L12 + XOR r, r, T0
			ADD_IDER(RV32Inst12Eq3L6, 3, L"XORIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | 0:3 {3.L12:5:1} 5:5 {3.L12:4:5} 1:2 | 0:7 5:5 {1:4:5} 4:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.H20 == [-32..31]) XORI r, r, <value32>: C.LUI T0, <value32>.H20 + ADDI T0, T0, <value32>.L12 + XOR r, r, T0
			ADD_IDER(RV32Inst12Eq3H6NZ, 3, L"XORIXV,XV,V", L"3:3 {3.H20:5:1} 5:5 {3.H20:4:5} 1:2 | {3.L12:B:4} {3.L12:7:8} 5:5 0:3 5:5 13:7 | 0:7 5:5 {1:4:5} 4:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		}


		// (for rd == rs) ANDI r, r, <value32>: LUI T0, <value32>.H20 + ADDI T0, T0, <value32>.L12 + AND r, r, T0
		ADD_IDER(RV32Inst12EqNT0, 3, L"ANDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | {3.L12:B:4} {3.L12:7:8} 5:5 0:3 5:5 13:7 | 0:7 5:5 {1:4:5} 7:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		// (for rd == rs, <value32>.L12 == 0) ANDI r, r, <value32>: LUI T0, <value32>.H20 + AND r, r, T0
		ADD_IDER(RV32Inst12Eq3L0, 2, L"ANDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | 0:7 5:5 {1:4:5} 7:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);

		if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst())
		{
			// (for rd == rs, <value32>.H20 == [-32..31], <value32>.L12 == 0) ANDI r, r, <value32>: C.LUI T0, <value32>.H20 + AND r, r, T0
			ADD_IDER(RV32Inst12Eq3H6NZL0, 2, L"ANDIXV,XV,V", L"3:3 {3.H20:5:1} 5:5 {3.H20:4:5} 1:2 | 0:7 5:5 {1:4:5} 7:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.H20 == [-32..31], <value32>.L12 == [-32..31]) ANDI r, r, <value32>: C.LUI T0, <value32>.H20 + C.ADDI T0, <value32>.L12 + AND r, r, T0
			ADD_IDER(RV32Inst12Eq3H6NZL6, 3, L"ANDIXV,XV,V", L"3:3 {3.H20:5:1} 5:5 {3.H20:4:5} 1:2 | 0:3 {3.L12:5:1} 5:5 {3.L12:4:5} 1:2 | 0:7 5:5 {1:4:5} 7:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.L12 == [-32..31]) ANDI r, r, <value32>: LUI T0, <value32>.H20 + C.ADDI T0, <value32>.L12 + AND r, r, T0
			ADD_IDER(RV32Inst12Eq3L6, 3, L"ANDIXV,XV,V", L"{3.H20:13:8} {3.H20:B:8} {3.H20:3:4} 5:5 37:7 | 0:3 {3.L12:5:1} 5:5 {3.L12:4:5} 1:2 | 0:7 5:5 {1:4:5} 7:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
			// (for rd == rs, <value32>.H20 == [-32..31]) ANDI r, r, <value32>: C.LUI T0, <value32>.H20 + ADDI T0, T0, <value32>.L12 + AND r, r, T0
			ADD_IDER(RV32Inst12Eq3H6NZ, 3, L"ANDIXV,XV,V", L"3:3 {3.H20:5:1} 5:5 {3.H20:4:5} 1:2 | {3.L12:B:4} {3.L12:7:8} 5:5 0:3 5:5 13:7 | 0:7 5:5 {1:4:5} 7:3 {1:4:5} 33:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		}


		// SB rs, <symbol32>: LUI T0, <symbol32>.H20 + SB rs, <symbol32>.L12(T0)
		ADD_INST2(L"SBXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | {2.L12:B:7} {1:4:5} 5:5 0:3 {2.L12:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		// SH rs, <symbol32>: LUI T0, <symbol32>.H20 + SH rs, <symbol32>.L12(T0)
		ADD_INST2(L"SHXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | {2.L12:B:7} {1:4:5} 5:5 1:3 {2.L12:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);
		// SW rs, <symbol32>: LUI T0, <symbol32>.H20 + SW rs, <symbol32>.L12(T0)
		ADD_INST2(L"SWXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | {2.L12:B:7} {1:4:5} 5:5 2:3 {2.L12:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL);

		// SB rs1, <value32>(rs2): LUI T0, <value32>.H20 + ADD T0, T0, rs2 + SB rs1, <value32>.L12(T0)
		ADD_IDER(RV32Inst13NeNT0, 3, L"SBXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 {3:4:5} 5:5 0:3 5:5 33:7 | {2.L12:B:7} {1:4:5} 5:5 0:3 {2.L12:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
		// SH rs1, <value32>(rs2): LUI T0, <value32>.H20 + ADD T0, T0, rs2 + SH rs1, <value32>.L12(T0)
		ADD_IDER(RV32Inst13NeNT0, 3, L"SHXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 {3:4:5} 5:5 0:3 5:5 33:7 | {2.L12:B:7} {1:4:5} 5:5 1:3 {2.L12:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
		// SW rs1, <value32>(rs2): LUI T0, <value32>.H20 + ADD T0, T0, rs2 + SW rs1, <value32>.L12(T0)
		ADD_IDER(RV32Inst13NeNT0, 3, L"SWXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 {3:4:5} 5:5 0:3 5:5 33:7 | {2.L12:B:7} {1:4:5} 5:5 2:3 {2.L12:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);

		// (for rd == rs) LB r, <value32>(r): LUI T0, <value32>.H20 + ADD r, r, T0 + LB r, <value32>.L12(r)
		ADD_IDER(RV32Inst13EqNT0, 3, L"LBXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 5:5 {3:4:5} 0:3 {3:4:5} 33:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 0:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
		// (for rd == rs) LH r, <value32>(r): LUI T0, <value32>.H20 + ADD r, r, T0 + LH r, <value32>.L12(r)
		ADD_IDER(RV32Inst13EqNT0, 3, L"LHXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 5:5 {3:4:5} 0:3 {3:4:5} 33:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 1:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
		// (for rd == rs) LW r, <value32>(r): LUI T0, <value32>.H20 + ADD r, r, T0 + LW r, <value32>.L12(r)
		ADD_IDER(RV32Inst13EqNT0, 3, L"LWXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 5:5 {3:4:5} 0:3 {3:4:5} 33:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 2:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
		// (for rd == rs) LBU r, <value32>(r): LUI T0, <value32>.H20 + ADD r, r, T0 + LBU r, <value32>.L12(r)
		ADD_IDER(RV32Inst13EqNT0, 3, L"LBUXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 5:5 {3:4:5} 0:3 {3:4:5} 33:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 4:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
		// (for rd == rs) LHU r, <value32>(r): LUI T0, <value32>.H20 + ADD r, r, T0 + LHU r, <value32>.L12(r)
		ADD_IDER(RV32Inst13EqNT0, 3, L"LHUXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 5:5 {3:4:5} 0:3 {3:4:5} 33:7 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 5:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);

		// LB ZERO, <symbol32>: LUI T0, <symbol32>.H20 + LB ZERO, <symbol32>.L12(T0)
		ADD_INST2(L"LBXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | {2.L12:B:4} {2.L12:7:8} 5:5 0:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL);
		// LH ZERO, <symbol32>: LUI T0, <symbol32>.H20 + LH ZERO, <symbol32>.L12(T0)
		ADD_INST2(L"LHXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | {2.L12:B:4} {2.L12:7:8} 5:5 1:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL);
		// LW ZERO, <symbol32>: LUI T0, <symbol32>.H20 + LW ZERO, <symbol32>.L12(T0)
		ADD_INST2(L"LWXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | {2.L12:B:4} {2.L12:7:8} 5:5 2:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL);
		// LBU ZERO, <symbol32>: LUI T0, <symbol32>.H20 + LBU ZERO, <symbol32>.L12(T0)
		ADD_INST2(L"LBUXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | {2.L12:B:4} {2.L12:7:8} 5:5 4:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL);
		// LHU ZERO, <symbol32>: LUI T0, <symbol32>.H20 + LHU ZERO, <symbol32>.L12(T0)
		ADD_INST2(L"LHUXV,V", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | {2.L12:B:4} {2.L12:7:8} 5:5 5:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL);

		// LB ZERO, <value32>(rs): LUI T0, <value32>.H20 + ADD T0, T0, rs + LB ZERO, <value32>.L12(T0)
		ADD_IDER(RV32Inst13NeNT0, 3, L"LBXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 {3:4:5} 5:5 0:3 5:5 33:7 | {2.L12:B:4} {2.L12:7:8} 5:5 0:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
		// LH ZERO, <value32>(rs): LUI T0, <value32>.H20 + ADD T0, T0, rs + LH ZERO, <value32>.L12(T0)
		ADD_IDER(RV32Inst13NeNT0, 3, L"LHXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 {3:4:5} 5:5 0:3 5:5 33:7 | {2.L12:B:4} {2.L12:7:8} 5:5 1:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
		// LW ZERO, <value32>(rs): LUI T0, <value32>.H20 + ADD T0, T0, rs + LW ZERO, <value32>.L12(T0)
		ADD_IDER(RV32Inst13NeNT0, 3, L"LWXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 {3:4:5} 5:5 0:3 5:5 33:7 | {2.L12:B:4} {2.L12:7:8} 5:5 2:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
		// LBU ZERO, <value32>(rs): LUI T0, <value32>.H20 + ADD T0, T0, rs + LBU ZERO, <value32>.L12(T0)
		ADD_IDER(RV32Inst13NeNT0, 3, L"LBUXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 {3:4:5} 5:5 0:3 5:5 33:7 | {2.L12:B:4} {2.L12:7:8} 5:5 4:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
		// LHU ZERO, <value32>(rs): LUI T0, <value32>.H20 + ADD T0, T0, rs + LHU ZERO, <value32>.L12(T0)
		ADD_IDER(RV32Inst13NeNT0, 3, L"LHUXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 0:7 {3:4:5} 5:5 0:3 5:5 33:7 | {2.L12:B:4} {2.L12:7:8} 5:5 5:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);

		if(_global_settings.GetCompressed() && global_settings.GetAutoCompInst())
		{
			// SB rs1, <value32>(rs2): LUI T0, <value32>.H20 + C.ADD T0, rs2 + SB rs1, <value32>.L12(T0)
			ADD_IDER(RV32Inst13NeNT0, 3, L"SBXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 5:5 {3:4:5} 2:2 | {2.L12:B:7} {1:4:5} 5:5 0:3 {2.L12:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
			// SH rs1, <value32>(rs2): LUI T0, <value32>.H20 + C.ADD T0, rs2 + SH rs1, <value32>.L12(T0)
			ADD_IDER(RV32Inst13NeNT0, 3, L"SHXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 5:5 {3:4:5} 2:2 | {2.L12:B:7} {1:4:5} 5:5 1:3 {2.L12:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
			// SW rs1, <value32>(rs2): LUI T0, <value32>.H20 + C.ADD T0, rs2 + SW rs1, <value32>.L12(T0)
			ADD_IDER(RV32Inst13NeNT0, 3, L"SWXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 5:5 {3:4:5} 2:2 | {2.L12:B:7} {1:4:5} 5:5 2:3 {2.L12:4:5} 23:7", RV32ArgType::AT_RV32_REG, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);

			// (for rd == rs) LB r, <value32>(r): LUI T0, <value32>.H20 + C.ADD r, T0 + LB r, <value32>.L12(r)
			ADD_IDER(RV32Inst13EqNT0, 3, L"LBXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 {1:4:5} 5:5 2:2 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 0:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
			// (for rd == rs) LH r, <value32>(r): LUI T0, <value32>.H20 + C.ADD r, T0 + LH r, <value32>.L12(r)
			ADD_IDER(RV32Inst13EqNT0, 3, L"LHXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 {1:4:5} 5:5 2:2 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 1:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
			// (for rd == rs) LW r, <value32>(r): LUI T0, <value32>.H20 + C.ADD r, T0 + LW r, <value32>.L12(r)
			ADD_IDER(RV32Inst13EqNT0, 3, L"LWXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 {1:4:5} 5:5 2:2 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 2:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
			// (for rd == rs) LBU r, <value32>(r): LUI T0, <value32>.H20 + C.ADD r, T0 + LBU r, <value32>.L12(r)
			ADD_IDER(RV32Inst13EqNT0, 3, L"LBUXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 {1:4:5} 5:5 2:2 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 4:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);
			// (for rd == rs) LHU r, <value32>(r): LUI T0, <value32>.H20 + C.ADD r, T0 + LHU r, <value32>.L12(r)
			ADD_IDER(RV32Inst13EqNT0, 3, L"LHUXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 {1:4:5} 5:5 2:2 | {2.L12:B:4} {2.L12:7:8} {1:4:5} 5:3 {1:4:5} 3:7", RV32ArgType::AT_RV32_REG_NZ, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG_NZ);

			// LB ZERO, <value32>(rs): LUI T0, <value32>.H20 + C.ADD T0, rs + LB ZERO, <value32>.L12(T0)
			ADD_IDER(RV32Inst13NeNT0, 3, L"LBXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 5:5 {3:4:5} 2:2 | {2.L12:B:4} {2.L12:7:8} 5:5 0:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
			// LH ZERO, <value32>(rs): LUI T0, <value32>.H20 + C.ADD T0, rs + LH ZERO, <value32>.L12(T0)
			ADD_IDER(RV32Inst13NeNT0, 3, L"LHXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 5:5 {3:4:5} 2:2 | {2.L12:B:4} {2.L12:7:8} 5:5 1:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
			// LW ZERO, <value32>(rs): LUI T0, <value32>.H20 + C.ADD T0, rs + LW ZERO, <value32>.L12(T0)
			ADD_IDER(RV32Inst13NeNT0, 3, L"LWXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 5:5 {3:4:5} 2:2 | {2.L12:B:4} {2.L12:7:8} 5:5 2:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
			// LBU ZERO, <value32>(rs): LUI T0, <value32>.H20 + C.ADD T0, rs + LBU ZERO, <value32>.L12(T0)
			ADD_IDER(RV32Inst13NeNT0, 3, L"LBUXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 5:5 {3:4:5} 2:2 | {2.L12:B:4} {2.L12:7:8} 5:5 4:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);
			// LHU ZERO, <value32>(rs): LUI T0, <value32>.H20 + C.ADD T0, rs + LHU ZERO, <value32>.L12(T0)
			ADD_IDER(RV32Inst13NeNT0, 3, L"LHUXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 5:5 {3:4:5} 2:2 | {2.L12:B:4} {2.L12:7:8} 5:5 5:3 0:5 3:7", RV32ArgType::AT_RV32_REG_Z, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_REG);

			// (for rd' == rs', <value32>.L12 == [0..127], <value32>.L12 is a multiple of 4) LW r, <value32>(r): LUI T0, <value32>.H20 + C.ADD r', T0 + C.LW r', <value32>.L12(r')
			ADD_IDER(RV32Inst13Eq2LU7M4, 3, L"LWXV,V(XV)", L"{2.H20:13:8} {2.H20:B:8} {2.H20:3:4} 5:5 37:7 | 4:3 1:1 {1:4:5} 5:5 2:2 | 2:3 {2.L12:5:3} {1:2:3} {2.L12:2:1} {2.L12:6:1} {1:2:3} 0:2", RV32ArgType::AT_RV32_COMP_REG, RV32ArgType::AT_RV32_4BYTE_VAL, RV32ArgType::AT_RV32_COMP_REG);
		}
	}
}


static std::map<std::wstring, int32_t> _registers;

#define ADD_REG(reg, index) _registers.emplace(std::make_pair(reg, index))

static void load_registers()
{
	ADD_REG(L"X0", 0);
	ADD_REG(L"X1", 1);
	ADD_REG(L"X2", 2);
	ADD_REG(L"X3", 3);
	ADD_REG(L"X4", 4);
	ADD_REG(L"X5", 5);
	ADD_REG(L"X6", 6);
	ADD_REG(L"X7", 7);
	ADD_REG(L"X8", 8);
	ADD_REG(L"X9", 9);
	ADD_REG(L"X10", 10);
	ADD_REG(L"X11", 11);
	ADD_REG(L"X12", 12);
	ADD_REG(L"X13", 13);
	ADD_REG(L"X14", 14);
	ADD_REG(L"X15", 15);
	ADD_REG(L"ZERO", 0);
	ADD_REG(L"RA", 1);
	ADD_REG(L"SP", 2);
	ADD_REG(L"GP", 3);
	ADD_REG(L"TP", 4);
	ADD_REG(L"T0", 5);
	ADD_REG(L"T1", 6);
	ADD_REG(L"T2", 7);
	ADD_REG(L"S0", 8);
	ADD_REG(L"FP", 8);
	ADD_REG(L"S1", 9);
	ADD_REG(L"A0", 10);
	ADD_REG(L"A1", 11);
	ADD_REG(L"A2", 12);
	ADD_REG(L"A3", 13);
	ADD_REG(L"A4", 14);
	ADD_REG(L"A5", 15);

	if(!_global_settings.GetEmbedded())
	{
		ADD_REG(L"X16", 16);
		ADD_REG(L"X17", 17);
		ADD_REG(L"X18", 18);
		ADD_REG(L"X19", 19);
		ADD_REG(L"X20", 20);
		ADD_REG(L"X21", 21);
		ADD_REG(L"X22", 22);
		ADD_REG(L"X23", 23);
		ADD_REG(L"X24", 24);
		ADD_REG(L"X25", 25);
		ADD_REG(L"X26", 26);
		ADD_REG(L"X27", 27);
		ADD_REG(L"X28", 28);
		ADD_REG(L"X29", 29);
		ADD_REG(L"X30", 30);
		ADD_REG(L"X31", 31);
		ADD_REG(L"A6", 16);
		ADD_REG(L"A7", 17);
		ADD_REG(L"S2", 18);
		ADD_REG(L"S3", 19);
		ADD_REG(L"S4", 20);
		ADD_REG(L"S5", 21);
		ADD_REG(L"S6", 22);
		ADD_REG(L"S7", 23);
		ADD_REG(L"S8", 24);
		ADD_REG(L"S9", 25);
		ADD_REG(L"S10", 26);
		ADD_REG(L"S11", 27);
		ADD_REG(L"T3", 28);
		ADD_REG(L"T4", 29);
		ADD_REG(L"T5", 30);
		ADD_REG(L"T6", 31);
	}
};


class CodeStmtRV32: public CodeStmt
{
protected:
	A1_T_ERROR GetExpressionSignature(Exp &exp, std::wstring &sign) const override
	{
		std::wstring reg_name;

		sign.clear();

		if(exp.GetSimpleValue(reg_name))
		{
			const auto reg = _registers.find(reg_name);
			if(reg != _registers.cend())
			{
				// a register found
				sign += L"XV";
				// clear expression
				exp.Clear();
				EVal v{ reg->second };
				exp.AddVal(v);

				return A1_T_ERROR::A1_RES_OK;
			}
		}

		// some value or expression
		sign += L"V";

		return A1_T_ERROR::A1_RES_OK;
	}

	A1_T_ERROR GetInstruction(const std::wstring &signature, const std::map<std::wstring, MemRef> &memrefs, int line_num, const std::string &file_name) override
	{
		decltype(_inst) last_valid = nullptr;
		decltype(_size) last_size = -1;
		decltype(_refs) last_refs;
		bool inst_found = false;

		while(true)
		{
			std::vector<const Inst *> insts;
			auto err = _global_settings.GetInstructions(signature, insts, line_num, file_name);
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}

			_inst = nullptr;
		
			bool valid = false;

			int32_t args[A1_MAX_INST_ARGS_NUM] = { 0 };

			for(auto i: insts)
			{
				bool rel_off = false;
				inst_found = true;

				_size = i->_size;
				_inst = i;

				for(auto a = 0; a < i->_argnum; a++)
				{
					int32_t val = -1;

					args[a] = 0;
					_refs[a].first = i->_argtypes[a];

					// PC-relative addresses cannot be resolved on this stage
					if(_refs[a].first.get().IsRelOffset())
					{
						continue;
					}

					if(_refs[a].first.get() == ArgType::AT_SPEC_TYPE)
					{
						auto err = i->GetSpecArg(a, _refs[a], val);
						if(err != A1_T_ERROR::A1_RES_OK)
						{
							return err;
						}
					}
					else
					{
						auto err = _refs[a].second.Eval(val, memrefs);
						if(err == A1_T_ERROR::A1_RES_OK)
						{
							if(!_refs[a].first.get().IsValidValue(val))
							{
								inst_found = false;
							}
						}
						else
						{
							inst_found = false;
						}
					}

					args[a] = val;
				}

				valid = _inst->CheckArgs(args[0], args[1], args[2]);

				if(inst_found)
				{
					inst_found = valid;
				}

				if(inst_found)
				{
					break;
				}

				// here valid = true if the instruction can be used in general (without PC-relative address or another symbols check)
				if(valid)
				{
					last_valid = _inst;
					last_size = _size;
					last_refs = _refs;
				}
			}

			if(!inst_found)
			{
				auto id = static_cast<const RV32Inst *>(_inst)->GetId();
				if(id < 0)
				{
					break;
				}
				_global_settings.AddInstToReplace(line_num, file_name, _inst);
				continue;
			}

			break;
		}

		if(!inst_found && last_valid != nullptr)
		{
			// use the most fit valid instruction (the largest one)
			_inst = last_valid;
			_size = last_size;
			_refs = last_refs;
		}
		else
		if(!inst_found)
		{
			if(_inst != nullptr)
			{
				_warnings.insert(A1_T_WARNING::A1_WRN_WINTOUTRANGE);
			}
			else
			{
				return A1_T_ERROR::A1_RES_EINVINST;
			}
		}

		return A1_T_ERROR::A1_RES_OK;
	}

	A1_T_ERROR GetRefValue(const std::pair<std::reference_wrapper<const ArgType>, Exp> &ref, const std::map<std::wstring, MemRef> &memrefs, uint32_t &value, int &size) override
	{
		int32_t addr = 0;
		bool rel_addr = false;

		auto err = ref.second.Eval(addr, memrefs);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		if(ref.first.get().IsRelOffset())
		{
			rel_addr = true;
			addr = addr - _address;
		}

		size = ref.first.get()._size;
		if(!ref.first.get().IsValidValue(addr))
		{
			if(ref.first.get() == RV32ArgType::AT_RV32_REG || ref.first.get() == RV32ArgType::AT_RV32_REG_NZ ||
				ref.first.get() == RV32ArgType::AT_RV32_COMP_REG || ref.first.get() == RV32ArgType::AT_RV32_REG_SP ||
				ref.first.get() == RV32ArgType::AT_RV32_REG_Z || ref.first.get() == RV32ArgType::AT_RV32_REG_NZ_NSP)
			{
				return static_cast<A1_T_ERROR>(B1_RES_EINVARG);
			}
			else
			{
				if(rel_addr)
				{
					return A1_T_ERROR::A1_RES_ERELOUTRANGE;
				}
				_warnings.insert(A1_T_WARNING::A1_WRN_WINTOUTRANGE);
			}
		}

		value = addr;

		return A1_T_ERROR::A1_RES_OK;
	}

public:
	CodeStmtRV32()
	: CodeStmt()
	{
	}
};

class CodeInitStmtRV32: public CodeStmtRV32
{
public:
	CodeInitStmtRV32()
	: CodeStmtRV32()
	{
	}
};

class RV32Sections : public Sections
{
protected:
	bool CheckSectionName(SectType stype, const std::wstring &type_mod) const override
	{
		if(type_mod.empty())
		{
			return (stype == SectType::ST_HEAP) || (stype == SectType::ST_STACK) || (stype == SectType::ST_DATA) || (stype == SectType::ST_INIT) || (stype == SectType::ST_CONST) || (stype == SectType::ST_CODE);
		}

		return false;
	}

	GenStmt *CreateNewStmt(SectType stype, const std::wstring &type_mod) const override
	{
		switch(stype)
		{
			case SectType::ST_DATA:
				return new DataStmt();

			case SectType::ST_HEAP:
				return new HeapStmt();

			case SectType::ST_STACK:
				return new StackStmt();

			case SectType::ST_CONST:
				return new ConstStmt();

			case SectType::ST_CODE:
				return new CodeStmtRV32();

			case SectType::ST_INIT:
				return new CodeInitStmtRV32();
		}

		return nullptr;
	}

	A1_T_ERROR AlignSectionBegin(Section *psec) override
	{
		if(!global_settings.GetAutoAlign())
		{
			return A1_T_ERROR::A1_RES_OK;
		}

		auto st = psec->GetType();
		if(st != SectType::ST_CONST)
		{
			return A1_T_ERROR::A1_RES_OK;
		}

		std::unique_ptr<GenStmt> stmt;
		A1_T_ERROR err;
		int32_t addr = 0;

		err = psec->GetAddress(addr);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		// align .CONST data starting address to 4 bytes
		if(addr % 4 != 0)
		{
			stmt.reset(new ConstStmt(1, 4 - addr % 4));
			stmt->SetAddress(addr);
			psec->push_back(stmt.release());
		}

		return A1_T_ERROR::A1_RES_OK;
	}

	A1_T_ERROR AlignSectionEnd(Section *psec) override
	{
		if(!global_settings.GetAutoAlign())
		{
			return A1_T_ERROR::A1_RES_OK;
		}

		auto st = psec->GetType();

		// since .DATA sections sizes are always multiples of 4 .HEAP section starts from 4-bytes aligned address 
		if(st == SectType::ST_NONE || st == SectType::ST_HEAP)
		{
			return A1_T_ERROR::A1_RES_OK;
		}

		std::unique_ptr<GenStmt> stmt;
		A1_T_ERROR err;
		int32_t size = 0;
		int32_t addr = 0;

		err = psec->GetSize(size);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		err = psec->GetAddress(addr);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		int32_t code_size_align = global_settings.GetCompressed() ? 2 : 4;

		switch(st)
		{
			// stack section address and size should be aligned to 16 bytes
			case SectType::ST_STACK:
				// it's enough to make stack section size to be a multiple of 16 for the section address to be aligned to 16-bytes
				// (because only one stack section is allowed and overall RAM size should be always a multiple of 16)
				if(size % 16 != 0)
				{
					stmt.reset(new StackStmt(1, 16 - size % 16));
					stmt->SetAddress(addr + size);
					psec->push_back(stmt.release());
				}
				break;
			// data section address and size should be aligned to 4 bytes
			case SectType::ST_DATA:
				// it's enough to make data section size to be a multiple of 4 for the section address to be aligned to 4-bytes
				if(size % 4 != 0)
				{
					stmt.reset(new DataStmt(1, 4 - size % 4));
					stmt->SetAddress(addr + size);
					psec->push_back(stmt.release());
				}
				break;
			// const section size should be aligned to 2 (or 4) bytes
			case SectType::ST_CONST:
				if(size % code_size_align != 0)
				{
					stmt.reset(new ConstStmt(1, code_size_align - size % code_size_align));
					stmt->SetAddress(addr + size);
					psec->push_back(stmt.release());
				}
				break;
			case SectType::ST_INIT:
			case SectType::ST_CODE:
				if(addr % code_size_align != 0 || size % code_size_align != 0)
				{
					return A1_T_ERROR::A1_RES_EWSECSIZE;
				}
				break;
			default:
				return A1_T_ERROR::A1_RES_EINTERR;
		}

		return A1_T_ERROR::A1_RES_OK;
	}


public:
	RV32Sections()
	: Sections()
	{
	}
};


int main(int argc, char **argv)
{
	bool print_err_desc = false;
	std::string ofn;
	bool print_version = false;
	std::string lib_dir;
	std::string MCU_name;
	// combination of I | E, M, C, ZMMUL
	std::string extensions = "IC";
	bool print_mem_use = false;
	std::vector<std::string> files;
	bool args_error = false;
	std::string args_error_txt;
	A1_T_ERROR err;


	// use current locale
	std::setlocale(LC_ALL, "");


	// read options and input file names
	for(int i = 1; i < argc; i++)
	{
		if(files.empty())
		{
			// allow sections auto-alignment
			if ((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'A' || argv[i][1] == 'a') &&
				argv[i][2] == 0)
			{
				global_settings.SetAutoAlign();
				continue;
			}

			// print error description
			if((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'D' || argv[i][1] == 'd') &&
				argv[i][2] == 0)
			{
				print_err_desc = true;
				continue;
			}

			// MCU extensions
			if ((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'E' || argv[i][1] == 'e') &&
				(argv[i][2] == 'X' || argv[i][2] == 'x') &&
				argv[i][3] == 0)
			{
				if(i == argc - 1)
				{
					args_error = true;
					args_error_txt = "missing MCU extensions";
				}
				else
				{
					i++;
					extensions = Utils::str_toupper(argv[i]);
				}

				continue;
			}

			// enable pseudo-instructions with different argument sizes and proper instruction selection algorithm
			if ((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'F' || argv[i][1] == 'f') &&
				argv[i][2] == 0)
			{
				_global_settings.SetFixAddresses();
				continue;
			}

			// libraries directory
			if ((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'L' || argv[i][1] == 'l') &&
				argv[i][2] == 0)
			{
				if(i == argc - 1)
				{
					args_error = true;
					args_error_txt = "missing libraries directory";
				}
				else
				{
					i++;
					lib_dir = argv[i];
				}

				continue;
			}

			// read MCU settings
			if ((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'M' || argv[i][1] == 'm') &&
				argv[i][2] == 0)
			{
				if(i == argc - 1)
				{
					args_error = true;
					args_error_txt = "missing MCU name";
				}
				else
				{
					i++;
					MCU_name = get_MCU_config_name(argv[i]);
				}

				continue;
			}

			// print memory usage
			if ((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'M' || argv[i][1] == 'm') &&
				(argv[i][2] == 'U' || argv[i][2] == 'u') &&
				argv[i][3] == 0)
			{
				print_mem_use = true;
				continue;
			}

			// forbid instructions converting to compressed representation
			if ((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'N' || argv[i][1] == 'n') &&
				(argv[i][2] == 'C' || argv[i][2] == 'c') &&
				(argv[i][3] == 'I' || argv[i][3] == 'i') &&
				argv[i][4] == 0)
			{
				global_settings.SetAutoCompInst(false);
				continue;
			}

			// specify output file name
			if ((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'O' || argv[i][1] == 'o') &&
				argv[i][2] == 0)
			{
				if(i == argc - 1)
				{
					args_error = true;
					args_error_txt = "missing output file name";
				}
				else
				{
					i++;
					ofn = argv[i];
				}

				continue;
			}

			// specify RAM size
			if((argv[i][0] == '-' || argv[i][0] == '/') && Utils::str_toupper(std::string(argv[i] + 1)) == "RAM_SIZE")
			{
				if(i == argc - 1)
				{
					args_error = true;
					args_error_txt = "missing RAM size";
				}
				else
				{
					i++;
					auto len = std::strlen(argv[i]);
					std::wstring s(argv[i], argv[i] + len);
					int32_t n = 0;
					auto err = Utils::str2int32(s, n);
					if(err != B1_RES_OK || n < 0)
					{
						args_error = true;
						args_error_txt = "wrong RAM size";
					}
					_global_settings.SetRAMSize(n);
				}

				continue;
			}

			// specify RAM starting address
			if((argv[i][0] == '-' || argv[i][0] == '/') && Utils::str_toupper(std::string(argv[i] + 1)) == "RAM_START")
			{
				if(i == argc - 1)
				{
					args_error = true;
					args_error_txt = "missing RAM starting address";
				}
				else
				{
					i++;
					auto len = std::strlen(argv[i]);
					std::wstring s(argv[i], argv[i] + len);
					int32_t n = 0;
					auto err = Utils::str2int32(s, n);
					if(err != B1_RES_OK || n < 0)
					{
						args_error = true;
						args_error_txt = "wrong RAM starting address";
					}
					_global_settings.SetRAMStart(n);
				}

				continue;
			}

			// specify ROM size
			if((argv[i][0] == '-' || argv[i][0] == '/') && Utils::str_toupper(std::string(argv[i] + 1)) == "ROM_SIZE")
			{
				if(i == argc - 1)
				{
					args_error = true;
					args_error_txt = "missing ROM size";
				}
				else
				{
					i++;
					auto len = std::strlen(argv[i]);
					std::wstring s(argv[i], argv[i] + len);
					int32_t n = 0;
					auto err = Utils::str2int32(s, n);
					if(err != B1_RES_OK || n < 0)
					{
						args_error = true;
						args_error_txt = "wrong ROM size";
					}
					_global_settings.SetROMSize(n);
				}

				continue;
			}

			// specify ROM starting address
			if((argv[i][0] == '-' || argv[i][0] == '/') && Utils::str_toupper(std::string(argv[i] + 1)) == "ROM_START")
			{
				if(i == argc - 1)
				{
					args_error = true;
					args_error_txt = "missing ROM starting address";
				}
				else
				{
					i++;
					auto len = std::strlen(argv[i]);
					std::wstring s(argv[i], argv[i] + len);
					int32_t n = 0;
					auto err = Utils::str2int32(s, n);
					if(err != B1_RES_OK || n < 0)
					{
						args_error = true;
						args_error_txt = "wrong ROM starting address";
					}
					_global_settings.SetROMStart(n);
				}

				continue;
			}

			// check target
			if ((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'T' || argv[i][1] == 't') &&
				argv[i][2] == 0)
			{
				if(i == argc - 1)
				{
					args_error = true;
					args_error_txt = "missing target";
				}
				else
				{
					i++;
					if(Utils::str_toupper(Utils::str_trim(argv[i])) != "RV32")
					{
						args_error = true;
						args_error_txt = "invalid target";
					}
				}

				continue;
			}

			// print version
			if((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'V' || argv[i][1] == 'v') &&
				argv[i][2] == 0)
			{
				print_version = true;
				continue;
			}
		}

		files.push_back(argv[i]);
	}


	_global_settings.SetTargetName("RV32");
	_global_settings.SetMCUName(MCU_name);
	_global_settings.SetLibDirRoot(lib_dir);

	// load target-specific stuff
	if(!select_target(global_settings))
	{
		args_error = true;
		args_error_txt = "invalid target";
	}


	if(args_error || files.empty() && !(print_version))
	{
		b1_print_version(stderr);

		if(args_error)
		{
			std::fputs("\nerror: ", stderr);
			std::fputs(args_error_txt.c_str(), stderr);
			std::fputs("\n", stderr);
		}
		else
		{
			std::fputs("\nerror: missing file name\n", stderr);
		}

		std::fputs("\nusage: ", stderr);
		std::fputs(B1_PROJECT_NAME, stderr);
		std::fputs(" [options] filename [filename1 filename2 ... filenameN]\n", stderr);
		std::fputs("options:\n", stderr);
		std::fputs("-d or /d - print error description\n", stderr);
		std::fputs("-ex or /ex - specify RISC-V MCU extensions (default IC), e.g.: -ex EC\n", stderr);
		std::fputs("-l or /l - libraries directory, e.g. -l \"../lib\"\n", stderr);
		std::fputs("-m or /m - specify MCU name, e.g. -m CH32V003F4\n", stderr);
		std::fputs("-mu or /mu - print memory usage\n", stderr);
		std::fputs("-o or /o - specify output file name, e.g.: -o out.ihx\n", stderr);
		std::fputs("-ram_size or /ram_size - specify RAM size, e.g.: -ram_size 0x800\n", stderr);
		std::fputs("-ram_start or /ram_start - specify RAM starting address, e.g.: -ram_start 0x20000000\n", stderr);
		std::fputs("-rom_size or /rom_size - specify ROM size, e.g.: -rom_size 0x4000\n", stderr);
		std::fputs("-rom_start or /rom_start - specify ROM starting address, e.g.: -rom_start 0x0\n", stderr);
		std::fputs("-t or /t - set target (default RV32), e.g.: -t RV32\n", stderr);
		std::fputs("-v or /v - show assembler version\n", stderr);
		return 1;
	}


	if(print_version)
	{
		// just print version and stop executing
		b1_print_version(stdout);
		return 0;
	}

	_global_settings.InitLibDirs();

	// read settings file if specified
	if(!MCU_name.empty())
	{
		auto file_name = _global_settings.GetLibFileName(MCU_name, ".cfg");
		if(!file_name.empty())
		{
			auto err = static_cast<A1_T_ERROR>(_global_settings.Read(file_name));
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				a1_print_error(err, -1, file_name, print_err_desc);
				return 2;
			}

			std::wstring ext;
			_global_settings.GetValue(L"EXTENSIONS", ext);
			if(!ext.empty())
			{
				extensions = Utils::str_toupper(Utils::wstr2str(ext));
			}
		}
		else
		{
			a1_print_warning(A1_T_WARNING::A1_WRN_WUNKNMCU, -1, MCU_name, _global_settings.GetPrintWarningDesc());
		}

		// initialize library directories a time more to take into account additional ones read from cfg file
		_global_settings.InitLibDirs();
	}


	_B1C_consts[L"__EXTENSIONS"].first = extensions;

	// parse extensions
	_global_settings.SetEmbedded(false);
	auto z = extensions.find('Z');
	auto e = extensions.rfind('I', z);
	if(e != std::string::npos)
	{
		extensions.erase(e, 1);
	}
	else
	{
		e = extensions.rfind('E', z);
		if(e != std::string::npos)
		{
			extensions.erase(e, 1);
			_global_settings.SetEmbedded();
		}
	}

	_global_settings.SetCompressed(false);
	z = extensions.find('Z');
	e = extensions.rfind('C', z);
	if(e != std::string::npos)
	{
		extensions.erase(e, 1);
		_global_settings.SetCompressed();
	}

	_global_settings.SetMultiplication(false);
	_global_settings.SetDivision(false);
	z = extensions.find('Z');
	e = extensions.rfind('M', z);
	if(e != std::string::npos)
	{
		extensions.erase(e, 1);
		_global_settings.SetMultiplication();
		_global_settings.SetDivision();
	}

	e = extensions.find("ZMMUL");
	if(e != std::string::npos)
	{
		extensions.erase(e, 5);
		_global_settings.SetMultiplication();
	}

	extensions = Utils::str_ltrim(extensions, "_");

	if(!extensions.empty())
	{
		a1_print_warning(A1_T_WARNING::A1_WRN_WUNKMCUEX, -1, "", _global_settings.GetPrintWarningDesc());
	}


	// prepare output file name
	if(ofn.empty())
	{
		// no output file, use input file's directory and name but with ihx extension
		ofn = files.front();
		auto delpos = ofn.find_last_of("\\/");
		auto pntpos = ofn.find_last_of('.');
		if(pntpos != std::string::npos && (delpos == std::string::npos || pntpos > delpos))
		{
			ofn.erase(pntpos, std::string::npos);
		}
		ofn += ".ihx";
	}
	else
	if(ofn.back() == '\\' || ofn.back() == '/')
	{
		// output directory only, use input file name but with ihx extension
		std::string tmp = files.front();
		auto delpos = tmp.find_last_of("\\/");
		if(delpos != std::string::npos)
		{
			tmp.erase(0, delpos + 1);
		}
		auto pntpos = tmp.find_last_of('.');
		if(pntpos != std::string::npos)
		{
			tmp.erase(pntpos, std::string::npos);
		}
		tmp += ".ihx";
		ofn += tmp;
	}


	// initialize registers map
	load_registers();

	// initialize instructions map
	load_RV32_instructions();


	_B1C_consts[L"__TARGET_NAME"].first = "RV32";
	_B1C_consts[L"__MCU_NAME"].first = MCU_name;


	RV32Sections secs;

	err = secs.ReadSourceFiles(files);
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		if(_global_settings.GetPrintWarnings())
		{
			auto &ws = secs.GetWarnings();
			for(auto &w: ws)
			{
				a1_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
			}
		}

		a1_print_error(err, secs.GetCurrLineNum(), secs.GetCurrFileName(), print_err_desc, secs.GetCustomErrorMsg());
		return 3;
	}

	while(true)
	{
		err = secs.ReadSections();
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			if(_global_settings.GetPrintWarnings())
			{
				auto &ws = secs.GetWarnings();
				for(auto &w: ws)
				{
					a1_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
				}
			}

			a1_print_error(err, secs.GetCurrLineNum(), secs.GetCurrFileName(), print_err_desc, secs.GetCustomErrorMsg());
			return 4;
		}

		err = secs.Write(ofn);
		if(err == A1_T_ERROR::A1_RES_ERELOUTRANGE && _global_settings.GetFixAddresses())
		{
			continue;
		}
		else
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			if(_global_settings.GetPrintWarnings())
			{
				auto &ws = secs.GetWarnings();
				for(auto &w: ws)
				{
					a1_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
				}
			}

			a1_print_error(err, secs.GetCurrLineNum(), secs.GetCurrFileName(), print_err_desc, secs.GetCustomErrorMsg());
			return 5;
		}

		break;
	}

	if(_global_settings.GetPrintWarnings())
	{
		auto &ws = secs.GetWarnings();
		for(auto &w: ws)
		{
			a1_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
		}
	}

	if(print_mem_use)
	{
		std::fwprintf(stdout, L"Memory usage:\n");
		std::fwprintf(stdout, L"Variables: %d (%ls kB)\n", (int)secs.GetVariablesSize(), get_size_kB(secs.GetVariablesSize()).c_str());
		std::fwprintf(stdout, L"Heap: %d (%ls kB)\n", (int)secs.GetHeapSize(), get_size_kB(secs.GetHeapSize()).c_str());
		std::fwprintf(stdout, L"Stack: %d (%ls kB)\n", (int)secs.GetStackSize(), get_size_kB(secs.GetStackSize()).c_str());
		std::fwprintf(stdout, L"Total RAM: %d (%ls kB)\n", (int)secs.GetVariablesSize() + secs.GetHeapSize() + secs.GetStackSize(), get_size_kB(secs.GetVariablesSize() + secs.GetHeapSize() + secs.GetStackSize()).c_str());
		std::fwprintf(stdout, L"Constants: %d (%ls kB)\n", (int)secs.GetConstSize(), get_size_kB(secs.GetConstSize()).c_str());
		std::fwprintf(stdout, L"Code: %d (%ls kB)\n", (int)secs.GetCodeSize(), get_size_kB(secs.GetCodeSize()).c_str());
		std::fwprintf(stdout, L"Total ROM: %d (%ls kB)\n", (int)(secs.GetConstSize() + secs.GetCodeSize()), get_size_kB(secs.GetConstSize() + secs.GetCodeSize()).c_str());
	}

	return 0;
}
