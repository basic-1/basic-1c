/*
 STM8 intermediate code compiler
 Copyright (c) 2021-2026 Nikolay Pletnev
 MIT license

 c1stm8.cpp: STM8 intermediate code compiler
*/


#include <clocale>
#include <cstring>
#include <algorithm>

#include "../../common/source/trgsel.h"
#include "../../common/source/version.h"
#include "../../common/source/gitrev.h"

#include "c1stm8.h"


static const char *version = B1_CMP_VERSION;


STM8Settings global_settings;
Settings &_global_settings = global_settings;


bool B1_ASM_OP_STM8::ParseNumeric(const std::wstring &num_str, int32_t &n) const
{
	std::wstring postfix;
	auto b = num_str.cbegin();
	auto e = num_str.cend();

	auto pos = num_str.find_last_of(L'.');
	if(pos != std::wstring::npos)
	{
		postfix = Utils::str_toupper(num_str.substr(pos + 1));
		if(postfix == L"H" || postfix == L"L" || postfix == L"HH" || postfix == L"HL" || postfix == L"LH" || postfix == L"LL")
		{
			e = num_str.cbegin() + pos;
		}
		else
		{
			postfix.clear();
		}
	}

	if(Utils::str2int32(std::wstring(b, e), n) == B1_RES_OK && (postfix.empty() || _global_settings.ProcessNumPostfix(postfix, n) == B1_RES_OK))
	{
		return true;
	}

	return false;
}

bool B1_ASM_OP_STM8::Parse() const
{
	const static std::vector<wchar_t> dels({ L',', L'(', L')', L'[', L']' });
	const static std::vector<std::wstring> regs({ L"A", L"X", L"Y", L"SP", L"CC" });

	if(!_parsed)
	{
		auto data = Utils::str_trim(_data);

		if(_type == AOT::AOT_LABEL)
		{
			_op = data;
			_parsed = true;
		}
		else
		if(_type == AOT::AOT_DATA)
		{
			// no need of parsing data defintion at the moment
		}
		else
		if(_type == AOT::AOT_OP)
		{
			std::wstring op;
			std::vector<std::wstring> args;

			auto pos = data.find_first_of(L" ;\t");
			if(pos == std::wstring::npos)
			{
				op = data;
			}
			else
			{
				op = data.substr(0, pos);

				data = data.substr(pos + 1);
				pos = data.find(L';');
				if(pos != std::wstring::npos)
				{
					data = data.substr(0, pos);
				}
				data = Utils::str_trim(data);

				std::vector<std::wstring> argparts;
				Utils::str_split(data, dels, argparts, true);
				std::wstring arg;
				for(const auto &ap: argparts)
				{
					auto s = Utils::str_trim(ap);

					if(s == L",")
					{
						if(arg.length() > 0 && arg.front() == L'(' && arg.back() != L')')
						{
							arg += s;
							continue;
						}
						args.push_back(arg);
						arg.clear();
					}
					else
					{
						if(	(s.length() == 1 && std::find(dels.cbegin(), dels.cend(), s.front()) != dels.cend()) ||
							std::find(regs.cbegin(), regs.cend(), s) != regs.cend())
						{
							arg += Utils::str_trim(s);
						}
						else
						{
							// check for postfix presence
							int32_t n = 0;
							if(ParseNumeric(s, n))
							{
								arg += Utils::str_tohex32(n);
							}
							else
							{
								std::vector<std::wstring> adds;
								if(Utils::str_split(s, L"+", adds) > 1)
								{
									bool allcvt = true;
									int32_t sum = 0;
									std::vector<std::pair<bool, int32_t>> numvalues;
									for(const auto &a: adds)
									{
										if(ParseNumeric(Utils::str_trim(a), n))
										{
											numvalues.emplace_back(std::make_pair(true, n));
										}
										else
										{
											allcvt = false;
											numvalues.emplace_back(std::make_pair(false, -1));
										}

										sum += n;
									}

									if(allcvt)
									{
										s = Utils::str_tohex32(sum);
									}
									else
									{
										s.clear();
										for(auto i = 0; i < numvalues.size(); i++)
										{
											if(numvalues[i].first)
											{
												s += Utils::str_tohex32(numvalues[i].second);
											}
											else
											{
												s += adds[i];
											}

											if(i != numvalues.size() - 1)
											{
												s += L"+";
											}
										}
										s = Utils::str_delspaces(s);
									}
								}
								else
								{
									s = Utils::str_delspaces(s);
								}

								arg += s;
							}
						}
					}
				}
				if(!arg.empty())
				{
					args.push_back(arg);
				}
			}

			_op = op;
			_args = args;
			_parsed = true;
		}
	}

	return _parsed;
}


C1_T_ERROR C1STM8Compiler::process_asm_cmd(const std::wstring &line)
{
	if(!line.empty())
	{
		size_t offset = 0, prev_off, len;
		std::wstring cmd;
		int lbl_off = -1;
		bool is_call = false;

		// get opcode
		cmd = get_next_value(line, L" \t\r\n", offset);

		if(cmd == L"BTJF" || cmd == L"BTJT")
		{
			lbl_off = 2;
		}
		else
		if(cmd == L"CALL" || cmd == L"CALLF" || cmd == L"CALLR")
		{
			lbl_off = 0;
			is_call = true;
		}
		else
		if(cmd == L"INT")
		{
			lbl_off = 0;
		}
		else
		if(cmd == L"JP" || cmd == L"JPF")
		{
			lbl_off = 0;
		}
		else
		if(	cmd == L"JRA" || cmd == L"JRT" || cmd == L"JRC" || cmd == L"JRULT" || cmd == L"JREQ" || cmd == L"JRF" || cmd == L"JRH" || cmd == L"JRIH" || cmd == L"JRIL" ||
			cmd == L"JRM" || cmd == L"JRMI" || cmd == L"JRNC" || cmd == L"JRUGE" || cmd == L"JRNE" || cmd == L"JRNH" || cmd == L"JRNM" || cmd == L"JRNV" || cmd == L"JRPL" ||
			cmd == L"JRSGE" || cmd == L"JRSGT" || cmd == L"JRSLE" || cmd == L"JRSGT" || cmd == L"JRUGT" || cmd == L"JRULE" || cmd == L"JRV")
		{
			lbl_off = 0;
		}
		
		if(lbl_off >= 0)
		{
			for(int i = 0; i <= lbl_off; i++)
			{
				if(offset == line.npos || line[offset] == L';')
				{
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}

				prev_off = offset;
				cmd = get_next_value(line, L",;", offset);
			}

			len = cmd.length();
			cmd = Utils::str_trim(cmd);

			if(cmd.empty())
			{
				_asm_stmt_it->args.push_back(B1_CMP_ARG(line));
			}
			else
			{
				bool brackets = false;
				bool sqr_brackets = false;

				if(cmd.front() == L'(' && cmd.back() == L')')
				{
					cmd.pop_back();
					cmd.erase(cmd.begin());
					cmd = Utils::str_trim(cmd);
					brackets = true;
				}
				else
				if(cmd.front() == L'[' && cmd.back() == L']')
				{
					cmd.pop_back();
					cmd.erase(cmd.begin());
					cmd = Utils::str_trim(cmd);
					sqr_brackets = true;
				}

				if(!check_label_name(cmd))
				{
					return C1_T_ERROR::C1_RES_EINVLBNAME;
				}
				cmd = add_namespace(cmd);

				_req_symbols.insert(cmd);
				if(is_call && !sqr_brackets)
				{
					_sub_entry_labels.insert(cmd);
				}

				if(brackets)
				{
					cmd = L"(" + cmd + L")";
				}
				else
				if(sqr_brackets)
				{
					cmd = L"[" + cmd + L"]";
				}

				if(lbl_off > 0)
				{
					cmd = L" " + cmd;
				}

				_asm_stmt_it->args.push_back(B1_CMP_ARG(std::wstring(line.begin(), line.begin() + prev_off) + cmd + L" " + line.substr(prev_off + len)));
			}
		}
		else
		{
			_asm_stmt_it->args.push_back(B1_CMP_ARG(line));
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

B1_ASM_OPS::iterator C1STM8Compiler::create_asm_op(B1_ASM_OPS &sec, B1_ASM_OPS::const_iterator where, AOT type, const std::wstring &lbl, bool is_volatile, bool is_inline)
{
	return sec.emplace(where, new B1_ASM_OP_STM8(type, lbl, _comment, is_volatile, is_inline));
}

C1_T_ERROR C1STM8Compiler::stm8_calc_array_size(const B1_CMP_VAR &var, int32_t size1)
{
	if(var.fixed_size)
	{
		int32_t arr_size = 1;

		for(int32_t i = 0; i < var.dim_num; i++)
		{
			arr_size *= (var.dims[i * 2 + 1] - var.dims[i * 2] + 1);
		}

		arr_size *= size1;

		add_op(*_curr_code_sec, L"LDW X, " + Utils::str_tohex16(arr_size), false); //AE WORD_VALUE
	}
	else
	{
		add_op(*_curr_code_sec, L"LDW X, (" + var.name + L" + 0x4)", false); //BE SHORT_ADDRESS, CE LONG_ADDRESS

		for(int32_t i = 1; i < var.dim_num; i++)
		{
			add_op(*_curr_code_sec, L"PUSHW X", false); //89
			_stack_ptr += 2;
			add_op(*_curr_code_sec, L"LDW X, (" + var.name + L" + " + Utils::str_tohex16(4 * i + 4) + L")", false); //BE SHORT_ADDRESS, CE LONG_ADDRESS
			add_call_op(L"__LIB_COM_MUL16");
			add_op(*_curr_code_sec, L"ADDW SP, 2", false); //5B BYTE_VALUE
			_stack_ptr -= 2;
		}

		if(size1 == 2)
		{
			add_op(*_curr_code_sec, L"SLAW X", false); //58
		}
		else
		if(size1 == 4)
		{
			add_op(*_curr_code_sec, L"SLAW X", false); //58
			add_op(*_curr_code_sec, L"SLAW X", false); //58
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::stm8_st_gf(const B1_CMP_VAR &var, bool is_ma)
{
	int32_t size1;

	if(!B1CUtils::get_asm_type(var.type, nullptr, &size1))
	{
		return C1_T_ERROR::C1_RES_EINVTYPNAME;
	}

	if(!is_ma)
	{
		_req_symbols.insert(var.name);
	}

	const std::wstring v = is_ma ? (var.use_symbol ? var.symbol : std::to_wstring(var.address)) : var.name;

	if(var.dim_num == 0)
	{
		// simple variable
		if(size1 == 1)
		{
			// BYTE type
			add_op(*_curr_code_sec, L"MOV (" + v + L"), 0", var.is_volatile); //35 00 LONG_ADDRESS
		}
		else
		{
			if(var.type == B1Types::B1T_STRING)
			{
				// release string
				add_op(*_curr_code_sec, L"LDW X, (" + v + L")", false); //BE SHORT_ADDRESS, CE LONG_ADDRESS
				add_call_op(L"__LIB_STR_RLS");
			}
			add_op(*_curr_code_sec, L"CLRW X", false); //5F
			add_op(*_curr_code_sec, L"LDW (" + v + L"), X", var.is_volatile); //BF SHORT_ADDRESS CF LONG_ADDRESS
			if(var.type == B1Types::B1T_LONG)
			{
				add_op(*_curr_code_sec, L"LDW (" + v + L" + 2), X", var.is_volatile); //BF SHORT_ADDRESS CF LONG_ADDRESS
			}
		}
	}
	else
	{
		// array
		// check if the array is allocated
		const auto label = emit_label(true);
		if(!is_ma)
		{
			add_op(*_curr_code_sec, L"LDW X, (" + v + L")", var.is_volatile); //BE SHORT_ADDRESS, CE LONG_ADDRESS
			add_op(*_curr_code_sec, L"JREQ " + label, var.is_volatile); //27 SIGNED_BYTE_OFFSET
			_req_symbols.insert(label);
		}

		if(is_ma || var.type == B1Types::B1T_STRING)
		{
			auto err = stm8_calc_array_size(var, size1);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			add_op(*_curr_code_sec, L"PUSHW X", false); //89
			_stack_ptr += 2;
		}

		if(var.type == B1Types::B1T_STRING)
		{
			if(is_ma)
			{
				add_op(*_curr_code_sec, L"LDW X, " + v, false); //AE WORD_VALUE
			}
			else
			{
				add_op(*_curr_code_sec, L"LDW X, (" + v + L")", false); //BE SHORT_ADDRESS, CE LONG_ADDRESS
			}
			add_call_op(L"__LIB_STR_ARR_DAT_RLS");
		}

		if(is_ma)
		{
			add_op(*_curr_code_sec, L"LDW X, " + v, false); //AE WORD_VALUE
			add_op(*_curr_code_sec, L"PUSH 0", false); //4B BYTE_VALUE
			_stack_ptr += 1;
			add_call_op(L"__LIB_MEM_SET");
			add_op(*_curr_code_sec, L"ADDW SP, 3", false); //5B BYTE_VALUE
			_stack_ptr -= 3;
		}
		else
		{
			add_op(*_curr_code_sec, L"LDW X, (" + v + L")", var.is_volatile); //BE SHORT_ADDRESS, CE LONG_ADDRESS
			add_call_op(L"__LIB_MEM_FRE");
			add_op(*_curr_code_sec, L"CLRW X", false); //5F
			add_op(*_curr_code_sec, L"LDW (" + v + L"), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS
			if(var.type == B1Types::B1T_STRING)
			{
				add_op(*_curr_code_sec, L"POPW X", false); //85
				_stack_ptr -= 2;
			}
		}

		if(!is_ma)
		{
			add_lbl(*_curr_code_sec, _curr_code_sec->cend(), label, false);
			_all_symbols.insert(label);
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::stm8_arrange_types(const B1Types type_from, const B1Types type_to)
{
	if(type_from != type_to)
	{
		if(type_from == B1Types::B1T_BYTE)
		{
			if(type_to == B1Types::B1T_LONG)
			{
				//A -> Y:X
				add_op(*_curr_code_sec, L"CLRW Y", false); //90 5F
			}
			// A -> X
			add_op(*_curr_code_sec, L"CLRW X", false); //5F
			add_op(*_curr_code_sec, L"LD XL, A", false); //97

			if(type_to == B1Types::B1T_STRING)
			{
				// BYTE to STRING
				add_call_op(L"__LIB_STR_STR_I");
			}
		}
		else
		if(type_from == B1Types::B1T_INT || type_from == B1Types::B1T_WORD)
		{
			if(type_to == B1Types::B1T_BYTE)
			{
				// X -> A
				add_op(*_curr_code_sec, L"LD A, XL", false); //9F
			}
			else
			if(type_to == B1Types::B1T_LONG)
			{
				// X -> Y:X
				add_op(*_curr_code_sec, L"CLRW Y", false); //90 5F
				if(type_from == B1Types::B1T_INT)
				{
					add_op(*_curr_code_sec, L"TNZW X", false); //5D
					const auto label = emit_label(true);
					add_op(*_curr_code_sec, L"JRPL " + label, false); //2A SIGNED_BYTE_OFFSET
					_req_symbols.insert(label);
					add_op(*_curr_code_sec, L"DECW Y", false); //90 5A
					add_lbl(*_curr_code_sec, _curr_code_sec->cend(), label, false);
					_all_symbols.insert(label);
				}
			}
			else
			if(type_to == B1Types::B1T_STRING)
			{
				if(type_from == B1Types::B1T_INT)
				{
					add_call_op(L"__LIB_STR_STR_I");
				}
				else
				{
					add_call_op(L"__LIB_STR_STR_W");
				}
			}
		}
		else
		if(type_from == B1Types::B1T_LONG)
		{
			if(type_to == B1Types::B1T_BYTE)
			{
				// Y:X -> A
				add_op(*_curr_code_sec, L"LD A, XL", false); //9F
			}
			else
			if(type_to == B1Types::B1T_STRING)
			{
				add_call_op(L"__LIB_STR_STR_L");
			}
		}
		else
		{
			// string, can't convert to any other type
			return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

int32_t C1STM8Compiler::stm8_get_local_offset(const std::wstring &local_name)
{
	int32_t offset = -1;

	for(const auto &loc: _local_offset)
	{
		if(loc.first.value == local_name)
		{
			offset = _stack_ptr - loc.second;
		}
	}

	return offset;
}

int32_t C1STM8Compiler::stm8_get_type_cvt_offset(B1Types type_from, B1Types type_to)
{
	int32_t offset = 0;

	if(type_from != B1Types::B1T_BYTE && type_from != B1Types::B1T_INT && type_from != B1Types::B1T_WORD && type_from != B1Types::B1T_LONG && type_from != B1Types::B1T_STRING)
	{
		return -1;
	}
	if(type_to != B1Types::B1T_BYTE && type_to != B1Types::B1T_INT && type_to != B1Types::B1T_WORD && type_to != B1Types::B1T_LONG && type_to != B1Types::B1T_STRING)
	{
		return -1;
	}

	if(type_from == type_to)
	{
		return 0;
	}

	if(type_from == B1Types::B1T_STRING || type_to == B1Types::B1T_STRING)
	{
		return -1;
	}

	if(type_from == B1Types::B1T_LONG)
	{
		switch(type_to)
		{
			case B1Types::B1T_BYTE:
				return 3;
			case B1Types::B1T_INT:
			case B1Types::B1T_WORD:
				return 2;
		}
	}
	else
	if(type_from == B1Types::B1T_INT || type_from == B1Types::B1T_WORD)
	{
		if(type_to == B1Types::B1T_BYTE)
		{
			return 1;
		}
	}

	return -1;
}

C1_T_ERROR C1STM8Compiler::stm8_load_from_stack(int32_t offset, const B1Types init_type, const B1Types req_type, LVT req_valtype, LVT &rvt, std::wstring &rv, B1_ASM_OPS::const_iterator *str_last_use_it /*= nullptr*/)
{
	if(offset < 0 || offset > 255)
	{
		return C1_T_ERROR::C1_RES_ESTCKOVF;
	}

	if(init_type == B1Types::B1T_BYTE)
	{
		if((req_valtype & LVT::LVT_STKREF) && req_type == B1Types::B1T_BYTE)
		{
			rvt = LVT::LVT_STKREF;
			rv = Utils::str_tohex16(offset);
		}
		else
		if(req_valtype & LVT::LVT_REG)
		{
			rvt = LVT::LVT_REG;

			add_op(*_curr_code_sec, L"LD A, (" + Utils::str_tohex16(offset) + L", SP)", false); //7B BYTE_OFFSET

			if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD || req_type == B1Types::B1T_STRING)
			{
				add_op(*_curr_code_sec, L"CLRW X", false); //5F
				add_op(*_curr_code_sec, L"LD XL, A", false); //97
				if(req_type == B1Types::B1T_STRING)
				{
					add_call_op(L"__LIB_STR_STR_I");
				}
			}
			else
			if(req_type == B1Types::B1T_LONG)
			{
				add_op(*_curr_code_sec, L"CLRW Y", false); //90 5F
				add_op(*_curr_code_sec, L"CLRW X", false); //5F
				add_op(*_curr_code_sec, L"LD XL, A", false); //97
			}
		}
		else
		{
			return C1_T_ERROR::C1_RES_EINTERR;
		}
	}
	else
	if(init_type == B1Types::B1T_INT || init_type == B1Types::B1T_WORD)
	{
		offset += (req_type == B1Types::B1T_BYTE) ? 1 : 0;
		if(offset > 255)
		{
			return C1_T_ERROR::C1_RES_ESTCKOVF;
		}

		if((req_valtype & LVT::LVT_STKREF) && req_type != B1Types::B1T_STRING && req_type != B1Types::B1T_LONG)
		{
			rvt = LVT::LVT_STKREF;
			rv = Utils::str_tohex16(offset);
		}
		else
		if(req_valtype & LVT::LVT_REG)
		{
			rvt = LVT::LVT_REG;

			if(req_type == B1Types::B1T_BYTE)
			{
				add_op(*_curr_code_sec, L"LD A, (" + Utils::str_tohex16(offset) + L", SP)", false); //7B BYTE_OFFSET
			}
			else
			{
				if(req_type == B1Types::B1T_LONG)
				{
					add_op(*_curr_code_sec, L"CLRW Y", false); //90 5F
				}

				add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)", false); //1E BYTE_OFFSET

				if(req_type == B1Types::B1T_STRING)
				{
					if(init_type == B1Types::B1T_INT)
					{
						add_call_op(L"__LIB_STR_STR_I");
					}
					else
					{
						add_call_op(L"__LIB_STR_STR_W");
					}
				}
				else
				if(req_type == B1Types::B1T_LONG)
				{
					if(init_type == B1Types::B1T_INT)
					{
						const auto label = emit_label(true);
						add_op(*_curr_code_sec, L"JRPL " + label, false); //2A SIGNED_BYTE_OFFSET
						_req_symbols.insert(label);
						add_op(*_curr_code_sec, L"DECW Y", false); //90 5A
						add_lbl(*_curr_code_sec, _curr_code_sec->cend(), label, false);
						_all_symbols.insert(label);
					}
				}
			}
		}
		else
		{
			return C1_T_ERROR::C1_RES_EINTERR;
		}
	}
	else
	if(init_type == B1Types::B1T_LONG)
	{
		offset += (req_type == B1Types::B1T_BYTE) ? 3 : ((req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD) ? 2 : 0);
		if(offset > (req_type == B1Types::B1T_LONG ? 253 : 255))
		{
			return C1_T_ERROR::C1_RES_ESTCKOVF;
		}

		if((req_valtype & LVT::LVT_STKREF) && req_type != B1Types::B1T_STRING)
		{
			rvt = LVT::LVT_STKREF;
			rv = Utils::str_tohex16(offset);
		}
		else
		if(req_valtype & LVT::LVT_REG)
		{
			rvt = LVT::LVT_REG;

			if(req_type == B1Types::B1T_BYTE)
			{
				add_op(*_curr_code_sec, L"LD A, (" + Utils::str_tohex16(offset) + L", SP)", false); //7B BYTE_OFFSET
			}
			else
			if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
			{
				add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)", false); //1E BYTE_OFFSET
			}
			else
			if(req_type == B1Types::B1T_STRING)
			{
				add_op(*_curr_code_sec, L"LDW Y, (" + Utils::str_tohex16(offset) + L", SP)", false); //16 BYTE_OFFSET
				add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset + 2) + L", SP)", false); //1E BYTE_OFFSET
				add_call_op(L"__LIB_STR_STR_L");
			}
			else
			{
				add_op(*_curr_code_sec, L"LDW Y, (" + Utils::str_tohex16(offset) + L", SP)", false); //16 BYTE_OFFSET
				add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset + 2) + L", SP)", false); //1E BYTE_OFFSET
			}
		}
		else
		{
			return C1_T_ERROR::C1_RES_EINTERR;
		}
	}
	else
	{
		// string
		if(req_type != B1Types::B1T_STRING)
		{
			return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
		}

		if(req_valtype & LVT::LVT_REG)
		{
			rvt = LVT::LVT_REG;
			// STRING variable, copy value
			add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)", false); //1E BYTE_OFFSET
			auto it = add_call_op(L"__LIB_STR_CPY");
			if(str_last_use_it != nullptr)
			{
				*str_last_use_it = it;
			}
		}
		else
		{
			return C1_T_ERROR::C1_RES_EINTERR;
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

std::wstring C1STM8Compiler::stm8_get_var_addr(const std::wstring &var_name, B1Types type_from, B1Types type_to, bool direct_cvt, bool *volatile_var /*= nullptr*/)
{
	// simple variable
	auto ma = _mem_areas.find(var_name);
	bool is_ma = ma != _mem_areas.end();
	std::wstring str_off;
	int int_off = 0;
	bool is_volatile = false;

	if(type_from != B1Types::B1T_BYTE && type_from != B1Types::B1T_INT && type_from != B1Types::B1T_WORD && type_from != B1Types::B1T_LONG && type_from != B1Types::B1T_STRING)
	{
		return std::wstring();
	}
	if(type_to != B1Types::B1T_BYTE && type_to != B1Types::B1T_INT && type_to != B1Types::B1T_WORD && type_to != B1Types::B1T_LONG && type_to != B1Types::B1T_STRING)
	{
		return std::wstring();
	}

	if(type_from == B1Types::B1T_LONG)
	{
		if(type_to == B1Types::B1T_INT || type_to == B1Types::B1T_WORD)
		{
			str_off = L" + 0x2";
			int_off = 2;
		}
		else
		if(type_to == B1Types::B1T_BYTE)
		{
			str_off = L" + 0x3";
			int_off = 3;
		}
		else
		if(direct_cvt && type_to != B1Types::B1T_LONG)
		{
			return std::wstring();
		}
	}
	else
	if(type_from == B1Types::B1T_INT || type_from == B1Types::B1T_WORD)
	{
		if(type_to == B1Types::B1T_BYTE)
		{
			str_off = L" + 0x1";
			int_off = 1;
		}
		else
		if(direct_cvt && !(type_to == B1Types::B1T_INT || type_to == B1Types::B1T_WORD))
		{
			return std::wstring();
		}
	}
	else
	if(direct_cvt && !(type_from == B1Types::B1T_BYTE && type_to == B1Types::B1T_BYTE) && !(type_from == B1Types::B1T_STRING && type_to == B1Types::B1T_STRING))
	{
		return std::wstring();
	}

	auto addr = is_ma ?
		(ma->second.use_symbol ? (ma->second.symbol + str_off) : std::to_wstring(ma->second.address + int_off)) :
		(var_name + str_off);

	if(is_ma)
	{
		is_volatile = ma->second.is_volatile;
	}
	else
	{
		is_volatile = _vars.find(var_name)->second.is_volatile;
		_req_symbols.insert(var_name);
	}

	if(volatile_var != nullptr)
	{
		*volatile_var = is_volatile;
	}

	return addr;
}

C1_T_ERROR C1STM8Compiler::stm8_load(const B1_TYPED_VALUE &tv, const B1Types req_type, LVT req_valtype, LVT *res_valtype /*= nullptr*/, std::wstring *res_val /*= nullptr*/, bool *volatile_var /*= nullptr*/)
{
	std::wstring rv;
	LVT rvt;
	auto init_type = tv.type;

	if(volatile_var != nullptr)
	{
		*volatile_var = false;
	}

	if(B1CUtils::is_imm_val(tv.value) || Utils::check_const_name(tv.value))
	{
		// imm. value
		if(init_type == B1Types::B1T_BYTE)
		{
			if((req_valtype & LVT::LVT_IMMVAL) && req_type != B1Types::B1T_STRING)
			{
				rvt = LVT::LVT_IMMVAL;
				rv = tv.value;
			}
			else
			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				if(req_type == B1Types::B1T_BYTE)
				{
					add_op(*_curr_code_sec, L"LD A, " + tv.value, false); //A6 BYTE_VALUE
				}
				else
				if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
				{
					add_op(*_curr_code_sec, L"LDW X, " + tv.value, false); //AE WORD_VALUE
				}
				else
				if(req_type == B1Types::B1T_LONG)
				{
					add_op(*_curr_code_sec, L"LDW Y, " + tv.value + L".h", false); //90 AE WORD_VALUE
					add_op(*_curr_code_sec, L"LDW X, " + tv.value + L".l", false); //AE WORD_VALUE
				}
				else
				{
					add_op(*_curr_code_sec, L"LDW X, " + tv.value, false); //AE WORD_VALUE
					add_call_op(L"__LIB_STR_STR_I");
				}
			}
			else
			{
				return C1_T_ERROR::C1_RES_EINTERR;
			}
		}
		else
		if(init_type == B1Types::B1T_INT || init_type == B1Types::B1T_WORD)
		{
			if((req_valtype & LVT::LVT_IMMVAL) && req_type != B1Types::B1T_STRING)
			{
				rvt = LVT::LVT_IMMVAL;
				rv = tv.value;
			}
			else
			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				if(req_type == B1Types::B1T_BYTE)
				{
					add_op(*_curr_code_sec, L"LD A, " + tv.value, false); //A6 BYTE_VALUE
				}
				else
				if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
				{
					add_op(*_curr_code_sec, L"LDW X, " + tv.value, false); //AE WORD_VALUE
				}
				else
				if(req_type == B1Types::B1T_LONG)
				{
					add_op(*_curr_code_sec, L"LDW Y, " + tv.value + L".h", false); //90 AE WORD_VALUE
					add_op(*_curr_code_sec, L"LDW X, " + tv.value + L".l", false); //AE WORD_VALUE
				}
				else
				{
					add_op(*_curr_code_sec, L"LDW X, " + tv.value, false); //AE WORD_VALUE
					if(init_type == B1Types::B1T_INT)
					{
						add_call_op(L"__LIB_STR_STR_I");
					}
					else
					{
						add_call_op(L"__LIB_STR_STR_W");
					}
				}
			}
			else
			{
				return C1_T_ERROR::C1_RES_EINTERR;
			}
		}
		else
		if(init_type == B1Types::B1T_LONG)
		{
			if((req_valtype & LVT::LVT_IMMVAL) && req_type != B1Types::B1T_STRING)
			{
				rvt = LVT::LVT_IMMVAL;
				rv = tv.value;
			}
			else
			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				if(req_type == B1Types::B1T_BYTE)
				{
					add_op(*_curr_code_sec, L"LD A, " + tv.value, false); //A6 BYTE_VALUE
				}
				else
				if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
				{
					add_op(*_curr_code_sec, L"LDW X, " + tv.value, false); //AE WORD_VALUE
				}
				else
				if(req_type == B1Types::B1T_LONG)
				{
					add_op(*_curr_code_sec, L"LDW Y, " + tv.value + L".h", false); //90 AE WORD_VALUE
					add_op(*_curr_code_sec, L"LDW X, " + tv.value + L".l", false); //AE WORD_VALUE
				}
				else
				{
					add_op(*_curr_code_sec, L"LDW Y, " + tv.value + L".h", false); //90 AE WORD_VALUE
					add_op(*_curr_code_sec, L"LDW X, " + tv.value + L".l", false); //AE WORD_VALUE
					add_call_op(L"__LIB_STR_STR_L");
				}
			}
			else
			{
				return C1_T_ERROR::C1_RES_EINTERR;
			}
		}
		else
		if(init_type == B1Types::B1T_STRING)
		{
			if(req_type != B1Types::B1T_STRING)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
			}

			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;
				add_op(*_curr_code_sec, L"LDW X, " + std::get<0>(_str_labels[tv.value]), false); //AE WORD_VALUE
				_req_symbols.insert(std::get<0>(_str_labels[tv.value]));
			}
			else
			{
				return C1_T_ERROR::C1_RES_EINTERR;
			}
		}
		else
		{
			return C1_T_ERROR::C1_RES_EINTERR;
		}
	}
	else
	if(_locals.find(tv.value) != _locals.end())
	{
		// local variable
		int32_t offset = stm8_get_local_offset(tv.value);
		auto err = stm8_load_from_stack(offset, init_type, req_type, req_valtype, rvt, rv);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}
	}
	else
	if(B1CUtils::is_fn_arg(tv.value))
	{
		int32_t offset = -1;
		int32_t arg_off = 0;

		if(_curr_udef_arg_offsets.size() == 1)
		{
			// temporary solution for a single argument case: function prologue code stores it in stack
			offset = _stack_ptr - _curr_udef_args_size + 1;
		}
		else
		{
			int32_t arg_num = B1CUtils::get_fn_arg_index(tv.value);
			arg_off = _curr_udef_arg_offsets[arg_num];
			offset = _stack_ptr + _global_settings.GetRetAddressSize() + arg_off;
		}

		B1_ASM_OPS::const_iterator it;
		auto err = stm8_load_from_stack(offset, init_type, req_type, req_valtype, rvt, rv, &it);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}

		if(init_type == B1Types::B1T_STRING && req_type == B1Types::B1T_STRING)
		{
			// pointer to the last call of __LIB_STR_CPY for the fn argument
			_curr_udef_str_arg_last_use[arg_off] = it;
		}
	}
	else
	{
		auto fn = get_fn(tv);

		if(fn == nullptr)
		{
			// simple variable
			bool is_volatile = false;
			rv = stm8_get_var_addr(tv.value, init_type, req_type, false, &is_volatile);
			if(volatile_var != nullptr)
			{
				*volatile_var = is_volatile;
			}

			if(init_type == B1Types::B1T_BYTE)
			{
				if((req_valtype & LVT::LVT_MEMREF) && req_type == B1Types::B1T_BYTE)
				{
					rvt = LVT::LVT_MEMREF;
				}
				else
				if(req_valtype & LVT::LVT_REG)
				{
					rvt = LVT::LVT_REG;

					add_op(*_curr_code_sec, L"LD A, (" + rv + L")", is_volatile); //B6 SHORT_ADDRESS C6 LONG_ADDRESS

					if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD || req_type == B1Types::B1T_STRING)
					{
						add_op(*_curr_code_sec, L"CLRW X", is_volatile); //5F
						add_op(*_curr_code_sec, L"LD XL, A", is_volatile); //97
						if(req_type == B1Types::B1T_STRING)
						{
							add_call_op(L"__LIB_STR_STR_I", is_volatile);
						}
					}
					else
					if(req_type == B1Types::B1T_LONG)
					{
						add_op(*_curr_code_sec, L"CLRW Y", is_volatile); //90 5F
						add_op(*_curr_code_sec, L"CLRW X", is_volatile); //5F
						add_op(*_curr_code_sec, L"LD XL, A", is_volatile); //97
					}

					rv.clear();
				}
				else
				{
					return C1_T_ERROR::C1_RES_EINTERR;
				}
			}
			else
			if(init_type == B1Types::B1T_INT || init_type == B1Types::B1T_WORD)
			{
				if((req_valtype & LVT::LVT_MEMREF) && req_type != B1Types::B1T_STRING && req_type != B1Types::B1T_LONG)
				{
					rvt = LVT::LVT_MEMREF;
				}
				else
				if(req_valtype & LVT::LVT_REG)
				{
					rvt = LVT::LVT_REG;

					if(req_type == B1Types::B1T_BYTE)
					{
						add_op(*_curr_code_sec, L"LD A, (" + rv + L")", is_volatile); //B6 SHORT_ADDRESS C6 LONG_ADDRESS
					}
					else
					{
						if(req_type == B1Types::B1T_LONG)
						{
							add_op(*_curr_code_sec, L"CLRW Y", is_volatile); //90 5F
						}

						add_op(*_curr_code_sec, L"LDW X, (" + rv + L")", is_volatile); //BE SHORT_ADDRESS CE LONG_ADDRESS

						if(req_type == B1Types::B1T_STRING)
						{
							if(init_type == B1Types::B1T_INT)
							{
								add_call_op(L"__LIB_STR_STR_I", is_volatile);
							}
							else
							{
								add_call_op(L"__LIB_STR_STR_W", is_volatile);
							}
						}
						else
						if(req_type == B1Types::B1T_LONG)
						{
							if(init_type == B1Types::B1T_INT)
							{
								const auto label = emit_label(true);
								add_op(*_curr_code_sec, L"JRPL " + label, is_volatile); //2A SIGNED_BYTE_OFFSET
								_req_symbols.insert(label);
								add_op(*_curr_code_sec, L"DECW Y", is_volatile); //90 5A
								add_lbl(*_curr_code_sec, _curr_code_sec->cend(), label, is_volatile);
								_all_symbols.insert(label);
							}
						}
					}

					rv.clear();
				}
				else
				{
					return C1_T_ERROR::C1_RES_EINTERR;
				}
			}
			else
			if(init_type == B1Types::B1T_LONG)
			{
				if((req_valtype & LVT::LVT_MEMREF) && req_type != B1Types::B1T_STRING)
				{
					rvt = LVT::LVT_MEMREF;
				}
				else
				if(req_valtype & LVT::LVT_REG)
				{
					rvt = LVT::LVT_REG;

					if(req_type == B1Types::B1T_BYTE)
					{
						add_op(*_curr_code_sec, L"LD A, (" + rv + L")", is_volatile); //B6 SHORT_ADDRESS C6 LONG_ADDRESS
					}
					else
					if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
					{
						add_op(*_curr_code_sec, L"LDW X, (" + rv + L")", is_volatile); //BE SHORT_ADDRESS CE LONG_ADDRESS
					}
					else
					if(req_type == B1Types::B1T_STRING)
					{
						add_op(*_curr_code_sec, L"LDW Y, (" + rv + L")", is_volatile); //90 BE SHORT_ADDRESS 90 CE LONG_ADDRESS
						add_op(*_curr_code_sec, L"LDW X, (" + rv + L" + 2)", is_volatile); //BE SHORT_ADDRESS CE LONG_ADDRESS
						add_call_op(L"__LIB_STR_STR_L", is_volatile);
					}
					else
					if(req_type == B1Types::B1T_LONG)
					{
						add_op(*_curr_code_sec, L"LDW Y, (" + rv + L")", is_volatile); //90 BE SHORT_ADDRESS 90 CE LONG_ADDRESS
						add_op(*_curr_code_sec, L"LDW X, (" + rv + L" + 2)", is_volatile); //BE SHORT_ADDRESS CE LONG_ADDRESS
					}

					rv.clear();
				}
				else
				{
					return C1_T_ERROR::C1_RES_EINTERR;
				}
			}
			else
			{
				if(req_type != B1Types::B1T_STRING)
				{
					return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
				}

				if(req_valtype & LVT::LVT_REG)
				{
					rvt = LVT::LVT_REG;

					// STRING variable, copy value
					add_op(*_curr_code_sec, L"LDW X, (" + rv + L")", is_volatile); //BE SHORT_ADDRESS CE LONG_ADDRESS
					add_call_op(L"__LIB_STR_CPY", is_volatile);

					rv.clear();
				}
				else
				{
					return C1_T_ERROR::C1_RES_EINTERR;
				}
			}
		}
		else
		{
			// function without arguments
			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				// call the function
				add_call_op(fn->iname);

				auto err = stm8_arrange_types(init_type, req_type);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}
			else
			{
				return C1_T_ERROR::C1_RES_EINTERR;
			}
		}
	}

	if(res_val != nullptr)
	{
		*res_val = rv;
	}

	if(res_valtype != nullptr)
	{
		*res_valtype = rvt;
	}

	return C1_T_ERROR::C1_RES_OK;
}

// allocates array of default size if necessary
C1_T_ERROR C1STM8Compiler::stm8_arr_alloc_def(const B1_CMP_VAR &var)
{
	int32_t size1 = (10 - b1_opt_base_val) + 1;
	int32_t dimnum, size = 1;

	dimnum = var.dim_num;

	if(dimnum < 1 || dimnum > B1_MAX_VAR_DIM_NUM)
	{
		return static_cast<C1_T_ERROR>(B1_RES_EWSUBSCNT);
	}

	if((_opt_nocheck && b1_opt_explicit_val != 0) || (!var.is_volatile && _allocated_arrays.find(var.name) != _allocated_arrays.end()))
	{
		return C1_T_ERROR::C1_RES_OK;
	}

	// check if memory is allocated
	const auto label = emit_label(true);
	add_op(*_curr_code_sec, L"LDW X, (" + var.name + L")", var.is_volatile); //BE SHORT_ADDRESS, CE LONG_ADDRESS
	_req_symbols.insert(var.name);
	add_op(*_curr_code_sec, L"JRNE " + label, false); //26 SIGNED_BYTE_OFFSET
	_req_symbols.insert(label);

	if(b1_opt_explicit_val == 0)
	{
		for(int i = 0; i < dimnum; i++)
		{
			size *= size1;
		}

		if(var.type == B1Types::B1T_BYTE)
		{
			add_op(*_curr_code_sec, L"LDW X, " + Utils::str_tohex16(size), false); //AE WORD_VALUE
		}
		else
		if(var.type == B1Types::B1T_INT || var.type == B1Types::B1T_WORD || var.type == B1Types::B1T_STRING)
		{
			add_op(*_curr_code_sec, L"LDW X, " + Utils::str_tohex16(size * 2), false); //AE WORD_VALUE
		}
		else
		{
			// LONG type
			add_op(*_curr_code_sec, L"LDW X, " + Utils::str_tohex16(size * 4), false); //AE WORD_VALUE
		}

		add_call_op(L"__LIB_MEM_ALC");

		// save address
		add_op(*_curr_code_sec, L"LDW (" + var.name + L"), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS

		// save array sizes if necessary
		if(!var.fixed_size)
		{
			add_op(*_curr_code_sec, L"CLRW X", false); //5F
			if(b1_opt_base_val == 1)
			{
				add_op(*_curr_code_sec, L"INCW X", false); //5C
			}
			for(int i = 0; i < dimnum; i++)
			{
				add_op(*_curr_code_sec, L"LDW (" + var.name + L" + " + Utils::str_tohex16((i + 1) * 4 - 2) + L"), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS
			}

			add_op(*_curr_code_sec, L"LDW X, " + Utils::str_tohex16(size1), false); //AE WORD_VALUE
			for(int i = 0; i < dimnum; i++)
			{
				add_op(*_curr_code_sec, L"LDW (" + var.name + L" + " + Utils::str_tohex16((i + 1) * 4) + L"), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS
			}
		}
	}
	else
	{
		add_op(*_curr_code_sec, L"MOV (__LIB_ERR_LAST_ERR), " + _RTE_error_names[B1C_T_RTERROR::B1C_RTE_ARR_UNALLOC], false); //35 BYTE_VALUE LONG_ADDRESS
		_init_files.push_back(L"__LIB_ERR_LAST_ERR");
		add_call_op(L"__LIB_ERR_HANDLER");
	}

	add_lbl(*_curr_code_sec, _curr_code_sec->cend(), label, false);
	_all_symbols.insert(label);

	_allocated_arrays.insert(var.name);

	return C1_T_ERROR::C1_RES_OK;
}

// fixed size and known arguments: no code, offset is returned
// fixed size and unknown arguments: code, no offset
// if the function sets imm_offset to true, offset variable contains offset from array's base address
C1_T_ERROR C1STM8Compiler::stm8_arr_offset(const B1_CMP_ARG &arg, bool &imm_offset, int32_t &offset)
{
	auto ma = _mem_areas.find(arg[0].value);
	bool is_ma = ma != _mem_areas.end();
	const auto &var = is_ma ? ma : _vars.find(arg[0].value);
	bool known_size = is_ma ? true : var->second.fixed_size;
	int32_t dims_size;

	bool imm_args = true;
	offset = 0;

	if(!is_ma)
	{
		_req_symbols.insert(arg[0].value);
	}

	dims_size = 1;

	for(int32_t i = (arg.size() - 2); i >= 0; i--)
	{
		const auto &tv = arg[i + 1];

		if(!B1CUtils::is_imm_val(tv.value))
		{
			imm_args = false;
			break;
		}

		if(known_size)
		{
			int32_t av;

			auto err = Utils::str2int32(tv.value, av);
			if(err != B1_RES_OK)
			{
				return static_cast<C1_T_ERROR>(err);
			}

			// dimension lbound
			av -= var->second.dims[i * 2];

			offset += dims_size * av;
			dims_size *= var->second.dims[i * 2 + 1] - var->second.dims[i * 2] + 1;
		}
	}

	if(known_size && imm_args)
	{
		imm_offset = true;
		return C1_T_ERROR::C1_RES_OK;
	}

	if(imm_offset)
	{
		return C1_T_ERROR::C1_RES_ENOIMMOFF;
	}

	if(arg.size() == 2)
	{
		// one-dimensional array
		const auto &tv = arg[1];

		auto err = stm8_load(tv, B1Types::B1T_INT, LVT::LVT_REG);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}

		if(known_size)
		{
			if(var->second.dims[0] != 0)
			{
				add_op(*_curr_code_sec, L"SUBW X, " + Utils::str_tohex16(var->second.dims[0]), false); //1D WORD_VALUE
			}
		}
		else
		{
			if(is_ma || !var->second.is_0_based[0])
			{
				add_op(*_curr_code_sec, L"SUBW X, (" + arg[0].value + L" + 0x2)", false); //72 B0 LONG_ADDRESS
			}
		}
	}
	else
	if(known_size)
	{
		// multidimensional array of fixed size
		dims_size = 1;

		for(int32_t i = (arg.size() - 2); i >= 0; i--)
		{
			const auto &tv = arg[i + 1];

			if(i != (arg.size() - 2))
			{
				auto dsval = Utils::str_tohex16(dims_size);
				add_op(*_curr_code_sec, L"PUSH " + dsval + L".ll", false); //4B BYTE_VALUE
				add_op(*_curr_code_sec, L"PUSH " + dsval + L".lh", false); //4B BYTE_VALUE
				_stack_ptr += 2;
			}

			auto err = stm8_load(tv, B1Types::B1T_INT, LVT::LVT_REG);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			if(var->second.dims[i * 2] != 0)
			{
				add_op(*_curr_code_sec, L"SUBW X, " + Utils::str_tohex16(var->second.dims[i * 2]), false); //1D WORD_VALUE
			}

			if(i != (arg.size() - 2))
			{
				add_call_op(L"__LIB_COM_MUL16");
				add_op(*_curr_code_sec, L"ADDW X, (3, SP)", false); //72 FB BYTE_OFFSET
				add_op(*_curr_code_sec, L"LDW (3, SP), X", false); //1F BYTE_OFFSET
				add_op(*_curr_code_sec, L"POPW X", false); //85
				_stack_ptr -= 2;
			}
			else
			{
				// offset
				add_op(*_curr_code_sec, L"PUSHW X", false); //89
				_stack_ptr += 2;

			}

			dims_size *= var->second.dims[i * 2 + 1] - var->second.dims[i * 2] + 1;
		}

		add_op(*_curr_code_sec, L"POPW X", false); //85
		_stack_ptr -= 2;
	}
	else
	{
		// multidimensional array of any size
		// offset
		auto err = stm8_load(arg.back(), B1Types::B1T_INT, LVT::LVT_REG);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}
		if(is_ma || !var->second.is_0_based[arg.size() - 2])
		{
			add_op(*_curr_code_sec, L"SUBW X, (" + arg[0].value + L" + " + Utils::str_tohex16(2 + (arg.size() - 2) * 4) + L")", false); //72 B0 LONG_ADDRESS
		}
		add_op(*_curr_code_sec, L"PUSHW X", false); //89
		_stack_ptr += 2;

		// dimensions size
		add_op(*_curr_code_sec, L"LDW X, (" + arg[0].value + L" + " + Utils::str_tohex16(2 + 2 + (arg.size() - 2) * 4) + L")", false); //BE SHORT_ADDRESS CE LONG_ADDRESS
		add_op(*_curr_code_sec, L"PUSHW X", false); //89
		_stack_ptr += 2;

		for(int32_t i = (arg.size() - 3); i >= 0; i--)
		{
			const auto &tv = arg[i + 1];

			auto err = stm8_load(tv, B1Types::B1T_INT, LVT::LVT_REG);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			if(is_ma || !var->second.is_0_based[i])
			{
				add_op(*_curr_code_sec, L"SUBW X, (" + arg[0].value + L" + " + Utils::str_tohex16(2 + i * 4) + L")", false); //72 B0 LONG_ADDRESS
			}
			add_call_op(L"__LIB_COM_MUL16");
			add_op(*_curr_code_sec, L"ADDW X, (3, SP)", false); //72 FB BYTE_OFFSET
			add_op(*_curr_code_sec, L"LDW (3, SP), X", false); //1F BYTE_OFFSET

			if(i != 0)
			{
				add_op(*_curr_code_sec, L"LDW X, (" + arg[0].value + L" + " + Utils::str_tohex16(2 + 2 + i * 4) + L")", false); //BE SHORT_ADDRESS CE LONG_ADDRESS
				add_call_op(L"__LIB_COM_MUL16");
				add_op(*_curr_code_sec, L"LDW (1, SP), X", false); //1F BYTE_OFFSET
			}
		}

		add_op(*_curr_code_sec, L"POPW X", false); //85
		_stack_ptr -= 2;
		add_op(*_curr_code_sec, L"POPW X", false); //85
		_stack_ptr -= 2;
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::stm8_load(const B1_CMP_ARG &arg, const B1Types req_type, LVT req_valtype, LVT *res_valtype /*= nullptr*/, std::wstring *res_val /*= nullptr*/, bool *volatile_var /*= nullptr*/)
{
	if(arg.size() == 1)
	{
		return stm8_load(arg[0], req_type, req_valtype, res_valtype, res_val, volatile_var);
	}

	if(volatile_var != nullptr)
	{
		*volatile_var = false;
	}

	if(!(req_valtype & (LVT::LVT_REG | LVT::LVT_MEMREF)))
	{
		return C1_T_ERROR::C1_RES_EINTERR;
	}

	std::wstring rv;
	LVT rvt;
	auto init_type = arg[0].type;

	// subscripted variable or function call
	auto fn = get_fn(arg);

	if(fn == nullptr)
	{
		// subscripted variable
		auto ma = _mem_areas.find(arg[0].value);
		bool is_ma = ma != _mem_areas.end();
		bool is_volatile = false;

		if(is_ma)
		{
			if(ma->second.dim_num != arg.size() - 1)
			{
				return static_cast<C1_T_ERROR>(B1_RES_EWRARGCNT);
			}

			is_volatile = ma->second.is_volatile;
		}
		else
		{
			if(!(req_valtype & LVT::LVT_REG))
			{
				return C1_T_ERROR::C1_RES_EINTERR;
			}

			const auto &var = _vars.find(arg[0].value)->second;

			if(var.dim_num != arg.size() - 1)
			{
				return static_cast<C1_T_ERROR>(B1_RES_EWRARGCNT);
			}

			// allocate array of default size if necessary
			auto err = stm8_arr_alloc_def(var);
			if(err != static_cast<C1_T_ERROR>(B1_RES_OK))
			{
				return err;
			}

			is_volatile = var.is_volatile;

			_req_symbols.insert(arg[0].value);
		}

		if(volatile_var != nullptr)
		{
			*volatile_var = is_volatile;
		}

		// calculate memory offset
		// request immediate offset value if LVT_MEMREF is the only option
		bool imm_offset = (req_valtype & LVT::LVT_MEMREF) && !(req_valtype & LVT::LVT_REG);
		int32_t offset = 0;
		auto err = stm8_arr_offset(arg, imm_offset, offset);
		if(err != static_cast<C1_T_ERROR>(B1_RES_OK))
		{
			return err;
		}

		rv = is_ma ? (ma->second.use_symbol ? ma->second.symbol : std::to_wstring(ma->second.address)) :
			arg[0].value;

		// get value
		if(init_type == B1Types::B1T_BYTE)
		{
			if((req_valtype & LVT::LVT_MEMREF) && is_ma && imm_offset && req_type == B1Types::B1T_BYTE)
			{
				rvt = LVT::LVT_MEMREF;
				rv += L" + " + Utils::str_tohex16(offset);
			}
			else
			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				if(is_ma)
				{
					if(imm_offset)
					{
						add_op(*_curr_code_sec, L"LD A, (" + rv + L" + " + Utils::str_tohex16(offset) + L")", is_volatile); //B6 SHORT_ADDRESS C6 LONG_ADDRESS
					}
					else
					{
						add_op(*_curr_code_sec, L"LD A, (" + rv + L", X)", is_volatile); //E6 BYTE_OFFSET D6 WORD_OFFSET
					}
				}
				else
				{
					if(imm_offset)
					{
						add_op(*_curr_code_sec, L"LDW X, (" + rv + L")", is_volatile); //BE BYTE_OFFSET CE WORD_OFFSET
						if(offset == 0)
						{
							add_op(*_curr_code_sec, L"LD A, (X)", is_volatile); //F6
						}
						else
						{
							add_op(*_curr_code_sec, L"LD A, (" + Utils::str_tohex16(offset) + L", X)", is_volatile); //E6 BYTE_OFFSET D6 WORD_OFFSET
						}
					}
					else
					{
						add_op(*_curr_code_sec, L"LD A, ([" + rv + L"], X)", is_volatile); //92 D6 BYTE_OFFSET 72 D6 WORD_OFFSET
					}
				}

				rv.clear();

				if(req_type != B1Types::B1T_BYTE)
				{
					if(req_type == B1Types::B1T_LONG)
					{
						add_op(*_curr_code_sec, L"CLRW Y", is_volatile); //90 5F
					}
					add_op(*_curr_code_sec, L"CLRW X", is_volatile); //5F
					add_op(*_curr_code_sec, L"LD XL, A", is_volatile); //97
				}
				if(req_type == B1Types::B1T_STRING)
				{
					add_call_op(L"__LIB_STR_STR_I", is_volatile);
				}
			}
			else
			{
				return C1_T_ERROR::C1_RES_EINTERR;
			}
		}
		else
		if(init_type == B1Types::B1T_INT || init_type == B1Types::B1T_WORD)
		{
			if(imm_offset)
			{
				offset *= 2;
			}
			else
			{
				add_op(*_curr_code_sec, L"SLAW X", is_volatile); //58
			}

			if(req_type == B1Types::B1T_BYTE)
			{
				if(imm_offset)
				{
					offset++;
				}
				else
				{
					add_op(*_curr_code_sec, L"INCW X", is_volatile); //5C
				}
			}

			if((req_valtype & LVT::LVT_MEMREF) && is_ma && imm_offset && (req_type == B1Types::B1T_BYTE || req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD))
			{
				rvt = LVT::LVT_MEMREF;
				rv += L" + " + Utils::str_tohex16(offset);
			}
			else
			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				if(req_type == B1Types::B1T_BYTE)
				{
					if(is_ma)
					{
						if(imm_offset)
						{
							add_op(*_curr_code_sec, L"LD A, (" + rv + L" + " + Utils::str_tohex16(offset) + L")", is_volatile); //B6 SHORT_ADDRESS C6 LONG_ADDRESS
						}
						else
						{
							add_op(*_curr_code_sec, L"LD A, (" + rv + L", X)", is_volatile); //E6 BYTE_OFFSET D6 WORD_OFFSET
						}
					}
					else
					{
						if(imm_offset)
						{
							add_op(*_curr_code_sec, L"LDW X, (" + rv + L")", is_volatile); //BE BYTE_OFFSET CE WORD_OFFSET
							if (offset == 0)
							{
								add_op(*_curr_code_sec, L"LD A, (X)", is_volatile); //F6
							}
							else
							{
								add_op(*_curr_code_sec, L"LD A, (" + Utils::str_tohex16(offset) + L", X)", is_volatile); //E6 BYTE_OFFSET D6 WORD_OFFSET
							}
						}
						else
						{
							add_op(*_curr_code_sec, L"LD A, ([" + rv + L"], X)", is_volatile); //92 D6 BYTE_OFFSET 72 D6 WORD_OFFSET
						}
					}
				}
				else
				{
					if(req_type == B1Types::B1T_LONG)
					{
						add_op(*_curr_code_sec, L"CLRW Y", is_volatile); //90 5F
					}

					if(is_ma)
					{
						if(imm_offset)
						{
							add_op(*_curr_code_sec, L"LDW X, (" + rv + L" + " + Utils::str_tohex16(offset) + L")", is_volatile); //BE SHORT_ADDRESS CE LONG_ADDRESS
						}
						else
						{
							add_op(*_curr_code_sec, L"LDW X, (" + rv + L", X)", is_volatile); //EE BYTE_OFFSET DE WORD_OFFSET
						}
					}
					else
					{
						if(imm_offset)
						{
							add_op(*_curr_code_sec, L"LDW X, (" + rv + L")", is_volatile); //BE BYTE_OFFSET CE WORD_OFFSET
							if(offset == 0)
							{
								add_op(*_curr_code_sec, L"LDW X, (X)", is_volatile); //FE
							}
							else
							{
								add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", X)", is_volatile); //EE BYTE_OFFSET DE WORD_OFFSET
							}
						}
						else
						{
							add_op(*_curr_code_sec, L"LDW X, ([" + rv + L"], X)", is_volatile); //92 DE BYTE_OFFSET 72 DE WORD_OFFSET
						}
					}

					if(req_type == B1Types::B1T_LONG)
					{
						if(init_type == B1Types::B1T_INT)
						{
							const auto label = emit_label(true);
							add_op(*_curr_code_sec, L"JRPL " + label, is_volatile); //2A SIGNED_BYTE_OFFSET
							_req_symbols.insert(label);
							add_op(*_curr_code_sec, L"DECW Y", is_volatile); //90 5A
							add_lbl(*_curr_code_sec, _curr_code_sec->cend(), label, is_volatile);
							_all_symbols.insert(label);
						}
					}
					else
					if(req_type == B1Types::B1T_STRING)
					{
						if(init_type == B1Types::B1T_INT)
						{
							add_call_op(L"__LIB_STR_STR_I", is_volatile);
						}
						else
						{
							add_call_op(L"__LIB_STR_STR_W", is_volatile);
						}
					}
				}

				rv.clear();
			}
			else
			{
				return C1_T_ERROR::C1_RES_EINTERR;
			}
		}
		else
		if(init_type == B1Types::B1T_LONG)
		{
			if(imm_offset)
			{
				offset *= 4;
			}
			else
			{
				add_op(*_curr_code_sec, L"SLAW X", is_volatile); //58
				add_op(*_curr_code_sec, L"SLAW X", is_volatile); //58
			}

			if(req_type == B1Types::B1T_BYTE)
			{
				if(imm_offset)
				{
					offset += 3;
				}
				else
				{
					add_op(*_curr_code_sec, L"ADDW X, 3", is_volatile); //1C WORD_VALUE
				}
			}
			if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
			{
				if(imm_offset)
				{
					offset += 2;
				}
				else
				{
					add_op(*_curr_code_sec, L"INCW X", is_volatile); //5C
					add_op(*_curr_code_sec, L"INCW X", is_volatile); //5C
				}
			}

			if((req_valtype & LVT::LVT_MEMREF) && is_ma && imm_offset && (req_type == B1Types::B1T_BYTE || req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD || req_type == B1Types::B1T_LONG))
			{
				rvt = LVT::LVT_MEMREF;
				rv += L" + " + Utils::str_tohex16(offset);
			}
			else
			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				if(req_type == B1Types::B1T_BYTE)
				{
					if(is_ma)
					{
						if(imm_offset)
						{
							add_op(*_curr_code_sec, L"LD A, (" + rv + L" + " + Utils::str_tohex16(offset) + L")", is_volatile); //B6 SHORT_ADDRESS C6 LONG_ADDRESS
						}
						else
						{
							add_op(*_curr_code_sec, L"LD A, (" + rv + L", X)", is_volatile); //E6 BYTE_OFFSET D6 WORD_OFFSET
						}
					}
					else
					{
						if(imm_offset)
						{
							add_op(*_curr_code_sec, L"LDW X, (" + rv + L")", is_volatile); //BE BYTE_OFFSET CE WORD_OFFSET
							if(offset == 0)
							{
								add_op(*_curr_code_sec, L"LD A, (X)", is_volatile); //F6
							}
							else
							{
								add_op(*_curr_code_sec, L"LD A, (" + Utils::str_tohex16(offset) + L", X)", is_volatile); //E6 BYTE_OFFSET D6 WORD_OFFSET
							}
						}
						else
						{
							add_op(*_curr_code_sec, L"LD A, ([" + rv + L"], X)", is_volatile); //92 D6 BYTE_OFFSET 72 D6 WORD_OFFSET
						}
					}
				}
				else
				if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
				{
					if(is_ma)
					{
						if(imm_offset)
						{
							add_op(*_curr_code_sec, L"LDW X, (" + rv + L" + " + Utils::str_tohex16(offset) + L")", is_volatile); //BE SHORT_ADDRESS CE LONG_ADDRESS
						}
						else
						{
							add_op(*_curr_code_sec, L"LDW X, (" + rv + L", X)", is_volatile); //EE BYTE_OFFSET DE WORD_OFFSET
						}
					}
					else
					{
						if(imm_offset)
						{
							add_op(*_curr_code_sec, L"LDW X, (" + rv + L")", is_volatile); //BE BYTE_OFFSET CE WORD_OFFSET
							if(offset == 0)
							{
								add_op(*_curr_code_sec, L"LDW X, (X)", is_volatile); //FE
							}
							else
							{
								add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", X)", is_volatile); //EE BYTE_OFFSET DE WORD_OFFSET
							}
						}
						else
						{
							add_op(*_curr_code_sec, L"LDW X, ([" + rv + L"], X)", is_volatile); //92 DE BYTE_OFFSET 72 DE WORD_OFFSET
						}
					}
				}
				else
				if(req_type == B1Types::B1T_LONG || req_type == B1Types::B1T_STRING)
				{
					if(is_ma)
					{
						if(imm_offset)
						{
							add_op(*_curr_code_sec, L"LDW Y, (" + rv + L" + " + Utils::str_tohex16(offset) + L")", is_volatile); //90 BE SHORT_ADDRESS 90 CE LONG_ADDRESS
							add_op(*_curr_code_sec, L"LDW X, (" + rv + L" + " + Utils::str_tohex16(offset) + L" + 2)", is_volatile); //BE SHORT_ADDRESS CE LONG_ADDRESS
						}
						else
						{
							add_op(*_curr_code_sec, L"LDW Y, X", is_volatile); //90 93
							add_op(*_curr_code_sec, L"LDW Y, (" + rv + L", Y)", is_volatile); //90 EE BYTE_OFFSET 90 DE WORD_OFFSET
							add_op(*_curr_code_sec, L"LDW X, (" + rv + L" + 2, X)", is_volatile); //EE BYTE_OFFSET DE WORD_OFFSET
						}
					}
					else
					{
						if(imm_offset)
						{
							add_op(*_curr_code_sec, L"LDW X, (" + rv + L")", is_volatile); //BE BYTE_OFFSET CE WORD_OFFSET
							add_op(*_curr_code_sec, L"LDW Y, X", is_volatile); //90 93
							if(offset == 0)
							{
								add_op(*_curr_code_sec, L"LDW Y, (Y)", is_volatile); //90 FE
							}
							else
							{
								add_op(*_curr_code_sec, L"LDW Y, (" + Utils::str_tohex16(offset) + L", Y)", is_volatile); //90 EE BYTE_OFFSET 90 DE WORD_OFFSET
							}
							add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L" + 2, X)", is_volatile); //EE BYTE_OFFSET DE WORD_OFFSET
						}
						else
						{
							add_op(*_curr_code_sec, L"ADDW X, (" + rv + L")", is_volatile); //72 BB WORD_OFFSET
							add_op(*_curr_code_sec, L"LDW Y, X", is_volatile); //90 93
							add_op(*_curr_code_sec, L"LDW Y, (Y)", is_volatile); //90 FE
							add_op(*_curr_code_sec, L"LDW X, (2, X)", is_volatile); //EE BYTE_OFFSET
						}
					}


					if(req_type == B1Types::B1T_STRING)
					{
						add_call_op(L"__LIB_STR_STR_L", is_volatile);
					}
				}

				rv.clear();
			}
			else
			{
				return C1_T_ERROR::C1_RES_EINTERR;
			}
		}
		else
		{
			// string type
			if(imm_offset)
			{
				offset *= 2;
			}
			else
			{
				add_op(*_curr_code_sec, L"SLAW X", is_volatile); //58
			}

			if(req_type != B1Types::B1T_STRING)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
			}

			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				if(is_ma)
				{
					// const and static arrays of strings
					if(imm_offset)
					{
						add_op(*_curr_code_sec, L"LDW X, (" + rv + L" + " + Utils::str_tohex16(offset) + L")", is_volatile); //BE SHORT_ADDRESS CE LONG_ADDRESS
					}
					else
					{
						add_op(*_curr_code_sec, L"LDW X, (" + rv + L", X)", is_volatile); //EE BYTE_OFFSET DE WORD_OFFSET
					}
				}
				else
				{
					if(imm_offset)
					{
						add_op(*_curr_code_sec, L"LDW X, (" + rv + L")", is_volatile); //BE BYTE_OFFSET CE WORD_OFFSET
						if(offset == 0)
						{
							add_op(*_curr_code_sec, L"LDW X, (X)", is_volatile); //FE
						}
						else
						{
							add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", X)", is_volatile); //EE BYTE_OFFSET DE WORD_OFFSET
						}
					}
					else
					{
						add_op(*_curr_code_sec, L"LDW X, ([" + rv + L"], X)", is_volatile); //92 DE BYTE_OFFSET 72 DE WORD_OFFSET
					}
				}

				if(!(is_ma && ma->second.is_const))
				{
					add_call_op(L"__LIB_STR_CPY", is_volatile);
				}

				rv.clear();
			}
			else
			{
				return C1_T_ERROR::C1_RES_EINTERR;
			}
		}
	}
	else
	{
		// function call
		if(!(req_valtype & LVT::LVT_REG))
		{
			return C1_T_ERROR::C1_RES_EINTERR;
		}

		// check for special data type conversion functions (inline)
		if(fn->args.size() == 1 && fn->isstdfn && fn->iname.empty())
		{
			if((fn->name == L"CBYTE" || fn->name == L"CINT" || fn->name == L"CWRD" || fn->name == L"CLNG") && (req_valtype & LVT::LVT_REG))
			{
				auto err = stm8_load(arg[1], fn->rettype, LVT::LVT_REG);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				if(fn->rettype != req_type)
				{
					auto err = stm8_arrange_types(fn->rettype, req_type);
					if(err != C1_T_ERROR::C1_RES_OK)
					{
						return err;
					}
				}

				if(res_val != nullptr)
				{
					res_val->clear();
				}

				if(res_valtype != nullptr)
				{
					*res_valtype = LVT::LVT_REG;
				}

				return C1_T_ERROR::C1_RES_OK;
			}

			return C1_T_ERROR::C1_RES_EINTERR;
		}

		if(fn->args.size() == 2 && fn->isstdfn && fn->iname.empty())
		{
			if((fn->name == L"IOCTL" || fn->name == L"IOCTL$") && (req_valtype & LVT::LVT_REG))
			{
				auto err = stm8_write_ioctl_fn(arg);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				if(init_type != req_type)
				{
					auto err = stm8_arrange_types(init_type, req_type);
					if(err != C1_T_ERROR::C1_RES_OK)
					{
						return err;
					}
				}

				if(res_val != nullptr)
				{
					res_val->clear();
				}

				if(res_valtype != nullptr)
				{
					*res_valtype = LVT::LVT_REG;
				}

				return C1_T_ERROR::C1_RES_OK;
			}
		}
		 
		// arguments size in stack
		int args_size = 0;
		int arg_ind = 0;

		// only one argument (or the first argument of standard function), pass the value in registers
		if(fn->args.size() == 1 || fn->isstdfn)
		{
			arg_ind = 1;
		}

		// transfer arguments in stack, starting from the first one
		for(; arg_ind < fn->args.size(); arg_ind++)
		{
			LVT lvt = LVT::LVT_NONE;
			std::wstring res_val;

			const auto &farg = fn->args[arg_ind];

			// for BYTE type try to use memref
			if(farg.type == B1Types::B1T_BYTE)
			{
				auto err = stm8_load(arg[arg_ind + 1], B1Types::B1T_BYTE, LVT::LVT_MEMREF, nullptr, &res_val);
				if(err == C1_T_ERROR::C1_RES_OK)
				{
					add_op(*_curr_code_sec, L"PUSH (" + res_val + L")", false); //3B LONG_ADDRESS
					_stack_ptr++;
					args_size++;
					continue;
				}
			}

			auto err = stm8_load(arg[arg_ind + 1], farg.type, LVT::LVT_REG | LVT::LVT_IMMVAL, &lvt, &res_val);
			if(err != static_cast<C1_T_ERROR>(B1_RES_OK))
			{
				return err;
			}

			if(lvt == LVT::LVT_IMMVAL)
			{
				if(farg.type == B1Types::B1T_BYTE)
				{
					add_op(*_curr_code_sec, L"PUSH " + res_val, false); //4B BYTE_VALUE
					_stack_ptr++;
					args_size++;
				}
				else
				{
					add_op(*_curr_code_sec, L"PUSH " + res_val + L".ll", false); //4B BYTE_VALUE
					add_op(*_curr_code_sec, L"PUSH " + res_val + L".lh", false); //4B BYTE_VALUE
					_stack_ptr += 2;
					args_size += 2;

					if(farg.type == B1Types::B1T_LONG)
					{
						add_op(*_curr_code_sec, L"PUSH " + res_val + L".hl", false); //4B BYTE_VALUE
						add_op(*_curr_code_sec, L"PUSH " + res_val + L".hh", false); //4B BYTE_VALUE
						_stack_ptr += 2;
						args_size += 2;
					}
				}
			}
			else
			{
				if(farg.type == B1Types::B1T_BYTE)
				{
					add_op(*_curr_code_sec, L"PUSH A", false); //88
					_stack_ptr++;
					args_size++;
				}
				else
				{
					add_op(*_curr_code_sec, L"PUSHW X", false); //89
					_stack_ptr += 2;
					args_size += 2;

					if(farg.type == B1Types::B1T_LONG)
					{
						add_op(*_curr_code_sec, L"PUSHW Y", false); //90 89
						_stack_ptr += 2;
						args_size += 2;
					}
				}
			}
		}

		// only one argument (or the first argument of standard function), pass the value in registers
		if(fn->args.size() == 1 || fn->isstdfn)
		{
			LVT lvt = LVT::LVT_NONE;
			std::wstring res_val;
			auto err = stm8_load(arg[1], fn->args[0].type, LVT::LVT_REG, &lvt, &res_val);
			if(err != static_cast<C1_T_ERROR>(B1_RES_OK))
			{
				return err;
			}
		}

		if(req_valtype & LVT::LVT_REG)
		{
			rvt = LVT::LVT_REG;
			add_call_op(fn->iname);

			if(fn->args.size() > 1)
			{
				// cleanup stack
				add_op(*_curr_code_sec, L"ADDW SP, " + Utils::str_tohex16(args_size), false); //5B BYTE_VALUE
				_stack_ptr -= args_size;
			}

			auto err = stm8_arrange_types(init_type, req_type);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
		}
		else
		{
			return C1_T_ERROR::C1_RES_EINTERR;
		}
	}

	if(res_val != nullptr)
	{
		*res_val = rv;
	}

	if(res_valtype != nullptr)
	{
		*res_valtype = rvt;
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::stm8_init_array(const B1_CMP_CMD &cmd, const B1_CMP_VAR &var)
{
	int32_t data_size;

	if(!B1CUtils::get_asm_type(cmd.args[1][0].type, nullptr, &data_size))
	{
		return C1_T_ERROR::C1_RES_EINVTYPNAME;
	}

	_req_symbols.insert(var.name);

	if(var.fixed_size)
	{
		auto err = stm8_calc_array_size(var, data_size);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}
	}
	else
	{
		int32_t dims = (cmd.args.size() - 2) / 2;

		for(int32_t i = 0; i < dims; i++)
		{
			// lbound
			auto err = stm8_load(cmd.args[2 + i * 2], B1Types::B1T_INT, LVT::LVT_REG);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			// save lbound
			add_op(*_curr_code_sec, L"LDW (" + cmd.args[0][0].value + L" + " + Utils::str_tohex16((i * 2 + 1) * 2) + L"), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS

			// ubound
			err = stm8_load(cmd.args[2 + i * 2 + 1], B1Types::B1T_INT, LVT::LVT_REG);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			// subtract lbound value
			add_op(*_curr_code_sec, L"SUBW X, (" + cmd.args[0][0].value + L" + " + Utils::str_tohex16((i * 2 + 1) * 2) + L")", false); //72 B0 WORD_OFFSET
			add_op(*_curr_code_sec, L"INCW X", false); //5C
			// save dimension size
			add_op(*_curr_code_sec, L"LDW (" + cmd.args[0][0].value + L" + " + Utils::str_tohex16((i * 2 + 2) * 2) + L"), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS

			if(i != 0)
			{
				add_call_op(L"__LIB_COM_MUL16");
				
				if(i == dims - 1)
				{
					add_op(*_curr_code_sec, L"ADDW SP, 2", false); //5B BYTE_VALUE
					_stack_ptr -= 2;
				}
				else
				{
					add_op(*_curr_code_sec, L"LDW (1, SP), X", false); //1F BYTE_OFFSET
				}
			}

			if(i == 0 && i != dims - 1)
			{
				add_op(*_curr_code_sec, L"PUSHW X", false); //89
				_stack_ptr += 2;
			}
		}

		if(data_size == 2)
		{
			// 2-byte types: data size = arr. size * 2
			add_op(*_curr_code_sec, L"SLAW X", false); //58
		}
		else
		if(data_size == 4)
		{
			// 4-byte types: data size = arr. size * 4
			add_op(*_curr_code_sec, L"SLAW X", false); //58
			add_op(*_curr_code_sec, L"SLAW X", false); //58
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::stm8_st_ga(const B1_CMP_CMD &cmd, const B1_CMP_VAR &var)
{
	// report error if the array is already allocated
	add_op(*_curr_code_sec, L"LDW X, (" + cmd.args[0][0].value + L")", var.is_volatile); //BE SHORT_ADDRESS CE LONG_ADDRESS
	_req_symbols.insert(cmd.args[0][0].value);
	const auto label = emit_label(true);
	add_op(*_curr_code_sec, L"JREQ " + label, false); //27 SIGNED_BYTE_OFFSET
	_req_symbols.insert(label);
	add_op(*_curr_code_sec, L"MOV (__LIB_ERR_LAST_ERR), " + _RTE_error_names[B1C_T_RTERROR::B1C_RTE_ARR_ALLOC], false); //35 BYTE_VALUE LONG_ADDRESS
	_init_files.push_back(L"__LIB_ERR_LAST_ERR");
	add_call_op(L"__LIB_ERR_HANDLER");
	add_lbl(*_curr_code_sec, _curr_code_sec->cend(), label, false);
	_all_symbols.insert(label);

	auto err = stm8_init_array(cmd, var);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	add_call_op(L"__LIB_MEM_ALC");

	// save address
	add_op(*_curr_code_sec, L"LDW (" + cmd.args[0][0].value + L"), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::stm8_store(const B1_TYPED_VALUE &tv)
{
	if(Utils::check_const_name(tv.value))
	{
		return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
	}

	if(_locals.find(tv.value) != _locals.end())
	{
		// local variable
		int32_t offset = stm8_get_local_offset(tv.value);

		if(offset < 0 || offset > (tv.type == B1Types::B1T_LONG ? 253 : 255))
		{
			return C1_T_ERROR::C1_RES_ESTCKOVF;
		}

		if(tv.type == B1Types::B1T_BYTE)
		{
			add_op(*_curr_code_sec, L"LD (" + Utils::str_tohex16(offset) + L", SP), A", false); //6B BYTE_OFFSET
		}
		else
		if(tv.type == B1Types::B1T_INT || tv.type == B1Types::B1T_WORD)
		{
			add_op(*_curr_code_sec, L"LDW (" + Utils::str_tohex16(offset) + L", SP), X", false); //1F BYTE_OFFSET
		}
		else
		if(tv.type == B1Types::B1T_LONG)
		{
			add_op(*_curr_code_sec, L"LDW (" + Utils::str_tohex16(offset) + L", SP), Y", false); //17 BYTE_OFFSET
			add_op(*_curr_code_sec, L"LDW (" + Utils::str_tohex16(offset) + L" + 2, SP), X", false); //1F BYTE_OFFSET
		}
		else
		{
			// string
			if(_clear_locals.find(tv.value) == _clear_locals.end())
			{
				// release previous string value
				add_op(*_curr_code_sec, L"PUSHW X", false); //89
				_stack_ptr += 2;
				offset += 2;
				if(offset > 255)
				{
					return C1_T_ERROR::C1_RES_ESTCKOVF;
				}

				add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)", false); //1E BYTE_OFFSET
				add_call_op(L"__LIB_STR_RLS");
				add_op(*_curr_code_sec, L"POPW X", false); //85
				_stack_ptr -= 2;
				offset -= 2;
			}
			else
			{
				_clear_locals.erase(tv.value);
			}

			add_op(*_curr_code_sec, L"LDW (" + Utils::str_tohex16(offset) + L", SP), X", false); //1F BYTE_OFFSET
		}
	}
	else
	{
		// simple variable
		bool is_volatile = false;
		auto dst = stm8_get_var_addr(tv.value, tv.type, tv.type, true, &is_volatile);

		if(tv.type == B1Types::B1T_BYTE)
		{
			add_op(*_curr_code_sec, L"LD (" + dst + L"), A", is_volatile); //B7 SHORT_ADDRESS C7 LONG_ADDRESS
		}
		else
		if(tv.type == B1Types::B1T_INT || tv.type == B1Types::B1T_WORD)
		{
			add_op(*_curr_code_sec, L"LDW (" + dst + L"), X", is_volatile); //BF SHORT_ADDRESS CF LONG_ADDRESS
		}
		else
		if(tv.type == B1Types::B1T_LONG)
		{
			add_op(*_curr_code_sec, L"LDW (" + dst + L"), Y", is_volatile); //90 BF SHORT_ADDRESS 90 CF LONG_ADDRESS
			add_op(*_curr_code_sec, L"LDW (" + dst + L" + 2), X", is_volatile); //BF SHORT_ADDRESS CF LONG_ADDRESS
		}
		else
		{
			// STRING variable
			// release previous string value
			add_op(*_curr_code_sec, L"PUSHW X", is_volatile); //89
			_stack_ptr += 2;
			add_op(*_curr_code_sec, L"LDW X, (" + dst + L")", is_volatile); //BE SHORT_ADDRESS, CE LONG_ADDRESS
			add_call_op(L"__LIB_STR_RLS", is_volatile);
			add_op(*_curr_code_sec, L"POPW X", is_volatile); //85
			_stack_ptr -= 2;

			add_op(*_curr_code_sec, L"LDW (" + dst + L"), X", is_volatile); //BF SHORT_ADDRESS CF LONG_ADDRESS
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::stm8_store(const B1_CMP_ARG &arg)
{
	if(arg.size() == 1)
	{
		return stm8_store(arg[0]);
	}

	// subscripted variable
	auto ma = _mem_areas.find(arg[0].value);
	bool is_ma = ma != _mem_areas.end();
	std::wstring dst;

	if(is_ma)
	{
		if(ma->second.dim_num != arg.size() - 1)
		{
			return static_cast<C1_T_ERROR>(B1_RES_EWRARGCNT);
		}

		dst = ma->second.use_symbol ? ma->second.symbol : std::to_wstring(ma->second.address);
	}

	const auto &var = is_ma ? ma->second : _vars.find(arg[0].value)->second;

	if(!is_ma)
	{
		if(var.dim_num != arg.size() - 1)
		{
			return static_cast<C1_T_ERROR>(B1_RES_EWRARGCNT);
		}

		dst = arg[0].value;
		
		_req_symbols.insert(dst);
	}

	bool is_volatile = var.is_volatile;

	if(arg[0].type == B1Types::B1T_BYTE)
	{
		add_op(*_curr_code_sec, L"PUSH A", is_volatile); //88
		_stack_ptr++;
	}
	else
	{
		add_op(*_curr_code_sec, L"PUSHW X", is_volatile); //89
		_stack_ptr += 2;


		if(arg[0].type == B1Types::B1T_LONG)
		{
			add_op(*_curr_code_sec, L"PUSHW Y", is_volatile); //90 89
			_stack_ptr += 2;
		}
	}

	if(!is_ma)
	{
		// allocate array of default size if necessary
		auto err = stm8_arr_alloc_def(var);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}
	}

	// calculate memory offset
	bool imm_offset = false;
	int32_t offset = 0;
	auto err = stm8_arr_offset(arg, imm_offset, offset);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	// store value
	if(arg[0].type == B1Types::B1T_BYTE)
	{
		add_op(*_curr_code_sec, L"POP A", is_volatile); //84
		_stack_ptr--;

		if(is_ma)
		{
			if(imm_offset)
			{
				add_op(*_curr_code_sec, L"LD (" + dst + L" + " + Utils::str_tohex16(offset) + L"), A", is_volatile); //B7 SHORT_ADDRESS C7 LONG_ADDRESS
			}
			else
			{
				add_op(*_curr_code_sec, L"LD (" + dst + L", X), A", is_volatile); //E7 BYTE_OFFSET D7 WORD_OFFSET
			}
		}
		else
		{
			if(imm_offset)
			{
				add_op(*_curr_code_sec, L"LDW X, (" + dst + L")", is_volatile); //BE BYTE_OFFSET CE WORD_OFFSET
				if(offset == 0)
				{
					add_op(*_curr_code_sec, L"LD (X), A", is_volatile); //F7
				}
				else
				{
					add_op(*_curr_code_sec, L"LD (" + Utils::str_tohex16(offset) + L", X), A", is_volatile); //E7 BYTE_OFFSET D7 WORD_OFFSET
				}
			}
			else
			{
				add_op(*_curr_code_sec, L"LD ([" + dst + L"], X), A", is_volatile); //92 D7 BYTE_OFFSET 72 D7 WORD_OFFSET
			}
		}
	}
	else
	if(arg[0].type == B1Types::B1T_INT || arg[0].type == B1Types::B1T_WORD || arg[0].type == B1Types::B1T_STRING)
	{
		if(imm_offset)
		{
			offset *= 2;
		}
		else
		{
			add_op(*_curr_code_sec, L"SLAW X", is_volatile); //58
		}

		if(is_ma)
		{
			if(imm_offset)
			{
				// release previous string
				if(arg[0].type == B1Types::B1T_STRING)
				{
					add_op(*_curr_code_sec, L"LDW X, (" + dst + L" + " + Utils::str_tohex16(offset) + L")", is_volatile); //BE BYTE_OFFSET CE WORD_OFFSET
					add_call_op(L"__LIB_STR_RLS", is_volatile);
				}

				add_op(*_curr_code_sec, L"POPW X", is_volatile); //85
				_stack_ptr -= 2;

				add_op(*_curr_code_sec, L"LDW (" + dst + L" + " + Utils::str_tohex16(offset) + L"), X", is_volatile); //BF SHORT_ADDRESS CF LONG_ADDRESS
			}
			else
			{
				// release previous string
				if(arg[0].type == B1Types::B1T_STRING)
				{
					add_op(*_curr_code_sec, L"PUSHW X", is_volatile); //89
					_stack_ptr += 2;
					add_op(*_curr_code_sec, L"LDW X, (" + dst + L", X)", is_volatile); //EE BYTE_OFFSET DE WORD_OFFSET
					add_call_op(L"__LIB_STR_RLS", is_volatile);
					add_op(*_curr_code_sec, L"POPW X", is_volatile); //85
					_stack_ptr -= 2;
				}

				add_op(*_curr_code_sec, L"POPW Y", is_volatile); //90 85
				_stack_ptr -= 2;

				add_op(*_curr_code_sec, L"LDW (" + dst + L", X), Y", is_volatile); //EF BYTE_OFFSET DF WORD_OFFSET
			}
		}
		else
		{
			if(imm_offset)
			{
				// release previous string
				if(arg[0].type == B1Types::B1T_STRING)
				{
					add_op(*_curr_code_sec, L"LDW X, (" + dst + L")", is_volatile); //BE BYTE_OFFSET CE WORD_OFFSET
					add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", X)", is_volatile); //EE BYTE_OFFSET DE WORD_OFFSET
					add_call_op(L"__LIB_STR_RLS", is_volatile);
				}

				add_op(*_curr_code_sec, L"POPW Y", is_volatile); //90 85
				_stack_ptr -= 2;

				add_op(*_curr_code_sec, L"LDW X, (" + dst + L")", is_volatile); //BE BYTE_OFFSET CE WORD_OFFSET
				if(offset == 0)
				{
					add_op(*_curr_code_sec, L"LDW (X), Y", is_volatile); //FF
				}
				else
				{
					add_op(*_curr_code_sec, L"LDW (" + Utils::str_tohex16(offset) + L", X), Y", is_volatile); //EF BYTE_OFFSET DF WORD_OFFSET
				}
			}
			else
			{
				// release previous string
				if(arg[0].type == B1Types::B1T_STRING)
				{
					add_op(*_curr_code_sec, L"PUSHW X", is_volatile); //89
					_stack_ptr += 2;
					add_op(*_curr_code_sec, L"LDW X, ([" + dst + L"], X)", is_volatile); //92 DE BYTE_OFFSET 72 DE WORD_OFFSET
					add_call_op(L"__LIB_STR_RLS", is_volatile);
					add_op(*_curr_code_sec, L"POPW X", is_volatile); //85
					_stack_ptr -= 2;
				}

				add_op(*_curr_code_sec, L"POPW Y", is_volatile); //90 85
				_stack_ptr -= 2;

				add_op(*_curr_code_sec, L"LDW ([" + dst + L"], X), Y", is_volatile); //92 DF BYTE_OFFSET 72 DF WORD_OFFSET
			}
		}
	}
	else
	{
		// LONG type
		if(imm_offset)
		{
			offset *= 4;
		}
		else
		{
			add_op(*_curr_code_sec, L"SLAW X", is_volatile); //58
			add_op(*_curr_code_sec, L"SLAW X", is_volatile); //58
		}

		if(is_ma)
		{
			if(imm_offset)
			{
				add_op(*_curr_code_sec, L"POPW X", is_volatile); //85
				_stack_ptr -= 2;
				add_op(*_curr_code_sec, L"LDW (" + dst + L" + " + Utils::str_tohex16(offset) + L"), X", is_volatile); //BF SHORT_ADDRESS CF LONG_ADDRESS
				add_op(*_curr_code_sec, L"POPW X", is_volatile); //85
				_stack_ptr -= 2;
				add_op(*_curr_code_sec, L"LDW (" + dst + L" + " + Utils::str_tohex16(offset) + L" + 2), X", is_volatile); //BF SHORT_ADDRESS CF LONG_ADDRESS
			}
			else
			{
				add_op(*_curr_code_sec, L"POPW Y", is_volatile); //90 85
				_stack_ptr -= 2;
				add_op(*_curr_code_sec, L"LDW (" + dst + L", X), Y", is_volatile); //EF BYTE_OFFSET DF WORD_OFFSET
				add_op(*_curr_code_sec, L"POPW Y", is_volatile); //90 85
				_stack_ptr -= 2;
				add_op(*_curr_code_sec, L"LDW (" + dst + L" + 2, X), Y", is_volatile); //EF BYTE_OFFSET DF WORD_OFFSET
			}
		}
		else
		{
			if(imm_offset)
			{
				add_op(*_curr_code_sec, L"LDW X, (" + dst + L")", is_volatile); //BE BYTE_OFFSET CE WORD_OFFSET
				add_op(*_curr_code_sec, L"POPW Y", is_volatile); //90 85
				_stack_ptr -= 2;
				if(offset == 0)
				{
					add_op(*_curr_code_sec, L"LDW (X), Y", is_volatile); //FF
				}
				else
				{
					add_op(*_curr_code_sec, L"LDW (" + Utils::str_tohex16(offset) + L", X), Y", is_volatile); //EF BYTE_OFFSET DF WORD_OFFSET
				}
				add_op(*_curr_code_sec, L"POPW Y", is_volatile); //90 85
				_stack_ptr -= 2;
				add_op(*_curr_code_sec, L"LDW (" + Utils::str_tohex16(offset) + L" + 2, X), Y", is_volatile); //EF BYTE_OFFSET DF WORD_OFFSET
			}
			else
			{
				add_op(*_curr_code_sec, L"ADDW X, (" + dst + L")", is_volatile); //72 BB WORD_OFFSET
				add_op(*_curr_code_sec, L"POPW Y", is_volatile); //90 85
				_stack_ptr -= 2;
				add_op(*_curr_code_sec, L"LDW (X), Y", is_volatile); // FF
				add_op(*_curr_code_sec, L"POPW Y", is_volatile); //90 85
				_stack_ptr -= 2;
				add_op(*_curr_code_sec, L"LDW(2, X), Y", is_volatile); // EF BYTE_OFFSET
			}
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::stm8_assign(const B1_CMP_CMD &cmd, bool omit_zero_init)
{
	if(cmd.cmd != L"=")
	{
		return C1_T_ERROR::C1_RES_EUNKINST;
	}

	bool can_omit_init;
	bool is_volatile;

	auto var = _mem_areas.find(cmd.args[1][0].value);
	if(var == _mem_areas.cend())
	{
		var = _vars.find(cmd.args[1][0].value);
		is_volatile = (var != _vars.cend()) && var->second.is_volatile;
		// allow omitting initialization of non-volatile variables
		can_omit_init = (var != _vars.cend()) && (!is_volatile);
	}
	else
	{
		is_volatile = var->second.is_volatile;
		// allow omitting initialization of non-volatile static variables only
		can_omit_init = var->second.use_symbol && (!is_volatile);
	}

	// omit initialization of scalar non-volatile variables in .CODE INIT section
	if(omit_zero_init && cmd.args[1].size() == 1 && (cmd.args[1][0].type == B1Types::B1T_BYTE || cmd.args[1][0].type == B1Types::B1T_INT || cmd.args[1][0].type == B1Types::B1T_WORD || cmd.args[1][0].type == B1Types::B1T_LONG))
	{
		std::wstring srcval;

		if(can_omit_init)
		{
			auto err = stm8_load(cmd.args[0], cmd.args[1][0].type, LVT::LVT_IMMVAL, nullptr, &srcval);
			if(err == C1_T_ERROR::C1_RES_OK && (srcval == L"0" || srcval == L"0x0" || srcval == L"0X0"))
			{
				return C1_T_ERROR::C1_RES_OK;
			}
		}
	}

	if(	(cmd.args[0][0].type == B1Types::B1T_BYTE || cmd.args[0][0].type == B1Types::B1T_INT || cmd.args[0][0].type == B1Types::B1T_WORD || cmd.args[0][0].type == B1Types::B1T_LONG) &&
		cmd.args[1][0].type == B1Types::B1T_BYTE
		)
	{
		// try to use MOV instead of two LDs
		LVT srctype = LVT::LVT_NONE;
		std::wstring srcval, dstval;

		auto err = stm8_load(cmd.args[0], B1Types::B1T_BYTE, LVT::LVT_IMMVAL | LVT::LVT_MEMREF, &srctype, &srcval);
		if(err == C1_T_ERROR::C1_RES_OK)
		{
			err = stm8_load(cmd.args[1], B1Types::B1T_BYTE, LVT::LVT_MEMREF, nullptr, &dstval);
			if(err == C1_T_ERROR::C1_RES_OK)
			{
				if(srctype == LVT::LVT_IMMVAL)
				{
					if(srcval == L"0" || srcval == L"0x0" || srcval == L"0X0")
					{
						add_op(*_curr_code_sec, L"CLR (" + dstval + L")", is_volatile); //3F SHORT_ADDRESS 72 5F LONG_ADDRESS
					}
					else
					{
						add_op(*_curr_code_sec, L"MOV (" + dstval + L"), " + srcval, is_volatile); //35 BYTE_VALUE LONG_ADDRESS
					}
				}
				else
				{
					add_op(*_curr_code_sec, L"MOV (" + dstval + L"), (" + srcval + L")", is_volatile); //45 SHORT_ADDRESS SHORT_ADDRESS 55 LONG_ADDRESS LONG_ADDRESS
				}

				return C1_T_ERROR::C1_RES_OK;
			}
		}
	}

	auto err = stm8_load(cmd.args[0], cmd.args[1][0].type, LVT::LVT_REG);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	err = stm8_store(cmd.args[1]);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::stm8_un_op(const B1_CMP_CMD &cmd, bool omit_zero_init)
{
	if(cmd.cmd == L"=")
	{
		return stm8_assign(cmd, omit_zero_init);
	}

	auto err = stm8_load(cmd.args[0], cmd.args[1][0].type, LVT::LVT_REG);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	if(cmd.cmd == L"-")
	{
		if(cmd.args[1][0].type == B1Types::B1T_BYTE)
		{
			add_op(*_curr_code_sec, L"NEG A", false); //40
		}
		else
		if(cmd.args[1][0].type == B1Types::B1T_INT || cmd.args[1][0].type == B1Types::B1T_WORD)
		{
			add_op(*_curr_code_sec, L"NEGW X", false); //50
		}
		else
		if(cmd.args[1][0].type == B1Types::B1T_LONG)
		{
			add_call_op(L"__LIB_AUX_NEG32");
		}
		else
		{
			return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
		}
	}
	else
	// bitwise NOT
	if(cmd.cmd == L"!")
	{
		if(cmd.args[1][0].type == B1Types::B1T_BYTE)
		{
			add_op(*_curr_code_sec, L"CPL A", false); //43
		}
		else
		if(cmd.args[1][0].type == B1Types::B1T_INT || cmd.args[1][0].type == B1Types::B1T_WORD)
		{
			add_op(*_curr_code_sec, L"CPLW X", false); //53
		}
		else
		if(cmd.args[1][0].type == B1Types::B1T_LONG)
		{
			add_op(*_curr_code_sec, L"CPLW Y", false); //90 53
			add_op(*_curr_code_sec, L"CPLW X", false); //53
		}
		else
		{
			return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
		}
	}

	err = stm8_store(cmd.args[1]);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	return C1_T_ERROR::C1_RES_OK;
}

// additive operations
C1_T_ERROR C1STM8Compiler::stm8_add_op(const B1_CMP_CMD &cmd)
{
	B1Types com_type = B1Types::B1T_UNKNOWN;
	std::wstring inst, val;
	LVT lvt;
	bool comp = false;
	bool imm_val = false;
	bool mem_ref = false;
	bool stk_ref = false;
	bool is_volatile = false;

	B1_CMP_ARG arg1 = cmd.args[0];
	B1_CMP_ARG arg2 = cmd.args[1];

	auto err = B1CUtils::get_com_type(arg1[0].type, arg2[0].type, com_type, comp);
	if(err != B1_RES_OK)
	{
		return static_cast<C1_T_ERROR>(err);
	}

	if(	arg1[0].type != B1Types::B1T_STRING && arg2[0].type != B1Types::B1T_STRING &&
		(B1CUtils::is_num_val(arg1[0].value) || B1CUtils::is_num_val(arg2[0].value)))
	{
		comp = true;
	}

	if(cmd.cmd != L"+")
	{
		if(arg1[0].type == B1Types::B1T_STRING || arg2[0].type == B1Types::B1T_STRING)
		{
			return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
		}
	}

	if(cmd.cmd == L"+")
	{
		inst = L"ADD";
	}
	else
	if(cmd.cmd == L"-")
	{
		inst = L"SUB";
	}
	else
	{
		return C1_T_ERROR::C1_RES_EUNKINST;
	}

	if(com_type == B1Types::B1T_INT || com_type == B1Types::B1T_WORD)
	{
		inst += L"W";
	}

	lvt = comp ? (LVT::LVT_REG | LVT::LVT_IMMVAL | LVT::LVT_MEMREF | LVT::LVT_STKREF) : (LVT::LVT_REG | LVT::LVT_IMMVAL);
	auto err1 = stm8_load(arg2, com_type, lvt, &lvt, &val, &is_volatile);
	if(err1 != C1_T_ERROR::C1_RES_OK)
	{
		return err1;
	}

	if(lvt == LVT::LVT_IMMVAL)
	{
		imm_val = true;
	}
	else
	if(lvt == LVT::LVT_MEMREF)
	{
		mem_ref = true;
	}
	else
	if(lvt == LVT::LVT_STKREF)
	{
		stk_ref = true;
	}
	else
	if(lvt == LVT::LVT_REG)
	{
		if(com_type == B1Types::B1T_BYTE)
		{
			add_op(*_curr_code_sec, L"PUSH A", false); //88
			_stack_ptr++;
		}
		else
		{
			add_op(*_curr_code_sec, L"PUSHW X", false); //89
			_stack_ptr += 2;

			if(com_type == B1Types::B1T_LONG)
			{
				add_op(*_curr_code_sec, L"PUSHW Y", false); // 90 89
				_stack_ptr += 2;
			}
		}
	}

	err1 = stm8_load(arg1, com_type, LVT::LVT_REG);
	if(err1 != C1_T_ERROR::C1_RES_OK)
	{
		return err1;
	}

	if(com_type == B1Types::B1T_STRING)
	{
		add_call_op(L"__LIB_STR_APD");
		add_op(*_curr_code_sec, L"ADDW SP, 2", false); //5B BYTE_VALUE
		_stack_ptr -= 2;
	}
	else
	if(com_type == B1Types::B1T_BYTE)
	{
		if(imm_val)
		{
			add_op(*_curr_code_sec, inst + L" A, " + val, false); //AB/A0 BYTE_VALUE
		}
		else
		if(mem_ref)
		{
			add_op(*_curr_code_sec, inst + L" A, (" + val + L")", is_volatile); //BB/B0 BYTE_OFFSET CB/C0 WORD_OFFSET
		}
		else
		if(stk_ref)
		{
			add_op(*_curr_code_sec, inst + L" A, (" + val + L", SP)", false); //1B/10 BYTE_OFFSET
		}
		else
		{
			add_op(*_curr_code_sec, inst + L" A, (0x1, SP)", false); //1B/10 BYTE_OFFSET
			add_op(*_curr_code_sec, L"ADDW SP, 1", false); //5B BYTE_VALUE
			_stack_ptr--;
		}
	}
	else
	if(com_type == B1Types::B1T_INT || com_type == B1Types::B1T_WORD)
	{
		if(imm_val)
		{
			add_op(*_curr_code_sec, inst + L" X, " + val, false); //1C/1D WORD_VALUE
		}
		else
		if(mem_ref)
		{
			add_op(*_curr_code_sec, inst + L" X, (" + val + L")", is_volatile); //72 BB/72 B0 WORD_OFFSET
		}
		else
		if(stk_ref)
		{
			add_op(*_curr_code_sec, inst + L" X, (" + val + L", SP)", false); //72 FB/72 F0 BYTE_OFFSET
		}
		else
		{
			add_op(*_curr_code_sec, inst + L" X, (0x1, SP)", false); //72 FB/72 F0 BYTE_OFFSET
			add_op(*_curr_code_sec, L"ADDW SP, 2", false); //5B BYTE_VALUE
			_stack_ptr -= 2;
		}
	}
	else
	{
		// LONG type
		if(imm_val)
		{
			if(cmd.cmd == L"+")
			{
				add_op(*_curr_code_sec, L"ADDW X, " + val + L".l", false); //1C WORD_VALUE
				const auto label = emit_label(true);
				add_op(*_curr_code_sec, L"JRNC " + label, false); //24 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				add_op(*_curr_code_sec, L"INCW Y", false); //90 5C
				add_lbl(*_curr_code_sec, _curr_code_sec->cend(), label, false);
				_all_symbols.insert(label);
				add_op(*_curr_code_sec, L"ADDW Y, " + val + L".h", false); //72 A9 WORD_VALUE
			}
			else
			{
				add_op(*_curr_code_sec, L"SUBW X, " + val + L".l", false); //1D WORD_VALUE
				add_op(*_curr_code_sec, L"RRWA Y", false); //90 01
				add_op(*_curr_code_sec, L"SBC A, " + val + L".hl", false); //A2 BYTE_VALUE
				add_op(*_curr_code_sec, L"RRWA Y", false); //90 01
				add_op(*_curr_code_sec, L"SBC A, " + val + L".hh", false); //A2 BYTE_VALUE
				add_op(*_curr_code_sec, L"RRWA Y", false); //90 01
			}
		}
		else
		if(mem_ref)
		{
			if(cmd.cmd == L"+")
			{
				add_op(*_curr_code_sec, L"ADDW X, (" + val + L" + 2)", is_volatile); //72 BB WORD_VALUE
				const auto label = emit_label(true);
				add_op(*_curr_code_sec, L"JRNC " + label, is_volatile); //24 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				add_op(*_curr_code_sec, L"INCW Y", is_volatile); //90 5C
				add_lbl(*_curr_code_sec, _curr_code_sec->cend(), label, is_volatile);
				_all_symbols.insert(label);
				add_op(*_curr_code_sec, L"ADDW Y, (" + val + L")", is_volatile); //72 B9 WORD_VALUE
			}
			else
			{
				add_op(*_curr_code_sec, L"SUBW X, (" + val + L" + 2)", is_volatile); //72 B0 WORD_VALUE
				add_op(*_curr_code_sec, L"RRWA Y", is_volatile); //90 01
				add_op(*_curr_code_sec, L"SBC A, (" + val + L" + 1)", is_volatile); //B2 BYTE_OFFSET C2 WORD_OFFSET
				add_op(*_curr_code_sec, L"RRWA Y", is_volatile); //90 01
				add_op(*_curr_code_sec, L"SBC A, (" + val + L")", is_volatile); //B2 BYTE_OFFSET C2 WORD_OFFSET
				add_op(*_curr_code_sec, L"RRWA Y", is_volatile); //90 01
			}
		}
		else
		if(stk_ref)
		{
			if(cmd.cmd == L"+")
			{
				add_op(*_curr_code_sec, L"ADDW X, (" + val + L" + 2, SP)", false); //72 FB BYTE_OFFSET
				const auto label = emit_label(true);
				add_op(*_curr_code_sec, L"JRNC " + label, false); //24 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				add_op(*_curr_code_sec, L"INCW Y", false); //90 5C
				add_lbl(*_curr_code_sec, _curr_code_sec->cend(), label, false);
				_all_symbols.insert(label);
				add_op(*_curr_code_sec, L"ADDW Y, (" + val + L", SP)", false); //72 F9 WORD_VALUE
			}
			else
			{
				add_op(*_curr_code_sec, L"SUBW X, (" + val + L" + 2, SP)", false); //72 F0 WORD_VALUE
				add_op(*_curr_code_sec, L"RRWA Y", false); //90 01
				add_op(*_curr_code_sec, L"SBC A, (" + val + L" + 1, SP)", false); //12 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RRWA Y", false); //90 01
				add_op(*_curr_code_sec, L"SBC A, (" + val + L", SP)", false); //12 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RRWA Y", false); //90 01
			}
		}
		else
		{
			if(cmd.cmd == L"+")
			{
				add_op(*_curr_code_sec, L"ADDW X, (0x3, SP)", false); //72 FB BYTE_OFFSET
				const auto label = emit_label(true);
				add_op(*_curr_code_sec, L"JRNC " + label, false); //24 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				add_op(*_curr_code_sec, L"INCW Y", false); //90 5C
				add_lbl(*_curr_code_sec, _curr_code_sec->cend(), label, false);
				_all_symbols.insert(label);
				add_op(*_curr_code_sec, L"ADDW Y, (0x1, SP)", false); //72 F9 WORD_VALUE
			}
			else
			{
				add_op(*_curr_code_sec, L"SUBW X, (0x3, SP)", false); //72 F0 WORD_VALUE
				add_op(*_curr_code_sec, L"RRWA Y", false); //90 01
				add_op(*_curr_code_sec, L"SBC A, (0x2, SP)", false); //12 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RRWA Y", false); //90 01
				add_op(*_curr_code_sec, L"SBC A, (0x1, SP)", false); //12 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RRWA Y", false); //90 01
			}

			add_op(*_curr_code_sec, L"ADDW SP, 4", false); //5B BYTE_VALUE
			_stack_ptr -= 4;
		}
	}

	err1 = stm8_arrange_types(com_type, cmd.args[2][0].type);
	if(err1 != C1_T_ERROR::C1_RES_OK)
	{
		return err1;
	}

	err1 = stm8_store(cmd.args[2]);
	if(err1 != C1_T_ERROR::C1_RES_OK)
	{
		return err1;
	}

	return C1_T_ERROR::C1_RES_OK;
}

// multiplicative operations (*, /, ^, MOD)
C1_T_ERROR C1STM8Compiler::stm8_mul_op(const B1_CMP_CMD &cmd)
{
	B1Types com_type;
	bool comp = false;

	B1_CMP_ARG arg1 = cmd.args[0];
	B1_CMP_ARG arg2 = cmd.args[1];

	if(arg1[0].type == B1Types::B1T_STRING || arg2[0].type == B1Types::B1T_STRING)
	{
		return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
	}

	if(cmd.cmd == L"^")
	{
		com_type = arg1[0].type;
		if(com_type == B1Types::B1T_BYTE)
		{
			com_type = B1Types::B1T_WORD;
		}
	}
	else
	{
		auto err = B1CUtils::get_com_type(arg1[0].type, arg2[0].type, com_type, comp);
		if(err != B1_RES_OK)
		{
			return static_cast<C1_T_ERROR>(err);
		}
	}

	if(com_type == B1Types::B1T_BYTE)
	{
		// two BYTE arguments and *, /, or MOD operator
		auto err = stm8_load(arg1, com_type, LVT::LVT_REG);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}

		if(cmd.cmd == L"/" || cmd.cmd == L"%")
		{
			add_op(*_curr_code_sec, L"CLRW X", false); //5F
		}

		add_op(*_curr_code_sec, L"LD XL, A", false); //97

		err = stm8_load(arg2, com_type, LVT::LVT_REG);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}

		if(cmd.cmd == L"*")
		{
			add_op(*_curr_code_sec, L"MUL X, A", false); //42
		}
		else
		{
			// div or mod
			add_op(*_curr_code_sec, L"DIV X, A", false); //62
		}

		if(cmd.cmd == L"*" || cmd.cmd == L"/")
		{
			add_op(*_curr_code_sec, L"LD A, XL", false); //9F
		}
	}
	else
	if(com_type == B1Types::B1T_WORD && (cmd.cmd == L"/" || cmd.cmd == L"%"))
	{
		// two WORD arguments or BYTE and WORD
		auto err = stm8_load(arg2, com_type, LVT::LVT_REG);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}

		add_op(*_curr_code_sec, L"PUSHW X", false); //89
		_stack_ptr += 2;

		err = stm8_load(arg1, com_type, LVT::LVT_REG);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}

		add_op(*_curr_code_sec, L"POPW Y", false); //90 85
		_stack_ptr -= 2;

		add_op(*_curr_code_sec, L"DIVW X, Y", false); //62

		if(cmd.cmd == L"%")
		{
			add_op(*_curr_code_sec, L"LDW X, Y", false); //93
		}
	}
	else
	{
		// here com_type cannot be BYTE
		// power operator: exponent is always INT, not depending on base type
		auto err = stm8_load(arg2, (com_type == B1Types::B1T_LONG && cmd.cmd == L"^") ? B1Types::B1T_INT : com_type, LVT::LVT_REG);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}
		add_op(*_curr_code_sec, L"PUSHW X", false); //89
		_stack_ptr += 2;
		if(com_type == B1Types::B1T_LONG && cmd.cmd != L"^")
		{
			add_op(*_curr_code_sec, L"PUSHW Y", false); // 90 89
			_stack_ptr += 2;
		}

		err = stm8_load(arg1, com_type, LVT::LVT_REG);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}

		auto fn_name =	(cmd.cmd == L"*") ? L"__LIB_COM_MUL" :
						(cmd.cmd == L"/") ? L"__LIB_COM_DIV" :
						(cmd.cmd == L"%") ? L"__LIB_COM_REM" :
						(cmd.cmd == L"^") ? L"__LIB_COM_POW" : std::wstring();

		if(fn_name.empty())
		{
			return C1_T_ERROR::C1_RES_EUNKINST;
		}

		fn_name += (com_type == B1Types::B1T_LONG) ? L"32" : L"16";

		add_call_op(fn_name);

		if(com_type == B1Types::B1T_LONG && cmd.cmd != L"^")
		{
			add_op(*_curr_code_sec, L"ADDW SP, 4", false); //5B BYTE_VALUE
			_stack_ptr -= 4;
		}
		else
		{
			add_op(*_curr_code_sec, L"ADDW SP, 2", false); //5B BYTE_VALUE
			_stack_ptr -= 2;
		}
	}

	auto err = stm8_arrange_types(com_type, cmd.args[2][0].type);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	err = stm8_store(cmd.args[2]);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	return C1_T_ERROR::C1_RES_OK;
}

// bitwise AND, OR and XOR operations
C1_T_ERROR C1STM8Compiler::stm8_bit_op(const B1_CMP_CMD &cmd)
{
	B1Types com_type = B1Types::B1T_UNKNOWN;
	std::wstring inst, val;
	LVT lvt;
	bool comp = false;
	bool imm_val = false;
	bool mem_ref = false;
	bool stk = false;
	bool is_volatile = false;

	B1_CMP_ARG arg1 = cmd.args[0];
	B1_CMP_ARG arg2 = cmd.args[1];

	if(arg1[0].type == B1Types::B1T_STRING || arg2[0].type == B1Types::B1T_STRING)
	{
		return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
	}

	auto err = B1CUtils::get_com_type(arg1[0].type, arg2[0].type, com_type, comp);
	if(err != B1_RES_OK)
	{
		return static_cast<C1_T_ERROR>(err);
	}

	if(cmd.cmd == L"&")
	{
		inst = L"AND";
	}
	else
	if(cmd.cmd == L"|")
	{
		inst = L"OR";
	}
	else
	if(cmd.cmd == L"~")
	{
		inst = L"XOR";
	}
	else
	{
		return C1_T_ERROR::C1_RES_EUNKINST;
	}

	lvt = comp ? (LVT::LVT_REG | LVT::LVT_IMMVAL | LVT::LVT_MEMREF | LVT::LVT_STKREF) : (LVT::LVT_REG | LVT::LVT_IMMVAL);
	auto err1 = stm8_load(arg2, com_type, lvt, &lvt, &val, &is_volatile);
	if(err1 != C1_T_ERROR::C1_RES_OK)
	{
		return err1;
	}

	if(lvt == LVT::LVT_IMMVAL)
	{
		imm_val = true;
	}
	else
	if(lvt == LVT::LVT_MEMREF)
	{
		mem_ref = true;
	}
	else
	if(lvt == LVT::LVT_STKREF)
	{
		stk = true;
	}
	else
	if(lvt == LVT::LVT_REG)
	{
		if(com_type == B1Types::B1T_BYTE)
		{
			add_op(*_curr_code_sec, L"PUSH A", false); //88
			_stack_ptr++;
		}
		else
		{
			add_op(*_curr_code_sec, L"PUSHW X", false); //89
			_stack_ptr += 2;
			if(com_type == B1Types::B1T_LONG)
			{
				add_op(*_curr_code_sec, L"PUSHW Y", false); //90 89
				_stack_ptr += 2;
			}
		}
	}

	err1 = stm8_load(arg1, com_type, LVT::LVT_REG);
	if(err1 != C1_T_ERROR::C1_RES_OK)
	{
		return err1;
	}

	if(com_type == B1Types::B1T_BYTE)
	{
		if(imm_val)
		{
			add_op(*_curr_code_sec, inst + L" A, " + val, false); //A4/AA/A8 BYTE_VALUE
		}
		else
		if(mem_ref)
		{
			add_op(*_curr_code_sec, inst + L" A, (" + val + L")", is_volatile); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
		}
		else
		if(stk)
		{
			add_op(*_curr_code_sec,  inst + L" A, (" + val + L", SP)", false); //14/1A/18 BYTE_OFFSET
		}
		else
		{
			add_op(*_curr_code_sec, inst + L" A, (1, SP)", false); //1B/10/14/1A/18 BYTE_OFFSET
			add_op(*_curr_code_sec, L"ADDW SP, 1", false); //5B BYTE_VALUE
			_stack_ptr--;
		}
	}
	else
	{
		if(imm_val)
		{
			add_op(*_curr_code_sec, L"RLWA X", false); //02
			add_op(*_curr_code_sec, inst + L" A, " + val + L".lh", false); //A4/AA/A8 BYTE_VALUE
			add_op(*_curr_code_sec, L"RLWA X", false); //02
			add_op(*_curr_code_sec, inst + L" A, " + val + L".ll", false); //A4/AA/A8 BYTE_VALUE
			add_op(*_curr_code_sec, L"RLWA X", false); //02
			if(com_type == B1Types::B1T_LONG)
			{
				add_op(*_curr_code_sec, L"RLWA Y", false); //90 02
				add_op(*_curr_code_sec, inst + L" A, " + val + L".hh", false); //A4/AA/A8 BYTE_VALUE
				add_op(*_curr_code_sec, L"RLWA Y", false); //90 02
				add_op(*_curr_code_sec, inst + L" A, " + val + L".hl", false); //A4/AA/A8 BYTE_VALUE
				add_op(*_curr_code_sec, L"RLWA Y", false); //90 02
			}
		}
		else
		if(mem_ref)
		{
			if(com_type == B1Types::B1T_INT || com_type == B1Types::B1T_WORD)
			{
				add_op(*_curr_code_sec, L"RLWA X", is_volatile); //02
				add_op(*_curr_code_sec, inst + L" A, (" + val + L")", is_volatile); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
				add_op(*_curr_code_sec, L"RLWA X", is_volatile); //02
				add_op(*_curr_code_sec, inst + L" A, (" + val + L" + 1)", is_volatile); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
				add_op(*_curr_code_sec, L"RLWA X", is_volatile); //02
			}
			else
			{
				// LONG type
				add_op(*_curr_code_sec, L"RLWA X", is_volatile); //02
				add_op(*_curr_code_sec, inst + L" A, (" + val + L" + 2)", is_volatile); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
				add_op(*_curr_code_sec, L"RLWA X", is_volatile); //02
				add_op(*_curr_code_sec, inst + L" A, (" + val + L" + 3)", is_volatile); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
				add_op(*_curr_code_sec, L"RLWA X", is_volatile); //02
				add_op(*_curr_code_sec, L"RLWA Y", is_volatile); //90 02
				add_op(*_curr_code_sec, inst + L" A, (" + val + L")", is_volatile); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
				add_op(*_curr_code_sec, L"RLWA Y", is_volatile); //90 02
				add_op(*_curr_code_sec, inst + L" A, (" + val + L" + 1)", is_volatile); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
				add_op(*_curr_code_sec, L"RLWA Y", is_volatile); //90 02
			}
		}
		else
		if(stk)
		{
			if(com_type == B1Types::B1T_INT || com_type == B1Types::B1T_WORD)
			{
				add_op(*_curr_code_sec, L"RLWA X", false); //02
				add_op(*_curr_code_sec, inst + L" A, (" + val + L", SP)", false); //14/1A/18 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RLWA X", false); //02
				add_op(*_curr_code_sec, inst + L" A, (" + val + L" + 1, SP)", false); //14/1A/18 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RLWA X", false); //02
			}
			else
			{
				// LONG type
				add_op(*_curr_code_sec, L"RLWA X", false); //02
				add_op(*_curr_code_sec, inst + L" A, (" + val + L" + 2, SP)", false); //14/1A/18 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RLWA X", false); //02
				add_op(*_curr_code_sec, inst + L" A, (" + val + L" + 3, SP)", false); //14/1A/18 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RLWA X", false); //02
				add_op(*_curr_code_sec, L"RLWA Y", false); //90 02
				add_op(*_curr_code_sec, inst + L" A, (" + val + L", SP)", false); //14/1A/18 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RLWA Y", false); //90 02
				add_op(*_curr_code_sec, inst + L" A, (" + val + L" + 1, SP)", false); //14/1A/18 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RLWA Y", false); //90 02
			}
		}
		else
		{
			if(com_type == B1Types::B1T_INT || com_type == B1Types::B1T_WORD)
			{
				add_op(*_curr_code_sec, L"RLWA X", false); //02
				add_op(*_curr_code_sec, inst + L" A, (1, SP)", false); //14/1A/18 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RLWA X", false); //02
				add_op(*_curr_code_sec, inst + L" A, (2, SP)", false); //14/1A/18 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RLWA X", false); //02
				add_op(*_curr_code_sec, L"ADDW SP, 2", false); //5B BYTE_VALUE
				_stack_ptr -= 2;
			}
			else
			{
				add_op(*_curr_code_sec, L"RLWA X", false); //02
				add_op(*_curr_code_sec, inst + L" A, (3, SP)", false); //14/1A/18 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RLWA X", false); //02
				add_op(*_curr_code_sec, inst + L" A, (4, SP)", false); //14/1A/18 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RLWA X", false); //02
				add_op(*_curr_code_sec, L"RLWA Y", false); //90 02
				add_op(*_curr_code_sec, inst + L" A, (1, SP)", false); //14/1A/18 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RLWA Y", false); //90 02
				add_op(*_curr_code_sec, inst + L" A, (2, SP)", false); //14/1A/18 BYTE_OFFSET
				add_op(*_curr_code_sec, L"RLWA Y", false); //90 02
				add_op(*_curr_code_sec, L"ADDW SP, 4", false); //5B BYTE_VALUE
				_stack_ptr -= 4;
			}
		}
	}

	err1 = stm8_arrange_types(com_type, cmd.args[2][0].type);
	if(err1 != C1_T_ERROR::C1_RES_OK)
	{
		return err1;
	}

	err1 = stm8_store(cmd.args[2]);
	if(err1 != C1_T_ERROR::C1_RES_OK)
	{
		return err1;
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::stm8_add_shift_op(const std::wstring &shift_cmd, const B1Types type)
{
	if(type == B1Types::B1T_BYTE)
	{
		if(shift_cmd == L"<<")
		{
			add_op(*_curr_code_sec, L"SLL A", false); //48
		}
		else
		{
			add_op(*_curr_code_sec, L"SRL A", false); //44
		}
	}
	else
	if(type == B1Types::B1T_INT)
	{
		if(shift_cmd == L"<<")
		{
			add_op(*_curr_code_sec, L"SLAW X", false); //58
		}
		else
		{
			add_op(*_curr_code_sec, L"SRAW X", false); //57
		}
	}
	else
	if(type == B1Types::B1T_WORD)
	{
		if(shift_cmd == L"<<")
		{
			add_op(*_curr_code_sec, L"SLLW X", false); //58
		}
		else
		{
			add_op(*_curr_code_sec, L"SRLW X", false); //54
		}
	}
	else
	{
		// LONG type
		if(shift_cmd == L"<<")
		{
			add_op(*_curr_code_sec, L"SLLW X", false); //58
			add_op(*_curr_code_sec, L"RLCW Y", false); //90 59
		}
		else
		{
			add_op(*_curr_code_sec, L"SRAW Y", false); //90 57
			add_op(*_curr_code_sec, L"RRCW X", false); //56
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

// shift operations
C1_T_ERROR C1STM8Compiler::stm8_shift_op(const B1_CMP_CMD &cmd)
{
	B1_CMP_ARG arg1 = cmd.args[0];
	B1_CMP_ARG arg2 = cmd.args[1];

	if(arg1[0].type == B1Types::B1T_STRING || arg2[0].type == B1Types::B1T_STRING)
	{
		return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
	}

	auto err = stm8_load(arg1, arg1[0].type, LVT::LVT_REG);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	int32_t n = 0;
	bool use_loop = true;
	bool imm_arg = false;

	if(arg2.size() == 1 && B1CUtils::is_num_val(arg2[0].value))
	{
		if(Utils::str2int32(arg2[0].value, n) == B1_RES_OK)
		{
			imm_arg = true;
			if(n >= 0 && n <= ((arg1[0].type == B1Types::B1T_LONG) ? 2 : 4))
			{
				use_loop = false;
			}
		}
	}

	if(use_loop)
	{
		if(imm_arg)
		{
			if(n != 0)
			{
				LVT lvt = LVT::LVT_NONE;
				std::wstring res_val;

				err = stm8_load(arg2, B1Types::B1T_BYTE, LVT::LVT_IMMVAL, &lvt, &res_val);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				if(arg1[0].type == B1Types::B1T_BYTE)
				{
					add_op(*_curr_code_sec, L"LDW X, " + res_val + L".ll", false); //AE WORD_VALUE
				}
				else
				{
					add_op(*_curr_code_sec, L"LD A, " + res_val, false); //A6 BYTE_VALUE
				}

				const auto loop_label = emit_label(true);
				add_lbl(*_curr_code_sec, _curr_code_sec->cend(), loop_label, false);
				_all_symbols.insert(loop_label);

				stm8_add_shift_op(cmd.cmd, arg1[0].type);

				if(arg1[0].type == B1Types::B1T_BYTE)
				{
					add_op(*_curr_code_sec, L"DECW X", false); //5A
				}
				else
				{
					add_op(*_curr_code_sec, L"DEC A", false); //4A
				}
				add_op(*_curr_code_sec, L"JRNE " + loop_label, false); //26 SIGNED_BYTE_OFFSET
				_req_symbols.insert(loop_label);
			}
		}
		else
		{
			const auto loop_label = emit_label(true);
			const auto loop_end_label = emit_label(true);

			if(arg1[0].type == B1Types::B1T_BYTE)
			{
				add_op(*_curr_code_sec, L"PUSH A", false); //88
				_stack_ptr++;

				err = stm8_load(arg2, B1Types::B1T_INT, LVT::LVT_REG);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				add_op(*_curr_code_sec, L"POP A", false); //84
				_stack_ptr--;

				add_op(*_curr_code_sec, L"TNZW X", false); //5D
				add_op(*_curr_code_sec, L"JREQ " + loop_end_label, false); //27 SIGNED_BYTE_OFFSET
				_req_symbols.insert(loop_end_label);

			}
			else
			{
				add_op(*_curr_code_sec, L"PUSHW X", false); //89
				_stack_ptr += 2;
				if(arg1[0].type == B1Types::B1T_LONG)
				{
					add_op(*_curr_code_sec, L"PUSHW Y", false); //90 89
					_stack_ptr += 2;
				}

				err = stm8_load(arg2, B1Types::B1T_BYTE, LVT::LVT_REG);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				if(arg1[0].type == B1Types::B1T_LONG)
				{
					add_op(*_curr_code_sec, L"POPW Y", false); //90 85
					_stack_ptr -= 2;
				}
				add_op(*_curr_code_sec, L"POPW X", false); //85
				_stack_ptr -= 2;

				add_op(*_curr_code_sec, L"TNZ A", false); //4D
				add_op(*_curr_code_sec, L"JREQ " + loop_end_label, false); //27 SIGNED_BYTE_OFFSET
				_req_symbols.insert(loop_end_label);
			}

			add_lbl(*_curr_code_sec, _curr_code_sec->cend(), loop_label, false);
			_all_symbols.insert(loop_label);

			stm8_add_shift_op(cmd.cmd, arg1[0].type);

			if(arg1[0].type == B1Types::B1T_BYTE)
			{
				add_op(*_curr_code_sec, L"DECW X", false); //5A
			}
			else
			{
				add_op(*_curr_code_sec, L"DEC A", false); //4A
			}

			add_op(*_curr_code_sec, L"JRNE " + loop_label, false); //26 SIGNED_BYTE_OFFSET
			_req_symbols.insert(loop_label);
			add_lbl(*_curr_code_sec, _curr_code_sec->cend(), loop_end_label, false);
			_all_symbols.insert(loop_end_label);
		}
	}
	else
	{
		for(; n > 0; n--)
		{
			stm8_add_shift_op(cmd.cmd, arg1[0].type);
		}
	}

	err = stm8_arrange_types(arg1[0].type, cmd.args[2][0].type);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	err = stm8_store(cmd.args[2]);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	return C1_T_ERROR::C1_RES_OK;
}

// numeric comparison operations
C1_T_ERROR C1STM8Compiler::stm8_num_cmp_op(const B1_CMP_CMD &cmd)
{
	B1Types com_type = B1Types::B1T_UNKNOWN;
	std::wstring val;
	LVT lvt;
	bool comp = false;
	bool imm_val = false;
	bool mem_ref = false;
	bool stk_ref = false;
	bool is_volatile = false;

	auto err = B1CUtils::get_com_type(cmd.args[0][0].type, cmd.args[1][0].type, com_type, comp);
	if(err != B1_RES_OK)
	{
		return static_cast<C1_T_ERROR>(err);
	}

	B1_CMP_ARG arg1;
	B1_CMP_ARG arg2;

	if(com_type == B1Types::B1T_LONG && (cmd.cmd == L">" || cmd.cmd == L"<="))
	{
		// change order and use other comparison operators
		arg1 = cmd.args[1];
		arg2 = cmd.args[0];
		_cmp_op = (cmd.cmd == L">") ? L"<" : L">=";
	}
	else
	{
		arg1 = cmd.args[0];
		arg2 = cmd.args[1];
		_cmp_op = cmd.cmd;
	}

	if(arg1[0].type == B1Types::B1T_STRING || arg2[0].type == B1Types::B1T_STRING)
	{
		return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
	}

	lvt = comp ? (LVT::LVT_REG | LVT::LVT_IMMVAL | LVT::LVT_MEMREF | LVT::LVT_STKREF) : (LVT::LVT_REG | LVT::LVT_IMMVAL);
	auto err1 = stm8_load(arg2, com_type, lvt, &lvt, &val, &is_volatile);
	if(err1 != C1_T_ERROR::C1_RES_OK)
	{
		return err1;
	}

	if(lvt == LVT::LVT_IMMVAL)
	{
		imm_val = true;
	}
	else
	if(lvt == LVT::LVT_MEMREF)
	{
		mem_ref = true;
	}
	else
	if(lvt == LVT::LVT_STKREF)
	{
		stk_ref = true;
	}
	else
	if(lvt == LVT::LVT_REG)
	{
		if(com_type == B1Types::B1T_BYTE)
		{
			add_op(*_curr_code_sec, L"PUSH A", false); //88
			_stack_ptr++;
		}
		else
		{
			add_op(*_curr_code_sec, L"PUSHW X", false); //89
			_stack_ptr += 2;
			if(com_type == B1Types::B1T_LONG)
			{
				add_op(*_curr_code_sec, L"PUSHW Y", false); //90 89
				_stack_ptr += 2;
			}
		}
	}

	err1 = stm8_load(arg1, com_type, LVT::LVT_REG);
	if(err1 != C1_T_ERROR::C1_RES_OK)
	{
		return err1;
	}

	if(com_type == B1Types::B1T_BYTE)
	{
		if(imm_val)
		{
			if((cmd.cmd == L"==" || cmd.cmd == L"<>") && (val == L"0" || val == L"0x0" || val == L"0X0"))
			{
				add_op(*_curr_code_sec, L"TNZ A", false); //4D
			}
			else
			{
				add_op(*_curr_code_sec, L"CP A, " + val, false); //A1 BYTE_VALUE
			}
		}
		else
		if(mem_ref)
		{
			add_op(*_curr_code_sec, L"CP A, (" + val + L")", is_volatile); //B1 BYTE_OFFSET C1 WORD_OFFSET
		}
		else
		if(stk_ref)
		{
			add_op(*_curr_code_sec, L"CP A, (" + val + L", SP)", false); //11 BYTE_OFFSET
		}
		else
		{
			add_op(*_curr_code_sec, L"CP A, (1, SP)", false); //11 BYTE_OFFSET
			add_op(*_curr_code_sec, L"POP A", false); //84
			_stack_ptr--;
		}
	}
	else
	if(com_type == B1Types::B1T_INT || com_type == B1Types::B1T_WORD)
	{
		if(imm_val)
		{
			if((cmd.cmd == L"==" || cmd.cmd == L"<>") && (val == L"0" || val == L"0x0" || val == L"0X0"))
			{
				add_op(*_curr_code_sec, L"TNZW X", false); //5D
			}
			else
			{
				add_op(*_curr_code_sec, L"CPW X, " + val, false); //A3 WORD_VALUE
			}
		}
		else
		if(mem_ref)
		{
			add_op(*_curr_code_sec, L"CPW X, (" + val + L")", is_volatile); //B3 BYTE_OFFSET C3 WORD_OFFSET
		}
		else
		if(stk_ref)
		{
			add_op(*_curr_code_sec, L"CPW X, (" + val + L", SP)", false); //13 BYTE_OFFSET
		}
		else
		{
			add_op(*_curr_code_sec, L"CPW X, (1, SP)", false); //13 BYTE_OFFSET
			add_op(*_curr_code_sec, L"POPW X", false); //85
			_stack_ptr -= 2;
		}
	}
	else
	{
		// LONG type
		if(cmd.cmd == L"==" || cmd.cmd == L"<>")
		{
			bool clr_stk = false;
			const auto label = emit_label(true);

			if(imm_val)
			{
				if(val == L"0" || val == L"0x0" || val == L"0X0")
				{
					add_op(*_curr_code_sec, L"TNZW X", false); //5D
					add_op(*_curr_code_sec, L"JRNE " + label, false); //26 SIGNED_BYTE_OFFSET
					_req_symbols.insert(label);
					add_op(*_curr_code_sec, L"TNZW Y", false); //90 5D
				}
				else
				{
					add_op(*_curr_code_sec, L"CPW X, " + val + L".l", false); //A3 WORD_VALUE
					add_op(*_curr_code_sec, L"JRNE " + label, false); //26 SIGNED_BYTE_OFFSET
					_req_symbols.insert(label);
					add_op(*_curr_code_sec, L"CPW Y, " + val + L".h", false); //90 A3 WORD_VALUE
				}
			}
			else
			if(mem_ref)
			{
				add_op(*_curr_code_sec, L"CPW X, (" + val + L" + 2)", is_volatile); //B3 BYTE_OFFSET C3 WORD_OFFSET
				add_op(*_curr_code_sec, L"JRNE " + label, is_volatile); //26 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				add_op(*_curr_code_sec, L"CPW Y, (" + val + L")", is_volatile); //90 B3 BYTE_OFFSET 90 C3 WORD_OFFSET
			}
			else
			if(stk_ref)
			{
				add_op(*_curr_code_sec, L"CPW X, (" + val + L" + 2, SP)", false); //13 BYTE_OFFSET
				add_op(*_curr_code_sec, L"JRNE " + label, false); //26 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				add_op(*_curr_code_sec, L"EXGW X, Y", false); //51
				add_op(*_curr_code_sec, L"CPW X, (" + val + L", SP)", false); //13 BYTE_OFFSET
			}
			else
			{
				add_op(*_curr_code_sec, L"CPW X, (" + val + L" + 3, SP)", false); //13 BYTE_OFFSET
				add_op(*_curr_code_sec, L"JRNE " + label, false); //26 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				add_op(*_curr_code_sec, L"EXGW X, Y", false); //51
				add_op(*_curr_code_sec, L"CPW X, (" + val + L" + 1, SP)", false); //13 BYTE_OFFSET

				clr_stk = true;
			}

			add_lbl(*_curr_code_sec, _curr_code_sec->cend(), label, is_volatile);
			_all_symbols.insert(label);

			if(clr_stk)
			{
				add_op(*_curr_code_sec, L"ADDW SP, 4", false); //5B BYTE_VALUE
				_stack_ptr -= 4;
			}
		}
		else
		{
			if(imm_val)
			{
				add_op(*_curr_code_sec, L"CPW X, " + val + L".l", false); //A3 WORD_VALUE
				add_op(*_curr_code_sec, L"LD A, YL", false); //90 9F
				add_op(*_curr_code_sec, L"SBC A, " + val + L".hl", false); //A2 BYTE_VALUE
				add_op(*_curr_code_sec, L"LD A, YH", false); //90 9E
				add_op(*_curr_code_sec, L"SBC A, " + val + L".hh", false); //A2 BYTE_VALUE
			}
			else
			if(mem_ref)
			{
				add_op(*_curr_code_sec, L"CPW X, (" + val + L" + 2)", is_volatile); //B3 BYTE_OFFSET C3 WORD_OFFSET
				add_op(*_curr_code_sec, L"LD A, YL", is_volatile); //90 9F
				add_op(*_curr_code_sec, L"SBC A, (" + val + L" + 1)", is_volatile); //B2 BYTE_OFFSET C2 WORD_OFFSET
				add_op(*_curr_code_sec, L"LD A, YH", is_volatile); //90 9E
				add_op(*_curr_code_sec, L"SBC A, (" + val + L")", is_volatile); //B2 BYTE_OFFSET C2 WORD_OFFSET
			}
			else
			if(stk_ref)
			{
				add_op(*_curr_code_sec, L"CPW X, (" + val + L" + 2, SP)", false); //13 BYTE_OFFSET
				add_op(*_curr_code_sec, L"LD A, YL", false); //90 9F
				add_op(*_curr_code_sec, L"SBC A, (" + val + L" + 1, SP)", false); //12 BYTE_OFFSET
				add_op(*_curr_code_sec, L"LD A, YH", false); //90 9E
				add_op(*_curr_code_sec, L"SBC A, (" + val + L", SP)", false); //12 BYTE_OFFSET
			}
			else
			{
				add_op(*_curr_code_sec, L"CPW X, (" + val + L" + 3, SP)", false); //13 BYTE_OFFSET
				add_op(*_curr_code_sec, L"LD A, YL", false); //90 9F
				add_op(*_curr_code_sec, L"SBC A, (" + val + L" + 2, SP)", false); //12 BYTE_OFFSET
				add_op(*_curr_code_sec, L"LD A, YH", false); //90 9E
				add_op(*_curr_code_sec, L"SBC A, (" + val + L" + 1, SP)", false); //12 BYTE_OFFSET
				add_op(*_curr_code_sec, L"ADDW SP, 4", false); //5B BYTE_VALUE
				_stack_ptr -= 4;
			}
		}
	}

	_cmp_active = true;
	_cmp_type = com_type;

	return C1_T_ERROR::C1_RES_OK;
}

// string comparison operations
C1_T_ERROR C1STM8Compiler::stm8_str_cmp_op(const B1_CMP_CMD &cmd)
{
	B1_CMP_ARG arg1 = cmd.args[0];
	B1_CMP_ARG arg2 = cmd.args[1];

	if(arg1[0].type != B1Types::B1T_STRING && arg2[0].type != B1Types::B1T_STRING)
	{
		return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
	}

	auto err = stm8_load(arg2, B1Types::B1T_STRING, LVT::LVT_REG);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	add_op(*_curr_code_sec, L"PUSHW X", false); //89
	_stack_ptr += 2;

	err = stm8_load(arg1, B1Types::B1T_STRING, LVT::LVT_REG);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	add_call_op(L"__LIB_STR_CMP");
	add_op(*_curr_code_sec, L"ADDW SP, 2", false); //5B BYTE_VALUE
	_stack_ptr -= 2;

	add_op(*_curr_code_sec, L"TNZ A", false); //4D

	_cmp_active = true;
	_cmp_op = cmd.cmd;
	_cmp_type = B1Types::B1T_STRING;

	return C1_T_ERROR::C1_RES_OK;
}

// stores address of the specified array element in X and data length in stack
C1_T_ERROR C1STM8Compiler::stm8_load_ptr(const B1_CMP_ARG &first, const B1_CMP_ARG &count)
{
	const auto it = _mem_areas.find(first[0].value);
	bool is_ma = it != _mem_areas.cend();
	const B1_CMP_VAR *var = nullptr;

	if(is_ma)
	{
		var = &(it->second);
	}
	else
	{
		var = &(_vars.find(first[0].value)->second);
	}

	if(var->dim_num != 1)
	{
		return static_cast<C1_T_ERROR>(B1_RES_EWRARGCNT);
	}
	if(var->type != B1Types::B1T_BYTE)
	{
		return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
	}

	LVT valtype = LVT::LVT_NONE;
	std::wstring val;
	bool is_volatile = false;
	auto err = stm8_load(count, B1Types::B1T_WORD, LVT::LVT_REG, &valtype, &val, &is_volatile);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	add_op(*_curr_code_sec, L"PUSHW X", is_volatile); //89
	_stack_ptr += 2;

	if(!is_ma)
	{
		// allocate array of default size if necessary
		auto err = stm8_arr_alloc_def(*var);
		if (err != static_cast<C1_T_ERROR>(B1_RES_OK))
		{
			return err;
		}

		_req_symbols.insert(first[0].value);
	}

	// calculate memory offset
	// request immediate offset value if LVT_MEMREF is the only option
	bool imm_offset = false;
	int32_t offset = 0;
	err = stm8_arr_offset(first, imm_offset, offset);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	const auto rv = is_ma ? (var->use_symbol ? var->symbol : std::to_wstring(var->address)) : first[0].value;

	if(is_ma)
	{
		if(imm_offset)
		{
			add_op(*_curr_code_sec, L"LDW X, " + rv + L" + " + Utils::str_tohex16(offset), false); //AE WORD_VALUE
		}
		else
		{
			add_op(*_curr_code_sec, L"ADDW X, " + rv, false); //1C WORD_VALUE
		}
	}
	else
	{
		if(imm_offset)
		{
			add_op(*_curr_code_sec, L"LDW X, (" + rv + L")", var->is_volatile); //BE SHORT_ADDRESS, CE LONG_ADDRESS
			add_op(*_curr_code_sec, L"ADDW X, " + Utils::str_tohex16(offset), false); //1C WORD_VALUE
		}
		else
		{
			add_op(*_curr_code_sec, L"ADDW X, (" + rv + L")", var->is_volatile); //72 BB WORD_OFFSET
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::stm8_write_ioctl_fn(const B1_CMP_ARG &arg)
{
	auto dev_name = _global_settings.GetIoDeviceName(arg[1].value.substr(1, arg[1].value.length() - 2));
	auto cmd_name = arg[2].value.substr(1, arg[2].value.length() - 2);
	Settings::IoCmd iocmd;
	if(!_global_settings.GetIoCmd(dev_name, cmd_name, iocmd))
	{
		return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
	}

	if(iocmd.call_type == Settings::IoCmd::IoCmdCallType::CT_CALL)
	{
		auto file_name = iocmd.file_name;
		if(file_name.empty())
		{
			file_name = L"__LIB_" + dev_name + L"_" + std::to_wstring(iocmd.id) + L"_CALL";
		}
		add_call_op(file_name);
	}
	else
	{
		return C1_T_ERROR::C1_RES_ENOTIMP;
	}

	return C1_T_ERROR::C1_RES_OK;
}

// on return cmd_it is set on the last processed cmd
C1_T_ERROR C1STM8Compiler::stm8_write_ioctl(std::list<B1_CMP_CMD>::iterator &cmd_it)
{
	std::wstring dev_name;
	std::wstring cmd_name;
	int32_t id = -1;
	auto data_type = B1Types::B1T_UNKNOWN;
	bool pre_cmd = false; // command(-s) with predefined value(-s)
	int32_t mask = 0;
	int32_t value = 0;
	std::wstring str_value;
	bool accepts_data = false;
	auto call_type = Settings::IoCmd::IoCmdCallType::CT_CALL;
	auto code_place = Settings::IoCmd::IoCmdCodePlacement::CP_CURR_POS;
	std::wstring file_name;
	int32_t ioctl_num = 1;
	LVT res_lvt = LVT::LVT_NONE;
	std::vector<int32_t> more_masks;
	std::vector<int32_t> more_values;

	while(true)
	{
		auto *cmd = &*cmd_it;
		if(cmd->cmd != L"IOCTL")
		{
			if(id < 0)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			cmd_it--;
			break;
		}

		auto dev_name1 = _global_settings.GetIoDeviceName(cmd->args[0][0].value.substr(1, cmd->args[0][0].value.length() - 2));
		if(id < 0)
		{
			dev_name = dev_name1;
		}
		else
		{
			if(dev_name != dev_name1)
			{
				cmd_it--;
				break;
			}
		}

		auto tmp_cmd_name = cmd->args[1][0].value.substr(1, cmd->args[1][0].value.length() - 2);
		Settings::IoCmd iocmd;
		if(!_global_settings.GetIoCmd(dev_name, tmp_cmd_name, iocmd))
		{
			return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
		}

		if(cmd_name.empty())
		{
			cmd_name = tmp_cmd_name;
		}

		if(id >= 0 && id != iocmd.id)
		{
			cmd_it--;
			break;
		}

		if(_out_src_lines)
		{
			_comment = Utils::str_trim(_src_lines[cmd_it->src_line_id]);
		}

		if(!iocmd.accepts_data)
		{
			id = iocmd.id;
			accepts_data = false;
			call_type = iocmd.call_type;
			file_name = iocmd.file_name;
			code_place = iocmd.code_place;
			break;
		}

		if(cmd_it->args.size() != 3)
		{
			return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
		}

		if(!iocmd.predef_only)
		{
			id = iocmd.id;
			accepts_data = true;
			data_type = iocmd.data_type;
			call_type = iocmd.call_type;
			code_place = iocmd.code_place;
			file_name = iocmd.file_name;

			if(iocmd.data_type == B1Types::B1T_LABEL || iocmd.data_type == B1Types::B1T_VARREF || iocmd.data_type == B1Types::B1T_TEXT)
			{
				str_value = cmd->args[2][0].value;
			}
			else
			{
				LVT req_lvt = LVT::LVT_REG;

				if(iocmd.call_type == Settings::IoCmd::IoCmdCallType::CT_INL)
				{
					if(iocmd.extra_data.find(L'I') != std::wstring::npos)
					{
						req_lvt |= LVT::LVT_IMMVAL;
					}
					if(iocmd.extra_data.find(L'M') != std::wstring::npos)
					{
						req_lvt |= LVT::LVT_MEMREF;
					}
					if (iocmd.extra_data.find(L'S') != std::wstring::npos)
					{
						req_lvt |= LVT::LVT_STKREF;
					}
				}

				auto err = stm8_load(cmd_it->args[2], iocmd.data_type, req_lvt, &res_lvt, &str_value);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}

			break;
		}

		auto val = iocmd.values.find(cmd->args[2][0].value.substr(1, cmd->args[2][0].value.length() - 2));
		if(val == iocmd.values.end())
		{
			return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
		}

		if(id < 0)
		{
			// the first cmd
			id = iocmd.id;

			// predefined values cannot be strings at the moment
			if(iocmd.data_type == B1Types::B1T_STRING)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
			}

			str_value = val->second;

			if(!iocmd.more_masks.empty())
			{
				std::vector<std::wstring> values;
				Utils::str_split(str_value, L"+", values);
				str_value = values[0];

				for(auto i = 0; i < iocmd.more_masks.size(); i++)
				{
					more_masks.push_back(iocmd.more_masks[i].second);
					value = 0;
					if(i + 1 < values.size())
					{
						auto err = Utils::str2int32(values[i + 1], value);
						if(err != B1_RES_OK)
						{
							return static_cast<C1_T_ERROR>(err);
						}
					}
					more_values.push_back(value);
				}
			}

			auto err = Utils::str2int32(str_value, value);
			if(err != B1_RES_OK)
			{
				return static_cast<C1_T_ERROR>(err);
			}

			pre_cmd = true;
			data_type = iocmd.data_type;
			mask = iocmd.mask;
			accepts_data = true;
			call_type = iocmd.call_type;
			code_place = iocmd.code_place;
			file_name = iocmd.file_name;

			// no mask
			if(mask == 0)
			{
				break;
			}
		}
		else
		{
			int32_t n;

			str_value = val->second;

			if(!iocmd.more_masks.empty())
			{
				if(iocmd.more_masks.size() != more_masks.size())
				{
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}

				std::vector<std::wstring> values;
				Utils::str_split(str_value, L"+", values);
				str_value = values[0];

				for(auto i = 0; i < iocmd.more_masks.size(); i++)
				{
					more_masks[i] |= iocmd.more_masks[i].second;

					n = 0;
					if(i + 1 < values.size())
					{
						auto err = Utils::str2int32(values[i + 1], n);
						if(err != B1_RES_OK)
						{
							return static_cast<C1_T_ERROR>(err);
						}
					}
					more_values[i] = (more_values[i] & ~iocmd.more_masks[i].second) | n;
				}
			}

			auto err = Utils::str2int32(str_value, n);
			if(err != B1_RES_OK)
			{
				return static_cast<C1_T_ERROR>(err);
			}

			mask |= iocmd.mask;
			value = (value & ~iocmd.mask) | n;
			ioctl_num++;
		}

		if(std::next(cmd_it) == cend())
		{
			break;
		}

		cmd_it++;
	}

	bool is_static = true;

	if(data_type == B1Types::B1T_VARREF)
	{
		auto v = _mem_areas.find(str_value);
		if(v == _mem_areas.cend())
		{
			v = _vars.find(str_value);
			if(v != _vars.cend())
			{
				is_static = (v->second.dim_num == 0) || v->second.is_const;
			}

			_req_symbols.insert(str_value);
		}
		else
		{
			if(!v->second.use_symbol)
			{
				str_value = std::to_wstring(v->second.address);
			}
		}
	}

	if(call_type == Settings::IoCmd::IoCmdCallType::CT_CALL)
	{
		if(file_name.empty())
		{
			file_name = L"__LIB_" + dev_name + L"_" + std::to_wstring(id) + L"_CALL";
		}

		if(pre_cmd)
		{
			if(data_type == B1Types::B1T_STRING)
			{
				// predefined values cannot be strings at the moment
				return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
			}

			if(data_type == B1Types::B1T_BYTE)
			{
				add_op(*_curr_code_sec, L"LD A, " + std::to_wstring(value), false); //A6 BYTE_VALUE
				if(mask != 0)
				{
					add_op(*_curr_code_sec, L"PUSH " + std::to_wstring(mask), false); //4B BYTE_VALUE
					_stack_ptr++;
				}
			}
			else
			if(data_type == B1Types::B1T_INT)
			{
				add_op(*_curr_code_sec, L"LDW X, " + std::to_wstring(value), false); //AE WORD_VALUE
			}
			else
			if(data_type == B1Types::B1T_WORD)
			{
				add_op(*_curr_code_sec, L"LDW X, " + std::to_wstring(value), false); //AE WORD_VALUE
			}
			else
			if(data_type == B1Types::B1T_LONG)
			{
				add_op(*_curr_code_sec, L"LDW X, " + std::to_wstring(value & 0xFFFF), false); //AE WORD_VALUE
				add_op(*_curr_code_sec, L"LDW Y, " + std::to_wstring((value >> 16) & 0xFFFF), false); //90 AE WORD_VALUE
			}
		}
		else
		if(data_type == B1Types::B1T_LABEL)
		{
			// function call with an argument of B1T_LABEL type
			add_op(*_curr_code_sec, L"LDW X, " + str_value, false);  //AE WORD_VALUE
		}
		else
		if(data_type == B1Types::B1T_VARREF)
		{
			// function call with an argument of B1T_VARREF type
			if(is_static)
			{
				add_op(*_curr_code_sec, L"LDW X, " + str_value, false);  //AE WORD_VALUE
			}
			else
			{
				add_op(*_curr_code_sec, L"LDW X, (" + str_value + L")", false); //BE SHORT_ADDRESS, CE LONG_ADDRESS
			}
		}

		add_call_op(file_name);

		if(pre_cmd && data_type == B1Types::B1T_BYTE && mask != 0)
		{
			add_op(*_curr_code_sec, L"ADDW SP, 1", false); //84
			_stack_ptr--;
		}
	}
	else
	{
		if(data_type == B1Types::B1T_VARREF && !is_static)
		{
			return C1_T_ERROR::C1_RES_ENOTIMP;
		}

		if(file_name.empty())
		{
			file_name = L"__LIB_" + dev_name + L"_" + std::to_wstring(id) + L"_INL";
		}

		const std::wstring arg_type = (!pre_cmd && res_lvt != LVT::LVT_NONE) ? (res_lvt == LVT::LVT_IMMVAL ? L"I" : res_lvt == LVT::LVT_MEMREF ? L"M" : res_lvt == LVT::LVT_STKREF ? L"S" : L"R") : L"";

		// inline code
		std::map<std::wstring, std::wstring> params =
		{
			{ L"ARG_TYPE", arg_type },
			{ L"VALUE", (data_type == B1Types::B1T_LABEL || data_type == B1Types::B1T_VARREF || data_type == B1Types::B1T_TEXT || !arg_type.empty()) ? str_value : std::to_wstring(value) },
			{ L"MASK", std::to_wstring(mask) },
			{ L"DEV_NAME", dev_name },
			{ L"ID", std::to_wstring(id) },
			{ L"CALL_TYPE", L"INL"},
			{ L"IOCTL_NUM", std::to_wstring(ioctl_num) },
			{ L"CMD_NAME", cmd_name },
		};

		for(auto i = 0; i < more_values.size(); i++)
		{
			params.emplace(std::make_pair(L"MASK" + std::to_wstring(i), Utils::str_tohex32(more_masks[i])));
			params.emplace(std::make_pair(L"VALUE" + std::to_wstring(i), Utils::str_tohex32(more_values[i])));
		}

		if(code_place == Settings::IoCmd::IoCmdCodePlacement::CP_CURR_POS)
		{
			auto saved_it = cmd_it++;
			auto err = load_inline(0, file_name, cmd_it, params, &*saved_it);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			cmd_it = saved_it;
		}
		else
		{
			// the file name will be used later when calling load_inline function
			params[L"FILE_NAME"] = file_name;

			// Settings::IoCmd::IoCmdCodePlacement::CP_END: placement after END statement
			_end_placement.push_back(std::make_pair(cmd_it, params));
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::write_data_sec(bool code_init)
{
	B1_ASM_OPS *data = _page0 ? &_page0_sec : &_data_sec;

	_comment.clear();

	for(const auto &vn: _vars_order)
	{
		bool is_static = false;
		int32_t size, rep;
		std::wstring type;

		auto v = _mem_areas.find(vn);
		if(v != _mem_areas.cend())
		{
			// constant variables are placed in .CONST section
			if(v->second.is_const)
			{
				continue;
			}

			is_static = true;
		}
		else
		{
			v = _vars.find(vn);
			if(v == _vars.cend())
			{
				continue;
			}
		}

		_curr_src_file_id = v->second.src_file_id;
		_curr_line_cnt = v->second.src_line_cnt;

		if(v->second.dim_num == 0)
		{
			if(!B1CUtils::get_asm_type(v->second.type, &type, &size, &rep))
			{
				return C1_T_ERROR::C1_RES_EINVTYPNAME;
			}
		}
		else
		if(is_static)
		{
			if(!B1CUtils::get_asm_type(v->second.type, &type, &size, &rep))
			{
				return C1_T_ERROR::C1_RES_EINVTYPNAME;
			}

			rep = 1;

			for(int32_t i = 0; i < v->second.dim_num; i++)
			{
				rep *= v->second.dims[i * 2 + 1] - v->second.dims[i * 2] + 1;
			}

			size *= rep;
		}
		else
		{
			if(!B1CUtils::get_asm_type(v->second.type, &type, &size, &rep, v->second.dim_num))
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			// correct size for arrays with known sizes (address only, no dimensions)
			if(v->second.fixed_size)
			{
				size = size / rep;
				rep = 1;
			}
		}

		if(!code_init && std::find(_init_files.cbegin(), _init_files.cend(), v->first) != _init_files.cend())
		{
			continue;
		}

		if(_page0 && _data_size + size > STM8_PAGE0_SIZE)
		{
			_page0 = false;
			data = &_data_sec;
		}

		add_lbl(*data, data->cend(), v->first, v->second.is_volatile);
		add_data(*data, data->cend(), type + (rep == 1 ? std::wstring() : L" (" + std::to_wstring(rep) + L")"), v->second.is_volatile);

		_all_symbols.insert(v->first);

		v->second.size = size;
		v->second.address = _data_size;

		_data_size += size;
	}

	// non-user variables
	if(!_data_stmts.empty())
	{
		for(const auto &dt: _data_stmts)
		{
			// namespace
			std::wstring label = dt.first;

			// no __DAT_PTR variable for const variables data
			if(_mem_areas.find(label) != _mem_areas.cend())
			{
				continue;
			}

			label = label.empty() ? std::wstring() : (label + L"::");

			label = label + L"__DAT_PTR";
			B1_CMP_VAR var(label, B1Types::B1T_WORD, 0, false, false, -1, 0);
			B1CUtils::get_asm_type(B1Types::B1T_WORD, nullptr, &var.size);
			var.address = _data_size;
			_vars[label] = var;
			// no use of non-user variables in _vars_order
			//_vars_order.push_back(label);

			if(_page0 && _data_size + var.size > STM8_PAGE0_SIZE)
			{
				_page0 = false;
				data = &_data_sec;
			}

			add_lbl(*data, data->cend(), label, false);
			add_data(*data, data->cend(), L"DW", false);

			_all_symbols.insert(label);

			_data_size += 2;
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::write_code_sec(bool code_init)
{
	// code
	_stack_ptr = 0;
	_local_offset.clear();

	_curr_udef_args_size = 0;
	_curr_udef_arg_offsets.clear();
	_curr_udef_str_arg_offsets.clear();
	_curr_udef_str_arg_last_use.clear();

	_cmp_active = false;
	_retval_active = false;

	_clear_locals.clear();

	_allocated_arrays.clear();

	_comment.clear();

	bool int_handler = false;

	bool omit_zero_init = code_init;

	std::map<std::wstring, std::wstring> extra_params;

	for(auto ci = begin(); ci != end(); ci++)
	{
		const auto &cmd = *ci;

		for(auto si = _store_at.cbegin(); si != _store_at.cend(); si++)
		{
			if(std::get<0>(*si) == ci)
			{
				_curr_src_file_id = std::get<2>(*si);
				_curr_line_cnt = std::get<3>(*si);

				auto err = stm8_store(std::get<1>(*si));
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				_store_at.erase(si);

				_cmp_active = false;
				_retval_active = false;

				extra_params.clear();

				break;
			}
		}

		_curr_src_file_id = cmd.src_file_id;
		_curr_line_cnt = cmd.line_cnt;

		if(B1CUtils::is_label(cmd))
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			add_lbl(*_curr_code_sec, _curr_code_sec->cend(), cmd.cmd, false);
			// labels are processed in load_next_command() function
			//_all_symbols.insert(cmd.cmd);

			const auto ufn = _ufns.find(cmd.cmd);

			if(ufn != _ufns.cend())
			{
				_curr_udef_arg_offsets.clear();
				_curr_udef_str_arg_offsets.clear();
				_curr_udef_str_arg_last_use.clear();

				int32_t arg_off = 1;
				for(auto a = ufn->second.args.rbegin(); a != ufn->second.args.rend(); a++)
				{
					const auto &arg = *a;
					int32_t size = 0;
					
					if(!B1CUtils::get_asm_type(arg.type, nullptr, &size))
					{
						return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
					}

					_curr_udef_arg_offsets.insert(_curr_udef_arg_offsets.begin(), arg_off);
					if(arg.type == B1Types::B1T_STRING)
					{
						_curr_udef_str_arg_offsets.push_back(arg_off);
					}

					arg_off += size;
				}

				_curr_udef_args_size = arg_off - 1;
			}

			// temporary solution for a single argument case: function prologue code stores it in stack
			if(_curr_udef_arg_offsets.size() == 1)
			{
				if(_curr_udef_args_size == 1)
				{
					add_op(*_curr_code_sec, L"PUSH A", false); //88
					_stack_ptr++;
				}
				else
				if(_curr_udef_args_size == 2)
				{
					add_op(*_curr_code_sec, L"PUSHW X", false); //89
					_stack_ptr += 2;
				}
				else
				{
					// LONG type
					add_op(*_curr_code_sec, L"PUSHW X", false); //89
					add_op(*_curr_code_sec, L"PUSHW Y", false); //90 89
					_stack_ptr += 4;
				}
			}

			_cmp_active = false;
			_retval_active = false;

			_allocated_arrays.clear();

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(B1CUtils::is_inline_asm(cmd))
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			for(const auto &a: cmd.args)
			{
				auto trimmed = Utils::str_trim(a[0].value);
				if(!trimmed.empty())
				{
					if(trimmed.front() == L':')
					{
						add_lbl(*_curr_code_sec, _curr_code_sec->cend(), trimmed.substr(1), true, true);
					}
					else
					if(trimmed.front() == L';')
					{
						_comment = trimmed.substr(1);
					}
					else
					if(trimmed.length() >= 2)
					{
						auto first2 = trimmed.substr(0, 2);
						if(first2 == L"DB" || first2 == L"DW" || first2 == L"DD")
						{
							add_data(*_curr_code_sec, _curr_code_sec->cend(), trimmed, true, true);
						}
						else
						{
							add_op(*_curr_code_sec, trimmed, true, true);
						}
					}
					else
					{
						return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
					}
				}
			}

			_cmp_active = false;
			_retval_active = false;

			_allocated_arrays.clear();

			omit_zero_init = false;
			
			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"NS")
		{
			if(cmd.args[0][0].value.empty())
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			_curr_name_space = cmd.args[0][0].value;
			_next_label = 32768;
			_next_local = 32768;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"GA")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			const auto &var = _vars.find(cmd.args[0][0].value)->second;

			if(cmd.args.size() == 2)
			{
				// omit initialization of scalar non-volatile variables in .CODE INIT section
				if(!omit_zero_init)
				{
					auto err = stm8_st_gf(var, false);
					if(err != C1_T_ERROR::C1_RES_OK)
					{
						return err;
					}
				}
			}
			else
			{
				// allocate array memory
				auto err = stm8_st_ga(cmd, var);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				_allocated_arrays.insert(cmd.args[0][0].value);
			}

			_cmp_active = false;
			_retval_active = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"GF")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			const auto var = _vars.find(cmd.args[0][0].value);
			
			auto err = (var != _vars.end()) ?
				stm8_st_gf(var->second, false) :
				stm8_st_gf(_mem_areas.find(cmd.args[0][0].value)->second, true);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			_cmp_active = false;
			_retval_active = false;

			_allocated_arrays.erase(cmd.args[0][0].value);

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"GET")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			auto in_dev = _global_settings.GetIoDeviceName(cmd.args[0][0].value);

			if(in_dev.empty())
			{
				if(cmd.args[0][0].value.empty())
				{
					return C1_T_ERROR::C1_RES_ENODEFIODEV;
				}
				else
				{
					return C1_T_ERROR::C1_RES_EUNKIODEV;
				}
			}

			auto dev_opts = _global_settings.GetDeviceOptions(in_dev);
			if(dev_opts == nullptr || dev_opts->find(B1C_DEV_OPT_BIN) == dev_opts->cend())
			{
				return C1_T_ERROR::C1_RES_EWDEVTYPE;
			}

			std::wstring suffix =	(cmd.args[1][0].type == B1Types::B1T_BYTE)	? L"_B" :
									(cmd.args[1][0].type == B1Types::B1T_INT)	? L"_W" :
									(cmd.args[1][0].type == B1Types::B1T_WORD)	? L"_W" :
									(cmd.args[1][0].type == B1Types::B1T_LONG)	? L"_L" : L"";

			bool arr_range = false;

			if(cmd.args.size() != 2)
			{
				suffix = L"_A";

				// load starting address in X, data size in stack
				auto err = stm8_load_ptr(cmd.args[1], cmd.args[2]);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				arr_range = true;
			}

			if(dev_opts->find(B1C_DEV_OPT_INL) == dev_opts->cend())
			{
				add_call_op(L"__LIB_" + in_dev + L"_GET" + suffix);

				if(arr_range)
				{
					add_op(*_curr_code_sec, L"ADDW SP, 2", false); //5B BYTE_VALUE
					_stack_ptr -= 2;
				}
				else
				{
					auto err = stm8_store(cmd.args[1]);
					if(err != C1_T_ERROR::C1_RES_OK)
					{
						return err;
					}
				}
			}
			else
			{
				// inline code
				auto saved_it = ci++;

				if(arr_range)
				{
					add_op(*_curr_code_sec, L"POPW Y", false); //90 85
					_stack_ptr -= 2;
				}
				else
				{
					// deferred store operaton
					_store_at.emplace_back(std::make_tuple(ci, cmd.args[1], _curr_src_file_id, _curr_line_cnt));
				}

				auto err = load_inline(0, L"__LIB_" + in_dev + L"_GET" + suffix + L"_INL", ci, extra_params);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				ci = saved_it;
			}

			_cmp_active = false;
			_retval_active = false;

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"CALL")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			add_call_op(cmd.args[0][0].value);

			_cmp_active = false;
			_retval_active = false;

			_allocated_arrays.clear();

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"LA")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			// get local size
			int32_t size;
			if(!B1CUtils::get_asm_type(cmd.args[1][0].type, nullptr, &size))
			{
				return C1_T_ERROR::C1_RES_EINVTYPNAME;
			}

			if(_cmp_active)
			{
				if(size == 1)
				{
					// B1T_BYTE
					add_op(*_curr_code_sec, L"SUB SP, 1", false); //52 BYTE_VALUE
				}
				else
				if(size == 2)
				{
					// use PUSH/POP for LA/LF after compare operations (in order not to overwrite flags register)
					// B1T_INT, B1T_WORD or B1T_STRING
					if(cmd.args[1][0].type == B1Types::B1T_STRING)
					{
						// string local variable must be emptied right after creation
						add_op(*_curr_code_sec, L"PUSH 0", false); //4B BYTE_VALUE
						add_op(*_curr_code_sec, L"PUSH 0", false); //4B BYTE_VALUE

						_clear_locals.insert(cmd.args[0][0].value);
					}
					else
					{
						add_op(*_curr_code_sec, L"SUB SP, 2", false); //52 BYTE_VALUE
					}
				}
				else
				{
					// B1T_LONG
					add_op(*_curr_code_sec, L"SUB SP, 4", false); //52 BYTE_VALUE
				}
			}
			else
			{
				if(cmd.args[1][0].type == B1Types::B1T_STRING)
				{
					// string local variable must be emptied right after creation
					add_op(*_curr_code_sec, L"CLRW X", false); //5F
					add_op(*_curr_code_sec, L"PUSHW X", false); //89

					_clear_locals.insert(cmd.args[0][0].value);
				}
				else
				{
					add_op(*_curr_code_sec, L"SUB SP, " + Utils::str_tohex16(size), false); //52 BYTE_VALUE
				}
			}

			_stack_ptr += size;
			_local_offset.push_back(std::pair<B1_TYPED_VALUE, int>(B1_TYPED_VALUE(cmd.args[0][0].value, cmd.args[1][0].type), _stack_ptr - 1));

			_retval_active = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"LF")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			const auto &loc = _local_offset.back();

			if(loc.first.value != cmd.args[0][0].value)
			{
				return C1_T_ERROR::C1_RES_ESTKFAIL;
			}

			// get local size
			int size;
			if(!B1CUtils::get_asm_type(loc.first.type, nullptr, &size))
			{
				return C1_T_ERROR::C1_RES_EINVTYPNAME;
			}

			bool not_used = (_clear_locals.find(cmd.args[0][0].value) != _clear_locals.end());

			if(_cmp_active)
			{
				// use PUSH/POP for LA/LF after compare operations (in order not to overwrite flags register)
				if(size == 1)
				{
					add_op(*_curr_code_sec, L"ADDW SP, 1", false); //5B BYTE_VALUE
				}
				else
				if(size == 2)
				{
					if(loc.first.type == B1Types::B1T_STRING)
					{
						add_op(*_curr_code_sec, L"POPW X", false); //85
						if(!not_used)
						{
							add_op(*_curr_code_sec, L"PUSH CC", false); //8A
							_stack_ptr += 1;
							add_call_op(L"__LIB_STR_RLS");
							add_op(*_curr_code_sec, L"POP CC", false); //86
							_stack_ptr -= 1;
						}
					}
					else
					{
						add_op(*_curr_code_sec, L"ADDW SP, 2", false); //5B BYTE_VALUE
					}
				}
				else
				{
					add_op(*_curr_code_sec, L"ADDW SP, 4", false); //5B BYTE_VALUE
				}
			}
			else
			if(_retval_active)
			{
				// after RETVAL command LF should not change registers (to preserve function return value)
				if(loc.first.type == B1Types::B1T_STRING)
				{
					if(!not_used)
					{
						if(_retval_type == B1Types::B1T_BYTE)
						{
							add_op(*_curr_code_sec, L"PUSH A", false); //88
							_stack_ptr += 1;
							add_op(*_curr_code_sec, L"LDW X, (2, SP)", false); //1E BYTE_OFFSET
							add_call_op(L"__LIB_STR_RLS");
							add_op(*_curr_code_sec, L"POP A", false); //84
							_stack_ptr -= 1;
						}
						else
						if(_retval_type == B1Types::B1T_INT || _retval_type == B1Types::B1T_WORD || _retval_type == B1Types::B1T_STRING)
						{
							add_op(*_curr_code_sec, L"PUSHW X", false); //89
							_stack_ptr += 2;
							add_op(*_curr_code_sec, L"LDW X, (3, SP)", false); //1E BYTE_OFFSET
							add_call_op(L"__LIB_STR_RLS");
							add_op(*_curr_code_sec, L"POPW X", false); //85
							_stack_ptr -= 2;
						}
						else
						{
							// LONG type
							add_op(*_curr_code_sec, L"PUSHW X", false); //89
							add_op(*_curr_code_sec, L"PUSHW Y", false); //90 89
							_stack_ptr += 4;
							add_op(*_curr_code_sec, L"LDW X, (5, SP)", false); //1E BYTE_OFFSET
							add_call_op(L"__LIB_STR_RLS");
							add_op(*_curr_code_sec, L"POPW Y", false); //90 85
							add_op(*_curr_code_sec, L"POPW X", false); //85
							_stack_ptr -= 4;
						}
					}

					add_op(*_curr_code_sec, L"ADDW SP, " + Utils::str_tohex16(size), false); //5B BYTE_VALUE
				}
				else
				{
					add_op(*_curr_code_sec, L"ADDW SP, " + Utils::str_tohex16(size), false); //5B BYTE_VALUE
				}
			}
			else
			{
				if(loc.first.type == B1Types::B1T_STRING)
				{
					add_op(*_curr_code_sec, L"POPW X", false); //85

					if(!not_used)
					{
						add_call_op(L"__LIB_STR_RLS");
					}
				}
				else
				{
					add_op(*_curr_code_sec, L"ADDW SP, " + Utils::str_tohex16(size), false); //5B BYTE_VALUE
				}
			}

			_clear_locals.erase(cmd.args[0][0].value);

			_stack_ptr -= size;
			_local_offset.pop_back();

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"MA")
		{
			extra_params.clear();
			continue;
		}

		if(cmd.cmd == L"DAT")
		{
			extra_params.clear();
			continue;
		}

		if(cmd.cmd == L"DEF")
		{
			extra_params.clear();
			continue;
		}

		if(cmd.cmd == L"IN")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			auto in_dev = _global_settings.GetIoDeviceName(cmd.args[0][0].value);

			if(in_dev.empty())
			{
				if(cmd.args[0][0].value.empty())
				{
					return C1_T_ERROR::C1_RES_ENODEFIODEV;
				}
				else
				{
					return C1_T_ERROR::C1_RES_EUNKIODEV;
				}
			}

			auto dev_opts = _global_settings.GetDeviceOptions(in_dev);
			if(dev_opts == nullptr || dev_opts->find(B1C_DEV_OPT_TXT) == dev_opts->cend())
			{
				return C1_T_ERROR::C1_RES_EWDEVTYPE;
			}

			add_call_op(L"__LIB_" + in_dev + L"_IN");
			if(cmd.args[1][0].type == B1Types::B1T_BYTE)
			{
				add_call_op(L"__LIB_STR_CBYTE");
			}
			else
			if(cmd.args[1][0].type == B1Types::B1T_INT)
			{
				add_call_op(L"__LIB_STR_CINT");
			}
			else
			if(cmd.args[1][0].type == B1Types::B1T_WORD)
			{
				add_call_op(L"__LIB_STR_CWRD");
			}
			else
			if(cmd.args[1][0].type == B1Types::B1T_LONG)
			{
				add_call_op(L"__LIB_STR_CLNG");
			}

			// store value
			auto err = stm8_store(cmd.args[1]);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			_cmp_active = false;
			_retval_active = false;

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"IOCTL")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			auto err = stm8_write_ioctl(ci);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			_cmp_active = false;
			_retval_active = false;

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"OUT")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			auto out_dev = _global_settings.GetIoDeviceName(cmd.args[0][0].value);

			if(out_dev.empty())
			{
				if(cmd.args[0][0].value.empty())
				{
					return C1_T_ERROR::C1_RES_ENODEFIODEV;
				}
				else
				{
					return C1_T_ERROR::C1_RES_EUNKIODEV;
				}
			}

			auto dev_opts = _global_settings.GetDeviceOptions(out_dev);
			if(dev_opts == nullptr || dev_opts->find(B1C_DEV_OPT_TXT) == dev_opts->cend())
			{
				return C1_T_ERROR::C1_RES_EWDEVTYPE;
			}

			if(cmd.args[1][0].value == L"NL")
			{
				// print new line
				add_call_op(L"__LIB_" + out_dev + L"_NL");
			}
			else
			if(cmd.args[1][0].value == L"TAB")
			{
				// PRINT TAB(n) function
				// TAB(0) is a special case: move to the next print zone
				// load argument of TAB or SPC function
				auto err = stm8_load(cmd.args[1][1], B1Types::B1T_BYTE, LVT::LVT_REG);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
				add_call_op(L"__LIB_" + out_dev + L"_TAB");
			}
			else
			if(cmd.args[1][0].value == L"SPC")
			{
				// PRINT SPC(n) function
				// load argument of TAB or SPC function
				auto err = stm8_load(cmd.args[1][1], B1Types::B1T_BYTE, LVT::LVT_REG);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
				add_call_op(L"__LIB_" + out_dev + L"_SPC");
			}
			else
			{
				if(cmd.args[1][0].type == B1Types::B1T_STRING)
				{
					auto err = stm8_load(cmd.args[1], B1Types::B1T_STRING, LVT::LVT_REG);
					if(err != C1_T_ERROR::C1_RES_OK)
					{
						return err;
					}
				}
				else
				if(cmd.args[1][0].type == B1Types::B1T_WORD || cmd.args[1][0].type == B1Types::B1T_BYTE)
				{
					auto err = stm8_load(cmd.args[1], B1Types::B1T_WORD, LVT::LVT_REG);
					if(err != C1_T_ERROR::C1_RES_OK)
					{
						return err;
					}

					add_op(*_curr_code_sec, L"PUSH 2", false); //4B BYTE_VALUE
					_stack_ptr += 1;
					add_call_op(L"__LIB_STR_STR16");
					add_op(*_curr_code_sec, L"POP A", false); //84
					_stack_ptr -= 1;
				}
				else
				if(cmd.args[1][0].type == B1Types::B1T_INT)
				{
					auto err = stm8_load(cmd.args[1], B1Types::B1T_INT, LVT::LVT_REG);
					if(err != C1_T_ERROR::C1_RES_OK)
					{
						return err;
					}

					add_op(*_curr_code_sec, L"PUSH 3", false); //4B BYTE_VALUE
					_stack_ptr += 1;
					add_call_op(L"__LIB_STR_STR16");
					add_op(*_curr_code_sec, L"POP A", false); //84
					_stack_ptr -= 1;
				}
				else
				if(cmd.args[1][0].type == B1Types::B1T_LONG)
				{
					auto err = stm8_load(cmd.args[1], B1Types::B1T_LONG, LVT::LVT_REG);
					if(err != C1_T_ERROR::C1_RES_OK)
					{
						return err;
					}

					add_op(*_curr_code_sec, L"PUSH 2", false); //4B BYTE_VALUE
					_stack_ptr += 1;
					add_call_op(L"__LIB_STR_STR32");
					add_op(*_curr_code_sec, L"POP A", false); //84
					_stack_ptr -= 1;
				}

				add_call_op(L"__LIB_" + out_dev + L"_OUT");
			}

			_cmp_active = false;
			_retval_active = false;

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"PUT")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			auto out_dev = _global_settings.GetIoDeviceName(cmd.args[0][0].value);

			if(out_dev.empty())
			{
				if(cmd.args[0][0].value.empty())
				{
					return C1_T_ERROR::C1_RES_ENODEFIODEV;
				}
				else
				{
					return C1_T_ERROR::C1_RES_EUNKIODEV;
				}
			}

			auto dev_opts = _global_settings.GetDeviceOptions(out_dev);
			if(dev_opts == nullptr || dev_opts->find(B1C_DEV_OPT_BIN) == dev_opts->cend())
			{
				return C1_T_ERROR::C1_RES_EWDEVTYPE;
			}

			std::wstring suffix =	(cmd.args[1][0].type == B1Types::B1T_BYTE)	? L"_B" :
									(cmd.args[1][0].type == B1Types::B1T_INT)	? L"_W" :
									(cmd.args[1][0].type == B1Types::B1T_WORD)	? L"_W" :
									(cmd.args[1][0].type == B1Types::B1T_LONG)	? L"_L" :
									(cmd.args[1][0].type == B1Types::B1T_STRING)? L"_S" : L"";

			bool arr_range = false;

			if(cmd.args.size() == 2)
			{
				auto err = stm8_load(cmd.args[1], cmd.args[1][0].type, LVT::LVT_REG);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}
			else
			{
				suffix = L"_A";

				// load starting address in X, data size in stack
				auto err = stm8_load_ptr(cmd.args[1], cmd.args[2]);
				if (err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				arr_range = true;
			}

			if(dev_opts->find(B1C_DEV_OPT_INL) == dev_opts->cend())
			{
				add_call_op(L"__LIB_" + out_dev + L"_PUT" + suffix);

				if(arr_range)
				{
					add_op(*_curr_code_sec, L"ADDW SP, 2", false); //5B BYTE_VALUE
					_stack_ptr -= 2;
				}
			}
			else
			{
				// inline code
				auto saved_it = ci++;

				if(arr_range)
				{
					add_op(*_curr_code_sec, L"POPW Y", false); //90 85
					_stack_ptr -= 2;
				}

				auto err = load_inline(0, L"__LIB_" + out_dev + L"_PUT" + suffix + L"_INL", ci, extra_params);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				ci = saved_it;
			}


			_cmp_active = false;
			_retval_active = false;

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"RST")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			std::wstring name_space = cmd.args[0][0].value;

			if(_data_stmts.find(name_space) == _data_stmts.end())
			{
				return C1_T_ERROR::C1_RES_ENODATA;
			}

			name_space = name_space.empty() ? std::wstring() : (name_space + L"::");

			if(cmd.args.size() == 1)
			{
				add_op(*_curr_code_sec, L"LDW X, " + name_space + L"__DAT_START", false); //AE WORD_VALUE
				_req_symbols.insert(name_space + L"__DAT_START");
			}
			else
			{
				auto rst_label = _dat_rst_labels.find(cmd.args[1][0].value);
				if(rst_label == _dat_rst_labels.end())
				{
					return C1_T_ERROR::C1_RES_EUNRESSYMBOL;
				}
				add_op(*_curr_code_sec, L"LDW X, " + rst_label->second, false); //AE WORD_VALUE
			}

			add_op(*_curr_code_sec, L"LDW (" + name_space + L"__DAT_PTR), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS
			_req_symbols.insert(name_space + L"__DAT_PTR");

			_cmp_active = false;
			_retval_active = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"READ")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			std::wstring name_space = cmd.args[0][0].value;

			if(_data_stmts.find(name_space) == _data_stmts.end())
			{
				return C1_T_ERROR::C1_RES_ENODATA;
			}

			name_space = name_space.empty() ? std::wstring() : (name_space + L"::");

			// load value
			if(cmd.args[1][0].type == B1Types::B1T_BYTE)
			{
#ifdef C1_DAT_STORE_BYTE_AS_WORD
				add_op(*_curr_code_sec, L"LDW X, (" + name_space + L"__DAT_PTR)", false); //BE SHORT_ADDRESS, CE LONG_ADDRESS
				add_op(*_curr_code_sec, L"INCW X", false); //5C
				add_op(*_curr_code_sec, L"LD A, (X)", false);
				add_op(*_curr_code_sec, L"INCW X", false); //5C
				add_op(*_curr_code_sec, L"LDW (" + name_space + L"__DAT_PTR), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS
#else
				add_op(*_curr_code_sec, L"LDW X, (" + name_space + L"__DAT_PTR)", false); //BE SHORT_ADDRESS, CE LONG_ADDRESS
				add_op(*_curr_code_sec, L"LD A, (X)", false);
				add_op(*_curr_code_sec, L"INCW X", false); //5C
				add_op(*_curr_code_sec, L"LDW (" + name_space + L"__DAT_PTR), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS
#endif
			}
			else
			if(cmd.args[1][0].type == B1Types::B1T_INT || cmd.args[1][0].type == B1Types::B1T_WORD || cmd.args[1][0].type == B1Types::B1T_STRING)
			{
				add_op(*_curr_code_sec, L"LDW X, (" + name_space + L"__DAT_PTR)", false); //BE SHORT_ADDRESS, CE LONG_ADDRESS
				add_op(*_curr_code_sec, L"PUSHW X", false); // 89
				_stack_ptr += 2;
				add_op(*_curr_code_sec, L"INCW X", false); //5C
				add_op(*_curr_code_sec, L"INCW X", false); //5C
				add_op(*_curr_code_sec, L"LDW (" + name_space + L"__DAT_PTR), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS
				add_op(*_curr_code_sec, L"POPW X", false); //85
				_stack_ptr -= 2;
				add_op(*_curr_code_sec, L"LDW X, (X)", false); //FE
			}
			else
			{
				// LONG type
				add_op(*_curr_code_sec, L"LDW X, (" + name_space + L"__DAT_PTR)", false); //BE SHORT_ADDRESS, CE LONG_ADDRESS
				add_op(*_curr_code_sec, L"LDW Y, X", false); //90 93
				add_op(*_curr_code_sec, L"ADDW X, 4", false); //1C WORD_VALUE
				add_op(*_curr_code_sec, L"LDW (" + name_space + L"__DAT_PTR), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS
				add_op(*_curr_code_sec, L"LDW X, Y", false); //93
				add_op(*_curr_code_sec, L"LDW Y, (Y)", false); //90 FE
				add_op(*_curr_code_sec, L"LDW X, (2, X)", false); //EE BYTE_OFFSET
			}
			_req_symbols.insert(name_space + L"__DAT_PTR");

			// store value
			auto err = stm8_store(cmd.args[1]);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			_cmp_active = false;
			_retval_active = false;

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"RETVAL")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			auto err = stm8_load(cmd.args[0], cmd.args[1][0].type, LVT::LVT_REG);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			if(cmd.args[0].size() == 1 && cmd.args[0][0].type == B1Types::B1T_STRING && cmd.args[1][0].type == B1Types::B1T_STRING && _locals.find(cmd.args[0][0].value) != _locals.cend())
			{
				// remove the last __LIB_STR_CPY call
				_curr_code_sec->pop_back();
				// do not call __LIB_STR_RLS for the local
				_clear_locals.insert(cmd.args[0][0].value);
			}

			_cmp_active = false;
			_retval_active = true;
			_retval_type = cmd.args[1][0].type;

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"RET")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			// release strings passed as arguments
			// temporary solution for a single argument case: function prologue code stores it in stack
			if(_curr_udef_arg_offsets.size() == 1)
			{
				if(!_curr_udef_str_arg_last_use.empty())
				{
					_curr_code_sec->erase(_curr_udef_str_arg_last_use.cbegin()->second);
				}
				else
				if(_curr_udef_str_arg_offsets.size() == 1)
				{
					int32_t offset;

					if(_retval_type == B1Types::B1T_BYTE)
					{
						add_op(*_curr_code_sec, L"PUSH A", false); //88
						_stack_ptr += 1;
						offset = _stack_ptr - _curr_udef_args_size + 1;
						add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)", false); //1E BYTE_OFFSET
						add_call_op(L"__LIB_STR_RLS");
						add_op(*_curr_code_sec, L"POP A", false); //84
						_stack_ptr -= 1;
					}
					else
					if(_retval_type == B1Types::B1T_INT || _retval_type == B1Types::B1T_WORD || _retval_type == B1Types::B1T_STRING)
					{
						add_op(*_curr_code_sec, L"PUSHW X", false); //89
						_stack_ptr += 2;
						offset = _stack_ptr - _curr_udef_args_size + 1;
						add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)", false); //1E BYTE_OFFSET
						add_call_op(L"__LIB_STR_RLS");
						add_op(*_curr_code_sec, L"POPW X", false); //85
						_stack_ptr -= 2;
					}
					else
					{
						// LONG type
						add_op(*_curr_code_sec, L"PUSHW X", false); //89
						add_op(*_curr_code_sec, L"PUSHW Y", false); //90 89
						_stack_ptr += 4;
						offset = _stack_ptr - _curr_udef_args_size + 1;
						add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)", false); //1E BYTE_OFFSET
						add_call_op(L"__LIB_STR_RLS");
						add_op(*_curr_code_sec, L"POPW Y", false); //90 85
						add_op(*_curr_code_sec, L"POPW X", false); //85
						_stack_ptr -= 4;
					}
				}
			}
			else
			{
				int32_t offset;

				for(const auto &sa: _curr_udef_str_arg_offsets)
				{
					auto last = _curr_udef_str_arg_last_use.find(sa);
					if(last != _curr_udef_str_arg_last_use.cend())
					{
						_curr_code_sec->erase(last->second);
					}
					else
					{
						if(_retval_type == B1Types::B1T_BYTE)
						{
							add_op(*_curr_code_sec, L"PUSH A", false); //88
							_stack_ptr += 1;
							offset = _stack_ptr + _global_settings.GetRetAddressSize() + sa;
							add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)", false); //1E BYTE_OFFSET
							add_call_op(L"__LIB_STR_RLS");
							add_op(*_curr_code_sec, L"POP A", false); //84
							_stack_ptr -= 1;
						}
						else
						if(_retval_type == B1Types::B1T_INT || _retval_type == B1Types::B1T_WORD || _retval_type == B1Types::B1T_STRING)
						{
							add_op(*_curr_code_sec, L"PUSHW X", false); //89
							_stack_ptr += 2;
							offset = _stack_ptr + _global_settings.GetRetAddressSize() + sa;
							add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)", false); //1E BYTE_OFFSET
							add_call_op(L"__LIB_STR_RLS");
							add_op(*_curr_code_sec, L"POPW X", false); //85
							_stack_ptr -= 2;
						}
						else
						{
							// LONG type
							add_op(*_curr_code_sec, L"PUSHW X", false); //89
							add_op(*_curr_code_sec, L"PUSHW Y", false); //90 89
							_stack_ptr += 4;
							offset = _stack_ptr + _global_settings.GetRetAddressSize() + sa;
							add_op(*_curr_code_sec, L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)", false); //1E BYTE_OFFSET
							add_call_op(L"__LIB_STR_RLS");
							add_op(*_curr_code_sec, L"POPW Y", false); //90 85
							add_op(*_curr_code_sec, L"POPW X", false); //85
							_stack_ptr -= 4;
						}
					}
				}
			}

			// temporary solution for a single argument case: function prologue code stores it in stack
			if(_curr_udef_arg_offsets.size() == 1)
			{
				add_op(*_curr_code_sec, L"ADDW SP, " + std::to_wstring(_curr_udef_args_size), false); //5B BYTE_VALUE
				_stack_ptr -= _curr_udef_args_size;
			}

			if(_stack_ptr != 0)
			{
				// probably some local variables are not released
				if(_global_settings.GetFixRetStackPtr())
				{
					add_op(*_curr_code_sec, L"ADDW SP, " + std::to_wstring(_stack_ptr), false); //5B BYTE_VALUE
				}
				else
				{
					_warnings.push_back(std::make_tuple(GetCurrLineNum(), GetCurrFileName(), C1_T_WARNING::C1_WRN_WRETSTKOVF));
				}
			}

			if(int_handler)
			{
				add_op(*_curr_code_sec, L"IRET", false); //80
			}
			else
			{
				add_op(*_curr_code_sec, _ret_stmt, false); //RET 81, RETF 87
			}

			_cmp_active = false;
			_retval_active = false;

			_allocated_arrays.clear();

			_curr_udef_args_size = 0;
			_curr_udef_arg_offsets.clear();
			_curr_udef_str_arg_offsets.clear();
			_curr_udef_str_arg_last_use.clear();

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"SET")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			if(cmd.args[0][0].value == L"ERR")
			{
				if(!B1CUtils::is_num_val(cmd.args[1][0].value))
				{
					return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
				}

				int32_t n;
				auto err = Utils::str2int32(cmd.args[1][0].value, n);
				if(err != B1_RES_OK)
				{
					return static_cast<C1_T_ERROR>(err);
				}

				add_op(*_curr_code_sec, L"MOV (__LIB_ERR_LAST_ERR), " + std::to_wstring(n), false); //35 BYTE_VALUE LONG_ADDRESS
				_init_files.push_back(L"__LIB_ERR_LAST_ERR");
			}

			_cmp_active = false;
			_retval_active = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"TRR")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			auto trr_dev = _global_settings.GetIoDeviceName(cmd.args[0][0].value);

			if(trr_dev.empty())
			{
				if(cmd.args[0][0].value.empty())
				{
					return C1_T_ERROR::C1_RES_ENODEFIODEV;
				}
				else
				{
					return C1_T_ERROR::C1_RES_EUNKIODEV;
				}
			}

			auto dev_opts = _global_settings.GetDeviceOptions(trr_dev);
			if(dev_opts == nullptr || dev_opts->find(B1C_DEV_OPT_BIN) == dev_opts->cend())
			{
				return C1_T_ERROR::C1_RES_EWDEVTYPE;
			}

			std::wstring suffix =	(cmd.args[1][0].type == B1Types::B1T_BYTE)	? L"_B" :
									(cmd.args[1][0].type == B1Types::B1T_INT)	? L"_W" :
									(cmd.args[1][0].type == B1Types::B1T_WORD)	? L"_W" :
									(cmd.args[1][0].type == B1Types::B1T_LONG)	? L"_L" : L"";

			bool arr_range = false;

			if(cmd.args.size() == 2)
			{
				auto err = stm8_load(cmd.args[1], cmd.args[1][0].type, LVT::LVT_REG);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}
			else
			{
				suffix = L"_A";

				// load starting address in X, data size in stack
				auto err = stm8_load_ptr(cmd.args[1], cmd.args[2]);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				arr_range = true;
			}

			if(dev_opts->find(B1C_DEV_OPT_INL) == dev_opts->cend())
			{
				add_call_op(L"__LIB_" + trr_dev + L"_TRR" + suffix);

				if(arr_range)
				{
					add_op(*_curr_code_sec, L"ADDW SP, 2", false); //5B BYTE_VALUE
					_stack_ptr -= 2;
				}
				else
				{
					auto err = stm8_store(cmd.args[1]);
					if(err != C1_T_ERROR::C1_RES_OK)
					{
						return err;
					}
				}
			}
			else
			{
				// inline code
				auto saved_it = ci++;

				if(arr_range)
				{
					add_op(*_curr_code_sec, L"POPW Y", false); //90 85
					_stack_ptr -= 2;
				}
				else
				{
					// deferred store operaton
					_store_at.emplace_back(std::make_tuple(ci, cmd.args[1], _curr_src_file_id, _curr_line_cnt));
				}

				auto err = load_inline(0, L"__LIB_" + trr_dev + L"_TRR" + suffix + L"_INL", ci, extra_params);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
				ci = saved_it;
			}

			_cmp_active = false;
			_retval_active = false;

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"XARG")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			auto fn = get_fn(cmd.args[0]);
			if(fn == nullptr)
			{
				return C1_T_ERROR::C1_RES_EINTERR;
			}

			auto value = cmd.args[0][1].value;
			std::wstring atype;

			if(B1CUtils::is_imm_val(value) || Utils::check_const_name(value))
			{
				atype = L"I";

				if(cmd.args[0][1].type != B1Types::B1T_BYTE && cmd.args[0][1].type != B1Types::B1T_INT && cmd.args[0][1].type != B1Types::B1T_WORD && cmd.args[0][1].type != B1Types::B1T_LONG)
				{
					return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
				}
			}
			else
			if(_locals.find(value) != _locals.end())
			{
				atype = L"S";

				int32_t offset = stm8_get_type_cvt_offset(cmd.args[0][1].type, fn->args[0].type);
				if(offset < 0)
				{
					return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
				}
				offset += stm8_get_local_offset(value);
				value = Utils::str_tohex16(offset);
			}
			else
			if(get_fn(cmd.args[0][1]) == nullptr && !B1CUtils::is_fn_arg(value))
			{
				atype = L"M";

				value = stm8_get_var_addr(value, cmd.args[0][1].type, fn->args[0].type, true);
				if(value.empty())
				{
					return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
				}
			}
			else
			{
				return C1_T_ERROR::C1_RES_EINTERR;
			}
			extra_params[cmd.args[0][0].value + L"_TYPE"] = atype;
			extra_params[cmd.args[0][0].value + L"_VALUE"] = value;

			_cmp_active = false;
			_retval_active = false;

			omit_zero_init = false;

			continue;
		}

		if(cmd.cmd == L"END")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			int_handler = false;

			_cmp_active = false;
			_retval_active = false;

			_allocated_arrays.clear();

			_curr_udef_args_size = 0;
			_curr_udef_arg_offsets.clear();
			_curr_udef_str_arg_offsets.clear();
			_curr_udef_str_arg_last_use.clear();

			for(const auto &epc: _end_placement)
			{
				auto err = load_inline(0, epc.second.at(L"FILE_NAME"), std::next(ci), epc.second);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}
			_end_placement.clear();

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"ERR")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			_init_files.push_back(L"__LIB_ERR_LAST_ERR");
			if(cmd.args[0][0].value.empty())
			{
				add_op(*_curr_code_sec, L"TNZ (__LIB_ERR_LAST_ERR)", false); // 3D SHORT_ADDRESS 72 5D LONG_ADDRESS
				add_op(*_curr_code_sec, L"JRNE " + cmd.args[1][0].value, false); //26 SIGNED_BYTE_OFFSET
			}
			else
			{
				add_op(*_curr_code_sec, L"LD A, (__LIB_ERR_LAST_ERR)", false); //B6 SHORT_ADDRESS C6 LONG_ADDRESS
				add_op(*_curr_code_sec, L"CP A, " + cmd.args[0][0].value, false); //A1 BYTE_VALUE
				add_op(*_curr_code_sec, L"JREQ " + cmd.args[1][0].value, false); //27 SIGNED_BYTE_OFFSET
			}
			_req_symbols.insert(cmd.args[1][0].value);

			_cmp_active = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"IMP" || cmd.cmd == L"INI")
		{
			extra_params.clear();
			continue;
		}

		if(cmd.cmd == L"INL")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			auto err = load_inline(0, cmd.args[0][0].value, std::next(ci), extra_params);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			_cmp_active = false;
			_retval_active = false;

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"INT")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			int_handler = true;

			std::string irq_name = Utils::wstr2str(cmd.args[0][0].value);
			int int_ind = _global_settings.GetInterruptIndex(irq_name);
			
			if(int_ind < 0)
			{
				// unknown interrupt name
				return C1_T_ERROR::C1_RES_EUNKINT;
			}

 			auto int_hnd = _irq_handlers.find(int_ind);
			if(int_hnd != _irq_handlers.end() && !int_hnd->second.empty())
			{
				// multiple handlers are defined for the same interrupt
				return C1_T_ERROR::C1_RES_EMULTINTHND;
			}

			const auto int_lbl_name = L"__" + cmd.args[0][0].value;

			add_lbl(*_curr_code_sec, _curr_code_sec->cend(), int_lbl_name, false);
			_all_symbols.insert(int_lbl_name);
			
			_irq_handlers[int_ind] = int_lbl_name;
			_req_symbols.insert(int_lbl_name);

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"USES")
		{
			continue;
		}

		if(cmd.cmd == L"JMP")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			add_op(*_curr_code_sec, L"JRA " + cmd.args[0][0].value, false); //20 SIGNED_BYTE_OFFSET
			_req_symbols.insert(cmd.args[0][0].value);

			_cmp_active = false;

			extra_params.clear();

			continue;
		}

		if(cmd.cmd == L"JT" || cmd.cmd == L"JF")
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			if(!_cmp_active)
			{
				return C1_T_ERROR::C1_RES_ENOCMPOP;
			}

			if(_cmp_op == L"==")
			{
				if(cmd.cmd == L"JT")
				{
					add_op(*_curr_code_sec, L"JREQ " + cmd.args[0][0].value, false); //27 SIGNED_BYTE_OFFSET
				}
				else
				{
					add_op(*_curr_code_sec, L"JRNE " + cmd.args[0][0].value, false); //26 SIGNED_BYTE_OFFSET
				}
			}
			else
			if(_cmp_op == L"<>")
			{
				if(cmd.cmd == L"JT")
				{
					add_op(*_curr_code_sec, L"JRNE " + cmd.args[0][0].value, false); //26 SIGNED_BYTE_OFFSET
				}
				else
				{
					add_op(*_curr_code_sec, L"JREQ " + cmd.args[0][0].value, false); //27 SIGNED_BYTE_OFFSET
				}
			}
			else
			if(_cmp_op == L">")
			{
				if(_cmp_type == B1Types::B1T_INT || _cmp_type == B1Types::B1T_STRING || _cmp_type == B1Types::B1T_LONG)
				{
					// signed comparison
					if(cmd.cmd == L"JT")
					{
						add_op(*_curr_code_sec, L"JRSGT " + cmd.args[0][0].value, false); //2C SIGNED_BYTE_OFFSET
					}
					else
					{
						add_op(*_curr_code_sec, L"JRSLE " + cmd.args[0][0].value, false); //2D SIGNED_BYTE_OFFSET
					}
				}
				else
				{
					// unsigned comparison
					if(cmd.cmd == L"JT")
					{
						add_op(*_curr_code_sec, L"JRUGT " + cmd.args[0][0].value, false); //22 SIGNED_BYTE_OFFSET
					}
					else
					{
						add_op(*_curr_code_sec, L"JRULE " + cmd.args[0][0].value, false); //23 SIGNED_BYTE_OFFSET
					}
				}
			}
			else
			if(_cmp_op == L">=")
			{
				if(_cmp_type == B1Types::B1T_INT || _cmp_type == B1Types::B1T_STRING || _cmp_type == B1Types::B1T_LONG)
				{
					// signed comparison
					if(cmd.cmd == L"JT")
					{
						add_op(*_curr_code_sec, L"JRSGE " + cmd.args[0][0].value, false); //2E SIGNED_BYTE_OFFSET
					}
					else
					{
						add_op(*_curr_code_sec, L"JRSLT " + cmd.args[0][0].value, false); //2F SIGNED_BYTE_OFFSET
					}
				}
				else
				{
					// unsigned comparison
					if(cmd.cmd == L"JT")
					{
						add_op(*_curr_code_sec, L"JRUGE " + cmd.args[0][0].value, false); //24 SIGNED_BYTE_OFFSET
					}
					else
					{
						add_op(*_curr_code_sec, L"JRULT " + cmd.args[0][0].value, false); //25 SIGNED_BYTE_OFFSET
					}
				}
			}
			else
			if(_cmp_op == L"<")
			{
				if(_cmp_type == B1Types::B1T_INT || _cmp_type == B1Types::B1T_STRING || _cmp_type == B1Types::B1T_LONG)
				{
					// signed comparison
					if(cmd.cmd == L"JT")
					{
						add_op(*_curr_code_sec, L"JRSLT " + cmd.args[0][0].value, false); //2F SIGNED_BYTE_OFFSET
					}
					else
					{
						add_op(*_curr_code_sec, L"JRSGE " + cmd.args[0][0].value, false); //2E SIGNED_BYTE_OFFSET
					}
				}
				else
				{
					// unsigned comparison
					if(cmd.cmd == L"JT")
					{
						add_op(*_curr_code_sec, L"JRULT " + cmd.args[0][0].value, false); //25 SIGNED_BYTE_OFFSET
					}
					else
					{
						add_op(*_curr_code_sec, L"JRUGE " + cmd.args[0][0].value, false); //24 SIGNED_BYTE_OFFSET
					}
				}
			}
			else
			if(_cmp_op == L"<=")
			{
				if(_cmp_type == B1Types::B1T_INT || _cmp_type == B1Types::B1T_STRING || _cmp_type == B1Types::B1T_LONG)
				{
					// signed comparison
					if(cmd.cmd == L"JT")
					{
						add_op(*_curr_code_sec, L"JRSLE " + cmd.args[0][0].value, false); //2D SIGNED_BYTE_OFFSET
					}
					else
					{
						add_op(*_curr_code_sec, L"JRSGT " + cmd.args[0][0].value, false); //2C SIGNED_BYTE_OFFSET
					}
				}
				else
				{
					// unsigned comparison
					if(cmd.cmd == L"JT")
					{
						add_op(*_curr_code_sec, L"JRULE " + cmd.args[0][0].value, false); //23 SIGNED_BYTE_OFFSET
					}
					else
					{
						add_op(*_curr_code_sec, L"JRUGT " + cmd.args[0][0].value, false); //22 SIGNED_BYTE_OFFSET
					}
				}
			}
			else
			{
				return C1_T_ERROR::C1_RES_EUNKINST;
			}

			_req_symbols.insert(cmd.args[0][0].value);

			_retval_active = false;

			extra_params.clear();

			continue;
		}

		if(B1CUtils::is_un_op(cmd))
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			// unary operation
			auto err = stm8_un_op(cmd, omit_zero_init);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			_cmp_active = false;
			_retval_active = false;

			if(cmd.cmd != L"=")
			{
				omit_zero_init = false;
			}

			extra_params.clear();

			continue;
		}

		if(B1CUtils::is_bin_op(cmd))
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			// additive operation
			if(cmd.cmd == L"+" || cmd.cmd == L"-")
			{
				auto err = stm8_add_op(cmd);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}
			else
			// multiplicative operation
			if(cmd.cmd == L"*" || cmd.cmd == L"/" || cmd.cmd == L"%" || cmd.cmd == L"^")
			{
				auto err = stm8_mul_op(cmd);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}
			else
			// bitwise operation
			if(cmd.cmd == L"&" || cmd.cmd == L"|" || cmd.cmd == L"~")
			{
				auto err = stm8_bit_op(cmd);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}
			else
			// shift operation
			if(cmd.cmd == L"<<" || cmd.cmd == L">>")
			{
				auto err = stm8_shift_op(cmd);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}

			_cmp_active = false;
			_retval_active = false;

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		if(B1CUtils::is_log_op(cmd))
		{
			if(_out_src_lines)
			{
				_comment = Utils::str_trim(_src_lines[cmd.src_line_id]);
			}

			if(cmd.args[0][0].type == B1Types::B1T_STRING || cmd.args[1][0].type == B1Types::B1T_STRING)
			{
				// string comparison
				auto err = stm8_str_cmp_op(cmd);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}
			else
			{
				// numeric comparison
				auto err = stm8_num_cmp_op(cmd);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}

			_retval_active = false;

			omit_zero_init = false;

			extra_params.clear();

			continue;
		}

		return C1_T_ERROR::C1_RES_EUNKINST;
	}

	if(!_store_at.empty() || !_end_placement.empty())
	{
		return C1_T_ERROR::C1_RES_EINTERR;
	}

	return C1_T_ERROR::C1_RES_OK;
}

std::wstring C1STM8Compiler::correct_SP_offset(const std::wstring &arg, int32_t op_size, bool &no_SP_off, int32_t *offset /*= nullptr*/) const
{
	int32_t n;

	no_SP_off = true;
	if(offset != nullptr)
	{
		*offset = -1;
	}

	if(arg.find(L",SP)") != std::wstring::npos)
	{
		no_SP_off = false;

		if(Utils::str2int32(arg.substr(1, arg.length() - 5), n) == B1_RES_OK)
		{
			n -= op_size;

			if(!(n <= 0 || n > 255))
			{
				if(offset != nullptr)
				{
					*offset = n;
				}

				return L"(" + Utils::str_tohex16(n) + L",SP)";
			}
		}
	}

	return std::wstring();
}

// add function description
bool C1STM8Compiler::is_arithm_op(const B1_ASM_OP_STM8 &ao, int32_t &size, bool *uses_SP /*= nullptr*/) const
{
	bool res = false;
	auto &op = ao._op;

	if(op == L"LDW" || op == L"ADDW" || op == L"SUBW" || op == L"MUL" || op == L"DIV" || op == L"DIVW" || op == L"INCW" || op == L"DECW" || op == L"NEGW" || op == L"CPLW" || op == L"CLRW" ||
		op == L"SLLW" || op == L"SLAW" || op == L"SRLW" || op == L"SRAW" || op == L"RLWA" || op == L"RRWA")
	{
		size = 2;
		res = true;
		if(op == L"LDW" && (ao._args[0] == L"X" || ao._args[0] == L"Y" || ao._args[0] == L"SP") && (ao._args[1] == L"X" || ao._args[1] == L"Y" || ao._args[1] == L"SP"))
		{
			res = false;
		}
	}
	else
	if(op == L"LD" || op == L"ADD" || op == L"SUB" || op == L"ADC" || op == L"SBC" || op == L"INC" || op == L"DEC" || op == L"NEG" || op == L"AND" || op == L"OR" || op == L"XOR" || op == L"CPL"
		|| op == L"CLR" || op == L"SLL" || op == L"SLA" || op == L"SRL" || op == L"SRA")
	{
		size = 1;
		res = true;
		if(op == L"LD" && (ao._args[0] == L"A" || ao._args[0] == L"XL" || ao._args[0] == L"XH" || ao._args[0] == L"YL" || ao._args[0] == L"YH") && (ao._args[1] == L"A" || ao._args[1] == L"XL" || ao._args[1] == L"XH" || ao._args[1] == L"YL" || ao._args[1] == L"YH"))
		{
			res = false;
		}
	}

	if(uses_SP != nullptr)
	{
		*uses_SP = false;
		if(ao._args.size() > 0 && (ao._args[0] == L"SP" || ao._args[0].find(L",SP)") != std::wstring::npos))
		{
			*uses_SP = true;
		}
		else
		if(ao._args.size() == 2 && (ao._args[1] == L"SP" || ao._args[1].find(L",SP)") != std::wstring::npos))
		{
			*uses_SP = true;
		}
	}

	return res;
}

// a strange function - must be revised
// for A, X and Y registers
bool C1STM8Compiler::is_reg_used(const B1_ASM_OP_STM8 &ao, const std::wstring &reg_name, bool &reg_write_op) const
{
	reg_write_op = false;

	if(ao._type == AOT::AOT_LABEL)
	{
		return false;
	}
	if(!ao.Parse())
	{
		return true;
	}
	if(ao._op == L"JRA" || ao._op == L"JP" || ao._op == L"JPF" || ao._op == L"JRT" || ((ao._op.size() > 2 && ao._op[0] == L'J' && ao._op[1] == L'R') || ao._op == L"BTJF" || ao._op == L"BTJT"))
	{
		return true;
	}
	if(ao._op == L"CALLR" || ao._op == L"CALL" || ao._op == L"CALLF")
	{
		return true;
	}
	if((ao._op == L"RLWA" || ao._op == L"RRWA") && reg_name == L"A")
	{
		return true;
	}
	if((ao._op == L"LD" || ao._op == L"LDW" || ao._op == L"LDF") && ao._args[0] == reg_name && ao._args[1] != L"(" + reg_name + L")" && ao._args[1].find(L"," + reg_name + L")") == std::wstring::npos)
	{
		reg_write_op = true;
		return false;
	}
	if((ao._op == L"CLR" || ao._op == L"CLRW") && ao._args[0] == reg_name)
	{
		reg_write_op = true;
		return false;
	}
	if((ao._op == L"POP" || ao._op == L"POPW") && ao._args[0] == reg_name)
	{
		reg_write_op = true;
		return false;
	}
	if(ao._op == L"RET" || ao._op == L"RETF" || ao._op == L"IRET" || ao._op == L"TRAP")
	{
		reg_write_op = true;
		return false;
	}
	for(auto &a: ao._args)
	{
		if(a == reg_name)
		{
			return true;
		}

		if((reg_name == L"X" || reg_name == L"Y") && (a == L"(" + reg_name + L")" || a.find(L"," + reg_name + L")") != std::wstring::npos))
		{
			return true;
		}
	}
	if(ao._op == L"EXG" && ((reg_name == L"X" && ao._args[1] == L"XL") || (reg_name == L"Y" && ao._args[1] == L"YL")))
	{
		return true;
	}

	if(ao._op == L"LD" && ((reg_name == L"X" && (ao._args[1] == L"XL" || ao._args[1] == L"XH")) || (reg_name == L"Y" && (ao._args[1] == L"YL" || ao._args[1] == L"YH"))))
	{
		return true;
	}

	return false;
}

// a strange function - must be revised
// for A, X and Y registers
bool C1STM8Compiler::is_reg_used_after(B1_ASM_OPS::const_iterator start, B1_ASM_OPS::const_iterator end, const std::wstring &reg_name, bool branch /*= false*/) const
{
	int n = 0;

	for(start++; start != end && (!branch || n < 5); start++, n++)
	{
		auto &ao = *static_cast<const B1_ASM_OP_STM8 *>(start->get());

		if(ao._type == AOT::AOT_LABEL)
		{
			continue;
		}

		if(!ao.Parse())
		{
			return true;
		}

		bool write_op = false;
		bool reg_used = is_reg_used(ao, reg_name, write_op);

		if(ao._op == L"JRA" || ao._op == L"JP" || ao._op == L"JPF" || ao._op == L"JRT")
		{
			if(branch)
			{
				return true;
			}

			auto label = _opt_labels.find(ao._args[0]);
			return (label == _opt_labels.cend()) ? true : is_reg_used_after(label->second, end, reg_name, true);
		}
		else
		if((ao._op.size() > 2 && ao._op[0] == L'J' && ao._op[1] == L'R') || ao._op == L"BTJF" || ao._op == L"BTJT")
		{
			if(branch)
			{
				return true;
			}

			auto label = _opt_labels.find(ao._args[(ao._op[0] == L'J') ? 0 : 2]);
			if(label == _opt_labels.cend())
			{
				return true;
			}

			if(is_reg_used_after(label->second, end, reg_name, true))
			{
				return true;
			}

			continue;
		}

		if(reg_used || write_op)
		{
			return reg_used;
		}
	}

	return (start != end);
}

C1STM8Compiler::C1STM8Compiler(bool out_src_lines, bool opt_nocheck)
: C1Compiler(out_src_lines, opt_nocheck)
, _page0(true)
, _stack_ptr(0)
, _curr_udef_args_size(0)
, _cmp_active(false)
, _cmp_type(B1Types::B1T_UNKNOWN)
, _retval_active(false)
, _retval_type(B1Types::B1T_UNKNOWN)
{
	if(_global_settings.GetRetAddressSize() == 2)
	{
		_call_stmt = L"CALLR";
		_ret_stmt = L"RET";
	}
	else
	{
		_call_stmt = L"CALLF";
		_ret_stmt = L"RETF";
	}
}

C1STM8Compiler::~C1STM8Compiler()
{
}

C1_T_ERROR C1STM8Compiler::WriteCodeInitBegin()
{
	_comment.clear();

	// interrupt vector table
	add_op(_code_init_sec, L"INT __START", false);
	_req_symbols.insert(L"__START");

	int prev = 0;

	for(const auto &i: _irq_handlers)
	{
		for(int n = prev + 1; n < i.first; n++)
		{
			add_op(_code_init_sec, L"INT __UNHANDLED", false);
			_req_symbols.insert(L"__UNHANDLED");
		}

		add_op(_code_init_sec, L"INT " + i.second, false);
		_req_symbols.insert(i.second);

		prev = i.first;
	}

	if(_req_symbols.find(L"__UNHANDLED") != _req_symbols.end())
	{
		// unhandled interrupt handler (empty loop)
		add_lbl(_code_init_sec, _code_init_sec.cend(), L"__UNHANDLED", false);
		_all_symbols.insert(L"__UNHANDLED");
		add_op(_code_init_sec, L"JRA __UNHANDLED", false); //20 SIGNED_BYTE_OFFSET
		_req_symbols.insert(L"__UNHANDLED");
	}

	// init code begin
	add_lbl(_code_init_sec, _code_init_sec.cend(), L"__START", false);
	_all_symbols.insert(L"__START");

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::WriteCodeInitDAT()
{
	// DAT statements initialization code
	for(auto const &ns: _data_stmts_init)
	{
		std::wstring name_space = ns.empty() ? std::wstring() : (ns + L"::");

		add_op(_code_init_sec, L"LDW X, " + name_space + L"__DAT_START", false); //AE WORD_VALUE
		_req_symbols.insert(name_space + L"__DAT_START");
		add_op(_code_init_sec, L"LDW (" + name_space + L"__DAT_PTR), X", false); //BF SHORT_ADDRESS CF LONG_ADDRESS
		_req_symbols.insert(name_space + L"__DAT_PTR");
	}
	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::WriteCodeInitEnd()
{
	if(_const_size != 0)
	{
		add_op(_code_init_sec, L"JRA __CODE_START", false); //20 SIGNED_BYTE_OFFSET
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::Optimize1(bool &changed)
{
	auto &cs = *_code_secs.begin();
	auto i = cs.begin();

	while(i != cs.end())
	{
		int32_t rule_id = 0x10000;

		auto &ao = *static_cast<B1_ASM_OP_STM8 *>(i->get());

		if(ao._type == AOT::AOT_LABEL)
		{
			_opt_labels[ao._data] = i;
			i++;
			continue;
		}

		if(ao._is_inline || !ao.Parse())
		{
			i++;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(	((ao._op == L"LDW" || ao._op == L"LD") && (ao._args[0] == L"X" || ao._args[0] == L"Y" || ao._args[0] == L"A") && ao._args[1] == L"0x0") ||
			(ao._op == L"MOV" && ao._args[1] == L"0x0")
			)
		{
			// LDW X, 0 -> CLRW X
			// LD A, 0 -> CLR A
			// MOV (addr), 0 -> CLR (addr)
			ao._data = (ao._op == L"LDW" ? L"CLRW " : L"CLR ") + ao._args[0];
			ao._parsed = false;

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"ADDW" || ao._op == L"SUBW" || ao._op == L"ADD" || ao._op == L"SUB" || ao._op == L"OR" || ao._op == L"AND" || ao._op == L"XOR") &&
			(ao._args[0] == L"A" || ao._args[0] == L"X" || ao._args[0] == L"Y" || ao._args[0] == L"SP") &&
			(ao._args[1] == L"0x1" || ao._args[1] == L"0x0" || (ao._args[1] == L"-0x1" || Utils::str_toupper(ao._args[1]) == L"0XFFFF" || Utils::str_toupper(ao._args[1]) == L"0XFFFFFFFF" || (Utils::str_toupper(ao._args[1]) == L"0XFF" && ao._args[0] == L"A")))
			)
		{
			// -ADD/ADDW/SUB/SUBW/OR A/X/Y/SP, 0
			// AND A, 0 -> CLR A
			// -AND A, 0xFF
			// ADD/ADDW A/X/Y, 1 -> INC/INCW A/X/Y
			// SUB/SUBW A/X/Y, 1 -> INC/DECW A/X/Y
			if(ao._args[1] == L"0x0" || ao._op == L"AND")
			{
				if(!(ao._op == L"AND" && (ao._args[1] == L"0x0" || ao._args[1] == L"0x1")))
				{
					auto next = std::next(i);
					cs.erase(i);
					i = next;

					update_opt_rule_usage_stat(rule_id);
					changed = true;
					continue;
				}
				else
				if(ao._op == L"AND" && ao._args[1] == L"0x0")
				{
					ao._data = L"CLR A";
					ao._parsed = false;

					update_opt_rule_usage_stat(rule_id);
					changed = true;
					continue;
				}
			}
			else
			if(ao._args[0] != L"SP" && ao._op != L"OR" && ao._op != L"XOR")
			{
				if(ao._args[1] == L"0x1")
				{
					ao._data = (
						ao._op == L"ADDW" ? L"INCW " :
						ao._op == L"SUBW" ? L"DECW " :
						ao._op == L"ADD" ? L"INC " : L"DEC "
						) + ao._args[0];
				}
				else
				{
					ao._data = (
						ao._op == L"ADDW" ? L"DECW " :
						ao._op == L"SUBW" ? L"INCW " :
						ao._op == L"ADD" ? L"DEC " : L"INC "
						) + ao._args[0];
				}
				ao._parsed = false;

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		auto next1 = std::next(i);
		if(next1 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon1 = *static_cast<B1_ASM_OP_STM8 *>(next1->get());
		if(aon1._is_inline || !aon1.Parse())
		{
			i++;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if((ao._op == L"PUSH" && aon1._op == L"POP" && ao._args[0] != L"CC") || (ao._op == L"PUSHW" && aon1._op == L"POPW"))
		{
			if(ao._op == L"PUSH")
			{
				if(ao._args[0] == aon1._args[0])
				{
					cs.erase(next1);
					next1 = std::next(i);
					cs.erase(i);
					i = next1;
				}
				else
				{
					if(ao._args[0][0] == L'(' && aon1._args[0][0] == L'(')
					{
						// MOV (mem2), (mem1)
						ao._data = L"MOV " + aon1._args[0] + L", " + ao._args[0];
						ao._parsed = false;
						cs.erase(next1);
					}
					else
					{
						// LD dst, src
						ao._data = L"LD " + aon1._args[0] + L", " + ao._args[0];
						ao._parsed = false;
						cs.erase(next1);
					}
				}
			}
			else
			{
				// PUSHW reg1
				// POPW reg2
				if(ao._args[0] == aon1._args[0])
				{
					cs.erase(next1);
					next1 = std::next(i);
					cs.erase(i);
					i = next1;
				}
				else
				{
					// LDW reg2, reg1
					ao._data = L"LDW " + aon1._args[0] + L", " + ao._args[0];
					ao._parsed = false;
					cs.erase(next1);
				}
			}

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if((ao._op == L"ADD" || ao._op == L"ADDW") && ao._args[0] == L"SP" && ((aon1._op == L"PUSH" && aon1._args[0] == L"A" && ao._args[1] == L"0x1") || (aon1._op == L"PUSHW" && ao._args[1] == L"0x2")))
		{
			// ADDW SP, 2
			// PUSHW X/Y
			// ->
			// LDW (1, SP), X/Y
			// or
			// ADDW SP, 1
			// PUSH A
			// ->
			// LD(1, SP), A
			ao._data = std::wstring(aon1._op == L"PUSH" ? L"LD" : L"LDW") + L" (1, SP), " + aon1._args[0];
			ao._parsed = false;
			cs.erase(next1);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		int i_size = 0;
		bool i_arithm_op = is_arithm_op(ao, i_size);
		
		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(!ao._volatile && i_arithm_op && i_size == 1 && ao._args[0].front() == L'(' && (aon1._op == L"LD" || aon1._op == L"MOV") && ao._args[0] == aon1._args[0])
		{
			// -CLR/LD/... (<mem_addr>), <smth>
			// LD/MOV (<mem_addr>), <smth1>
			cs.erase(i);
			i = next1;

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(!ao._volatile && i_arithm_op && (ao._args[0] == L"A" || ao._args[0] == L"X" || ao._args[0] == L"Y") && (aon1._op == L"LD" || aon1._op == L"LDW") && ao._args[0] == aon1._args[0] &&
			aon1._args[1] != L"(X)" && aon1._args[1] != L"(Y)" && aon1._args[1].find(L",X)") == std::wstring::npos && aon1._args[1].find(L",Y)") == std::wstring::npos)
		{
			// -CLR/LD/... <reg>, <smth>
			// LD <reg>, <smth1>
			cs.erase(i);
			i = next1;

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		auto next2 = std::next(next1);
		if(next2 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon2 = *static_cast<B1_ASM_OP_STM8 *>(next2->get());
		if(aon2._is_inline || !aon2.Parse())
		{
			i++;
			continue;
		}

		int n1_size = 0;
		bool n1_arithm_op = is_arithm_op(aon1, n1_size);

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (
			(((ao._op == L"PUSHW" && ao._args[0] == L"X") || ((ao._op == L"SUBW" || ao._op == L"SUB") && ao._args[0] == L"SP" && ao._args[1] == L"0x2")) &&
			(n1_arithm_op && n1_size == 2 && aon1._args[0] == L"X") &&
			(aon2._op == L"LDW" && aon2._args[0] == L"(0x1,SP)" && aon2._args[1] == L"X")) ||

			(((ao._op == L"PUSH" && ao._args[0] == L"A") || ((ao._op == L"SUBW" || ao._op == L"SUB") && ao._args[0] == L"SP" && ao._args[1] == L"0x1")) &&
			(n1_arithm_op && n1_size == 1 && aon1._args[0] == L"A") &&
			(aon2._op == L"LD" && aon2._args[0] == L"(0x1,SP)" && aon2._args[1] == L"A"))
			)
		{
			// PUSH/PUSHW A/X or SUBW SP, 1/2
			// LD/LDW A/X, <smth>
			// LD/LDW (0x1, SP), A/X
			// ->
			// LD/LDW A/X, smth
			// PUSH/PUSHW A/X

			bool err = false;

			if(aon1._args.size() > 1)
			{
				bool no_SP_off = true;
				auto new_off = correct_SP_offset(aon1._args[1], n1_size, no_SP_off);
				if(new_off.empty())
				{
					err = !no_SP_off;
				}
				else
				{
					aon1._data = aon1._op + (n1_size == 1 ? L" A, " : L" X, ") + new_off;
					aon1._parsed = false;
				}
			}

			if(!err)
			{
				aon2._data = n1_size == 1 ? L"PUSH A" : L"PUSHW X";
				aon2._parsed = false;
				cs.erase(i);
				i = next1;

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (
			(i_arithm_op && (ao._args[0] == L"A" || ao._args[0] == L"X" || ao._args[0] == L"Y") && !(ao._args.size() == 2 && (ao._args[1] == L"X" || ao._args[1] == L"Y" || ao._args[1] == L"XL" || ao._args[1] == L"YL" || ao._args[1] == L"XH" || ao._args[1] == L"YH" || ao._args[1] == L"SP"))) &&
			ao._op != L"MUL" && ao._op != L"DIV" && ao._op != L"DIVW" &&
			((aon1._op == L"PUSH" || aon1._op == L"PUSHW") && aon1._args[0] == ao._args[0]) &&
			((aon2._op == L"LD" || aon2._op == L"LDW") && aon2._args[0] == ao._args[0] && aon2._args[1] == L"(0x1,SP)")
			)
		{
			// LD/LDW A/X, smth (not Y) or ADD/ADDW A/X, smth or SUB/SUBW A/X, smth
			// PUSH/PUSHW A/X
			// -LD/LDW A/X, (1, SP)
			cs.erase(next2);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if( ((ao._op == L"PUSHW" && aon2._op == L"POPW") || (ao._op == L"PUSH" && aon2._op == L"POP")) && (ao._args[0] == aon2._args[0] && ao._args[0] != L"CC") &&
			(n1_arithm_op && ao._args[0] != aon1._args[0])
			)
		{
			// -PUSH/PUSHW <reg>
			// LD/LDW/ADD/ADDW not <reg> and not (1, SP) or (2, SP), smth
			// -POP/POPW <reg>
			bool err = false;
			int32_t size = (ao._op == L"PUSH") ? 1 : 2;
			int32_t off;
			bool no_SP_off = true;
			if(!correct_SP_offset(aon1._args[0], 0, no_SP_off, &off).empty())
			{
				// correct arg[0]
				if(off <= size)
				{
					err = true;
				}
				else
				{
					aon1._data = aon1._op + L" (" + Utils::str_tohex16(off - size) + L", SP)" + (aon1._args.size() > 1 ? L", ", aon1._args[1] : L"");
					aon1._parsed = false;
				}
			}
			else
			if(aon1._args.size() > 1)
			{
				// check arg[1]
				if(!correct_SP_offset(aon1._args[1], 0, no_SP_off, &off).empty())
				{
					// correct arg[1]
					if(off <= size)
					{
						err = true;
					}
					else
					{
						aon1._data = aon1._op + L" " + aon1._args[0] + L", (" + Utils::str_tohex16(off - size) + L", SP)";
						aon1._parsed = false;
					}
				}
			}

			if(!err)
			{
				cs.erase(i);
				cs.erase(next2);
				i = next1;

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if ((ao._op == L"ADD" || ao._op == L"ADDW") && ao._args[0] == L"SP" && ao._args[1] == L"0x4" &&
			(aon1._op == L"PUSHW" && aon1._args[0] == L"X") &&
			(aon2._op == L"PUSHW" && aon2._args[0] == L"Y")
			)
		{
			// ADDW SP, 4
			// PUSHW X
			// PUSHW Y
			// ->
			// LDW (1, SP), Y
			// LDW (3, SP), X
			ao._data = L"LDW (1, SP), Y";
			ao._parsed = false;
			aon1._data = L"LDW (3, SP), X";
			aon1._parsed = false;
			cs.erase(next2);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if ((ao._op == L"LDW" && ao._args[0] == L"X") &&
			(aon1._op == L"SUBW" && aon1._args[0] == L"X") &&
			(aon2._op == L"INCW" && aon2._args[0] == L"X")
			)
		{
			// LDW X, <imm>
			// SUBW X, <smth>
			// INCW X
			// ->
			// LDW X, <imm> + 1
			// SUBW X, <smth>
			int32_t n;
			if(Utils::str2int32(ao._args[1], n) == B1_RES_OK)
			{
				ao._data = L"LDW X, " + ao._args[1] + L" + 1";
				ao._parsed = false;
				cs.erase(next2);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (
			(ao._op == L"CLRW" && ao._args[0] == L"X") &&
			((aon1._op == L"LDW" && aon1._args[1] == L"X") || (aon1._op == L"PUSHW" && aon1._args[0] == L"X")) &&
			((aon2._op == L"LDW" && aon2._args[0] == L"X" && aon2._args[1] == L"0x1") || (aon2._op == L"CLRW" && aon2._args[0] == L"X"))
			)
		{
			// CLRW X
			// LDW (smth), X 
			// LDW X, 1.l
			// ->
			// CLRW X
			// LDW (smth), X
			// INCW X
			if(aon2._op == L"LDW")
			{
				aon2._data = L"INCW X";
				aon2._parsed = false;
			}
			else
			{
				cs.erase(next2);
			}

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}


		auto next3 = std::next(next2);
		if(next3 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon3 = *static_cast<B1_ASM_OP_STM8 *>(next3->get());
		if(aon3._is_inline || !aon3.Parse())
		{
			i++;
			continue;
		}


		int n2_size = 0;
		bool n2_arithm_op = is_arithm_op(aon2, n2_size);

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (
			(((ao._op == L"PUSHW" && ao._args[0] == L"X") || ((ao._op == L"SUBW" || ao._op == L"SUB") && ao._args[0] == L"SP" && ao._args[1] == L"0x2")) &&
			(aon1._op == L"LDW" && aon1._args[0] == L"X") &&
			(n2_arithm_op && n2_size == 2 && !(aon2._op == L"MUL" || aon2._op == L"DIV" || aon2._op == L"DIVW") && aon2._args[0] == L"X") &&
			(aon3._op == L"LDW" && aon3._args[0] == L"(0x1,SP)" && aon3._args[1] == L"X")) ||

			(((ao._op == L"PUSH" && ao._args[0] == L"A") || ((ao._op == L"SUBW" || ao._op == L"SUB") && ao._args[0] == L"SP" && ao._args[1] == L"0x1")) &&
			(aon1._op == L"LD" && aon1._args[0] == L"A") &&
			(n2_arithm_op && n2_size == 1 && aon2._args[0] == L"A") &&
			(aon3._op == L"LD" && aon3._args[0] == L"(0x1,SP)" && aon3._args[1] == L"A"))
			)
		{
			// PUSHW X or SUBW SP, 2
			// LDW X, smth
			// ADDW X, 10
			// LDW (0x1, SP), X
			// ->
			// LDW X, smth
			// ADDW X, 10
			// PUSHW X
			bool err = false;
			int32_t size = (aon3._op == L"LD") ? 1 : 2;
			std::wstring n1data;
			bool no_SP_off = true;
			n1data = correct_SP_offset(aon1._args[1], size, no_SP_off);
			if(n1data.empty())
			{
				err = !no_SP_off;
			}
			else
			{
				n1data = (size == 1 ? L"LD A, " : L"LDW X, ") + n1data;
			}

			std::wstring n2data;
			if(!err && aon2._args.size() > 1)
			{
				no_SP_off = true;
				n2data = correct_SP_offset(aon2._args[1], size, no_SP_off);
				if(n2data.empty())
				{
					err = !no_SP_off;
				}
				else
				{
					n2data = aon2._op + (size == 1 ? L" A, " : L" X, ") + n2data;
				}
			}

			if(!err)
			{
				if(!n1data.empty())
				{
					aon1._data = n1data;
					aon1._parsed = false;
				}
				if(!n2data.empty())
				{
					aon2._data = n2data;
					aon2._parsed = false;
				}
				aon3._data = size == 1 ? L"PUSH A" : L"PUSHW X";
				aon3._parsed = false;
				cs.erase(i);
				i = next1;

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (ao._op == L"CLRW" && ao._args[0] == L"Y" &&
			!aon1._volatile && aon1._op == L"LDW" && aon1._args[0] == L"X" &&
			!aon2._volatile && aon2._op == L"LDW" && aon2._args[1] == L"Y" &&
			!aon3._volatile && aon3._op == L"LDW" && aon3._args[1] == L"X"
			)
		{
			// CLRW Y
			// LDW X, smth1
			// LDW smth2, Y
			// LDW smth3, X
			// ->
			// CLRW X
			// LDW smth2, X
			// LDW X, smth1
			// LDW smth3, X

			ao._data = L"CLRW X";
			ao._parsed = false;
			aon2._data = aon1._data;
			aon2._parsed = false;
			aon1._data = L"LDW " + aon2._args[0] + L", X";
			aon1._parsed = false;

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (ao._op == L"RLWA" && ao._args[0] == L"X" &&
			(
				((aon1._op == L"OR" || aon1._op == L"AND" || aon1._op == L"XOR") && aon1._args[0] == L"A" &&
				aon2._op == L"RLWA" && aon2._args[0] == L"X") ||
				((aon2._op == L"OR" || aon2._op == L"AND" || aon2._op == L"XOR") && aon2._args[0] == L"A" &&
				aon1._op == L"RLWA" && aon1._args[0] == L"X")
			) &&
			aon3._op == L"RLWA" && aon3._args[0] == L"X"
			)
		{
			// RLWA X
			// OR/AND/XOR A, 0X2000.lh
			// RLWA X
			// RLWA X
			// ->
			// LD A, XH
			// OR/AND/XOR A, 0X2000.lh
			// LD XH, A
			cs.erase(next3);
			if(aon2._op == L"RLWA")
			{
				ao._data = L"LD A, XH";
				ao._parsed = false;
				aon2._data = L"LD XH, A";
				aon2._parsed = false;
			}
			else
			{
				ao._data = L"LD A, XL";
				ao._parsed = false;
				aon1._data = aon2._data;
				aon1._parsed = false;
				aon2._data = L"LD XL, A";
				aon2._parsed = false;
			}

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		auto next4 = std::next(next3);
		if(next4 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon4 = *static_cast<B1_ASM_OP_STM8 *>(next4->get());
		if(aon4._is_inline || !aon4.Parse())
		{
			i++;
			continue;
		}

		int n4_size = 0;
		bool n4_arithm_op = is_arithm_op(aon4, n4_size);

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (ao._op == L"PUSHW" && ao._args[0] == L"X" &&
			aon1._op == L"LDW" && aon1._args[0] == L"X" && aon1._args[1] == L"Y" &&
			aon2._op == L"LDW" && aon2._args[0].front() == L'(' && aon2._args[0].find(L",SP)") == std::wstring::npos && aon2._args[1] == L"X" &&
			aon3._op == L"POPW" && aon3._args[0] == L"X" &&
			aon4._op == L"LDW" && aon4._args[0].front() == L'(' && aon4._args[0].find(L",SP)") == std::wstring::npos && aon4._args[1] == L"X"
			)
		{
			// PUSHW X
			// LDW X, Y
			// LDW (NS1::__VAR_LA + 0x18), X
			// POPW X
			// LDW (NS1::__VAR_LA + 0x18 + 2), X
			// ->
			// LDW (NS1::__VAR_LA + 0x18), Y
			// LDW (NS1::__VAR_LA + 0x18 + 2), X
			cs.erase(i);
			cs.erase(next1);
			cs.erase(next3);
			aon2._data = aon2._op + L" " + aon2._args[0] + L", Y";
			aon2._parsed = false;
			i = next2;

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (ao._op == L"PUSHW" && aon1._op == L"PUSHW" && ao._args[0] != aon1._args[0] && (aon2._op == L"SUB" || aon2._op == L"SUBW") && aon2._args[0] == L"SP" &&
			aon3._op == L"LDW" && aon4._op == L"LDW" && (aon3._args[0] == L"X" || aon3._args[0] == L"Y") && (aon4._args[0] == L"X" || aon4._args[0] == L"Y") &&
			(aon3._args[0] != aon4._args[0]) && aon3._args[1].find(L",SP)") != std::wstring::npos && aon4._args[1].find(L",SP)") != std::wstring::npos
			)
		{
			// PUSHW X
			// PUSHW Y
			// SUB SP, 0x4
			// LDW Y, (0x5, SP)
			// LDW X, (0x7, SP)
			// ->
			// PUSHW X
			// PUSHW Y
			// SUB SP, 0x4

			int32_t n;
			if(Utils::str2int32(aon2._args[1], n) == B1_RES_OK)
			{
				if(n > 0 && n <= 255)
				{
					int32_t x_off = (ao._args[0] == L"X") ? n + 3 : (n + 1);
					int32_t y_off = (ao._args[0] == L"X") ? n + 1 : (n + 3);

					bool no_SP_off = true;
					int32_t off1 = -1;
					correct_SP_offset(aon3._args[1], 0, no_SP_off, &off1);
					int32_t off2 = -1;
					correct_SP_offset(aon4._args[1], 0, no_SP_off, &off2);
					if(off1 > 0 && off2 > 0)
					{
						if (((x_off == off1 && aon3._args[0] == L"X") && (y_off == off2 && aon4._args[0] == L"Y")) ||
							((x_off == off2 && aon4._args[0] == L"X") && (y_off == off1 && aon3._args[0] == L"Y")))
						{
							cs.erase(next3);
							cs.erase(next4);

							update_opt_rule_usage_stat(rule_id);
							changed = true;
							continue;
						}
					}
				}
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (n4_arithm_op &&
			!ao._volatile && !ao._is_inline && ao._op == L"LD" && ao._args[0] == L"A" &&
			(
				// imm. value
				(ao._args[1][0] != L'[' && ao._args[1][0] != L'(' && ao._args[1] != L"XL" && ao._args[1] != L"XH" && ao._args[1] != L"YL" && ao._args[1] != L"YH") ||
				// direct addressing
				(ao._args[1][0] == L'(' && ao._args[1] != L"(X)" && ao._args[1] != L"(Y)" && ao._args[1].find(L",X)") == std::wstring::npos && ao._args[1].find(L",Y)") == std::wstring::npos && ao._args[1].find(L",SP)") == std::wstring::npos)
			) &&
			aon1._op == L"CLRW" && aon1._args[0] == L"X" &&
			aon2._op == L"LD" && aon2._args[0] == L"XL" &&
			// direct addressing
			aon3._op == L"LDW" && aon3._args[0].front() == L'(' && aon3._args[0] != L"(Y)" && aon3._args[0].find(L",Y)") == std::wstring::npos && aon3._args[0].find(L",SP)") == std::wstring::npos && aon3._args[1] == L"X"
			)
		{
			// LD A, imm1 or (smth1)
			// CLRW X
			// LD XL, A
			// LDW (smth2), X
			// ->
			// CLR (smth2)
			// MOV (smth2 + 1), imm1 or (smth1)

			ao._data = L"CLR " + aon3._args[0];
			ao._parsed = false;
			aon1._data = L"MOV " + std::wstring(aon3._args[0].cbegin(), std::prev(aon3._args[0].cend())) + L"+1), " + ao._args[1];
			aon1._parsed = false;
			cs.erase(next2);
			cs.erase(next3);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}


		auto next5 = std::next(next4);
		if(next5 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon5 = *static_cast<B1_ASM_OP_STM8 *>(next5->get());
		if(aon5._is_inline || !aon5.Parse())
		{
			i++;
			continue;
		}

		int n5_size = 0;
		bool n5_arithm_op = is_arithm_op(aon5, n5_size);

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"LD" && ao._args[0] == L"A") &&
			(aon1._op == L"CLRW" && aon1._args[0] == L"X") &&
			(aon2._op == L"LD" && aon2._args[0] == L"XL") &&
			((aon3._op == L"DECW" || aon3._op == L"INCW" || aon3._op == L"ADDW" || aon3._op == L"SUBW") && aon3._args[0] == L"X") &&
			(aon4._op == L"LD" && aon4._args[1] == L"XL") &&
			(aon5._op == L"LD" && aon5._args[0] == ao._args[1] && aon5._args[1] == L"A")
		)
		{
			// LD A, (NS1::__VAR_I)
			// CLRW X
			// LD XL, A
			// DECW X
			// LD A, XL
			// LD (NS1::__VAR_I), A
			// ->
			// DEC (NS1::__VAR_I)

			bool proceed = true;
			if(aon3._op == L"DECW")
			{
				ao._data = L"DEC " + ao._args[1];
				ao._parsed = false;
				cs.erase(next3);
				cs.erase(next5);
			}
			else
			if(aon3._op == L"INCW")
			{
				ao._data = L"INC " + ao._args[1];
				ao._parsed = false;
				cs.erase(next3);
				cs.erase(next5);
			}
			else
			{
				if(B1CUtils::is_num_val(aon3._args[1]))
				{
					aon3._data = ((aon3._op == L"ADDW") ? L"ADD A, " : L"SUB A, ") + aon3._args[1];
					aon3._parsed = false;
				}
				else
				{
					proceed = false;
				}
			}
			
			if(proceed)
			{
				cs.erase(next1);
				cs.erase(next2);
				cs.erase(next4);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (!ao._volatile && ao._op == L"LDW" && ao._args[0] == L"X" && ao._args[1] != L"Y" && ao._args[1][0] != L'[' && ao._args[1][1] != L'[' &&
			aon1._op == L"LD" && (aon1._args[1] == L"XL" || aon1._args[1] == L"XH") &&
			(aon2._op == L"OR" || aon2._op == L"AND" || aon2._op == L"XOR") &&
			aon3._op == L"LD" && aon3._args[0] == aon1._args[1] &&
			aon4._op == L"LDW" && aon4._args[0] == ao._args[1] && aon4._args[1] == ao._args[0] &&
			(aon5._type == AOT::AOT_LABEL || n5_arithm_op || aon5._op[0] ==L'J' || aon5._op == L"CP" || aon5._op == L"CPW" || aon5._op == L"TNZ" || aon5._op == L"TNZW" || aon5._op == L"CALL" || aon5._op == L"CALLR" || aon5._op == L"CALLF" || aon5._op == L"RET" || aon5._op == L"RETF" || aon5._op == L"IRET")
			)
		{
			// LDW X, (smth non volatile)
			// LD A, XH
			// OR / AND / XOR A, smth1
			// LD XH, A
			// LDW (smth non volatile), X
			// ->
			// LD A, (smth)
			// OR / AND / XOR A, smth1
			// LD (smth), A
			bool ind_addr = (ao._args[1] == L"(X)" || ao._args[1] == L"(Y)" || ao._args[1].find(L",SP)") != std::wstring::npos || ao._args[1].find(L",Y)") != std::wstring::npos || ao._args[1].find(L",X)") != std::wstring::npos);

			if(aon1._args[1] == L"XH" || !ind_addr)
			{
				std::wstring new_arg = ao._args[1];

				if(aon1._args[1] == L"XL")
				{
					new_arg.pop_back();
					new_arg += L" + 1)";
				}

				ao._data = L"LD A, " + new_arg;
				ao._parsed = false;
				aon1._data = aon2._op + L" A, " + aon2._args[1];
				aon1._parsed = false;
				aon2._data = L"LD " + new_arg + L", A";
				aon2._parsed = false;
				cs.erase(next4);
				cs.erase(next3);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}


		i++;
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::Optimize2(bool &changed)
{
	auto &cs = *_code_secs.begin();
	auto i = cs.begin();

	while(i != cs.end())
	{
		int32_t rule_id = 0x20000;

		auto &ao = *static_cast<B1_ASM_OP_STM8 *>(i->get());

		if(ao._type == AOT::AOT_LABEL)
		{
			_opt_labels[ao._data] = i;
			i++;
			continue;
		}

		if(ao._is_inline || !ao.Parse())
		{
			i++;
			continue;
		}

		auto next1 = std::next(i);
		if(next1 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon1 = *static_cast<B1_ASM_OP_STM8 *>(next1->get());
		if(aon1._is_inline || !aon1.Parse())
		{
			i++;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if((ao._op == L"PUSH" && ao._args[0][0] != L'(') || ao._op == L"PUSHW" || ((ao._op == L"ADD" || ao._op == L"ADDW" || ao._op == L"SUB" || ao._op == L"SUBW") && ao._args[0] == L"SP"))
		{
			// PUSH/PUSHW <reg> or ADDW/SUBW SP, <value1>
			// ops not using stack
			// ADDW/SUBW SP, <value2>
			// ->
			// ADDW/SUBW SP, <corrected_value>
			// ops not using stack

			bool proceed = false;
			auto next = next1;
			B1_ASM_OP_STM8 *next_ao = nullptr;

			for(; next != cs.end(); next++)
			{
				next_ao = static_cast<B1_ASM_OP_STM8 *>(next->get());

				if(next_ao->_type == AOT::AOT_LABEL)
				{
					break;
				}

				if(next_ao->_is_inline || !next_ao->Parse())
				{
					break;
				}

				if((next_ao->_op == L"ADDW" || next_ao->_op == L"ADD" || next_ao->_op == L"SUBW" || next_ao->_op == L"SUB") && next_ao->_args[0] == L"SP")
				{
					proceed = true;
					break;
				}

				int n_size = 0;
				bool n_uses_SP = false;
				bool n_arithm_op = is_arithm_op(*next_ao, n_size, &n_uses_SP);

				if(
					(!n_arithm_op && !(next_ao->_op == L"LD" || next_ao->_op == L"LDW" || next_ao->_op == L"TNZ" || next_ao->_op == L"TNZW" || next_ao->_op == L"CP" || next_ao->_op == L"CPW")) ||
					n_uses_SP
					)
				{
					break;
				}
			}

			if(proceed)
			{
				int32_t sp_delta = 0;
				bool err = false;

				if(ao._op == L"PUSH") sp_delta--;
				else
				if(ao._op == L"PUSHW") sp_delta -= 2;
				else
				{
					int32_t n;
					if(Utils::str2int32(ao._args[1], n) == B1_RES_OK)
					{
						if(n > 0 && n <= 255)
						{
							if(ao._op == L"ADD" || ao._op == L"ADDW")
							{
								sp_delta += n;
							}
							else
							{
								sp_delta -= n;
							}
						}
						else
						{
							err = true;
						}
					}
					else
					{
						err = true;
					}
				}

				if(!err)
				{
					int32_t n;
					if(Utils::str2int32(next_ao->_args[1], n) == B1_RES_OK)
					{
						if(n > 0 && n <= 255)
						{
							if(next_ao->_op == L"ADD" || next_ao->_op == L"ADDW")
							{
								sp_delta += n;
							}
							else
							{
								// do not allow:
								// PUSH/PUSHW <reg>
								// SUBW SP, <value>
								if(ao._op == L"PUSH" || ao._op == L"PUSHW")
								{
									err = true;
								}
								else
								{
									sp_delta -= n;
								}
							}
						}
						else
						{
							err = true;
						}
					}
					else
					{
						err = true;
					}

					if(!err && sp_delta >= -255 && sp_delta <= 255)
					{
						if(sp_delta == 0)
						{
							cs.erase(next);
							next1 = std::next(i);
							cs.erase(i);
							i = next1;
						}
						else
						{
							ao._data = (sp_delta > 0) ? L"ADDW SP, " + Utils::str_tohex16(sp_delta) : (L"SUBW SP, " + Utils::str_tohex16(-sp_delta));
							ao._parsed = false;
							cs.erase(next);
						}

						update_opt_rule_usage_stat(rule_id);
						changed = true;
						continue;
					}
				}
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if( (ao._op == L"PUSHW" && aon1._op == L"LDW" &&
			((aon1._args[0] == ao._args[0] && aon1._args[1] == L"(0x1,SP)") || (aon1._args[1] == ao._args[0] && aon1._args[0] == L"(0x1,SP)"))) ||

			(ao._op == L"PUSH" && ao._args[0] == L"A" && aon1._op == L"LD" &&
				((aon1._args[0] == L"A" && aon1._args[1] == L"(0x1,SP)") || (aon1._args[1] == L"A" && aon1._args[0] == L"(0x1,SP)")))
			)
		{
			// PUSH/PUSHW <reg>
			// -LD/LDW <reg>, (0x1, SP) or LD/LDW (0x1, SP), <reg>
			cs.erase(next1);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if((ao._op == L"LD" || ao._op == L"LDW") && (aon1._op == L"ADDW" || aon1._op == L"ADD") && aon1._args[0] == L"SP")
		{
			// -LDW (0x1, SP), X
			// ADDW SP, 0x4
			bool err = false;
			int32_t n, n1;
			int32_t size = ao._op == L"LD" ? 1 : 2;
			bool no_SP_off = true;
			if(correct_SP_offset(ao._args[0], 0, no_SP_off, &n).empty())
			{
				err = !no_SP_off;
			}

			if(!no_SP_off && !err)
			{
				if(Utils::str2int32(aon1._args[1], n1) == B1_RES_OK)
				{
					if(!(n1 > 0 && n1 <= 255))
					{
						err = true;
					}
				}

				if(!err && (n1 - n) >= (size - 1))
				{
					cs.erase(i);
					i = next1;

					update_opt_rule_usage_stat(rule_id);
					changed = true;
					continue;
				}
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(((ao._op == L"LDW" && aon1._op == L"LDW") || (ao._op == L"LD" && aon1._op == L"LD")) && ao._args[0] == aon1._args[1] && ao._args[1] == aon1._args[0])
		{
			// LDW X, (ADDR)
			// -LDW (ADDR), X
			if(!aon1._volatile)
			{
				cs.erase(next1);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}


		int i_size = 0;
		bool i_arithm_op = is_arithm_op(ao, i_size);

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(!aon1._volatile && i_arithm_op && i_size == 2 && !(ao._op == L"MUL" || ao._op == L"DIV" || ao._op == L"DIVW") && aon1._op == L"TNZW" && ao._args[0] == aon1._args[0])
		{
			// LDW X, smth not reg
			// -TNZW X
			if (!(ao._op == L"LDW" && (ao._args[1] == L"X" || ao._args[1] == L"Y" || ao._args[1] == L"SP")))
			{
				cs.erase(next1);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}
		else
		if(!aon1._volatile && i_arithm_op && i_size == 1 && aon1._op == L"TNZ" && ao._args[0] == aon1._args[0])
		{
			// LD A, smth not reg
			// -TNZ A
			if (!(ao._op == L"LD" && (ao._args[1] == L"XL" || ao._args[1] == L"YL" || ao._args[1] == L"XH" || ao._args[1] == L"YH")))
			{
				cs.erase(next1);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		auto next2 = std::next(next1);
		if(next2 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon2 = *static_cast<B1_ASM_OP_STM8 *>(next2->get());
		if(aon2._is_inline || !aon2.Parse())
		{
			i++;
			continue;
		}

		int n1_size = 0;
		bool n1_arithm_op = is_arithm_op(aon1, n1_size);
		int n2_size = 0;
		bool n2_arithm_op = is_arithm_op(aon2, n2_size);

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			!ao._volatile && (ao._op == L"LDW" || ao._op == L"LD") && (ao._op == aon2._op) && (ao._args == aon2._args) && (ao._args[1] == L"X" || ao._args[1] == L"A" || ao._args[1] == L"Y") &&
			(aon1._args.size() < 2 || (aon1._args.size() == 2 && aon1._args[1] != ao._args[0])) &&
			(aon1._type == AOT::AOT_LABEL || n1_arithm_op)
			)
		{
			// -LDW (0x1, SP), <reg1>
			// <label>, NEGW <reg> or LD <reg>, smth
			// LDW (0x1, SP), <reg1>
			if(!ao._volatile)
			{
				cs.erase(i);
				i = next1;

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"LD" && ao._args[0] == L"A" && aon2._op == L"LD" && ao._args[1] == aon2._args[0]) &&
				(
				((aon1._op == L"ADD" || aon1._op == L"SUB") && (aon1._args[1] == L"0x1" || (aon1._args[1] == L"0x2" && !ao._volatile && !aon2._volatile))) ||
				((aon1._op == L"INC" || aon1._op == L"DEC" || aon1._op == L"NEG" || aon1._op == L"CPL" || aon1._op == L"SRL" || aon1._op == L"SRA" || aon1._op == L"SLL" || aon1._op == L"SLA") && aon1._args[0] == L"A") ||
				((aon1._op == L"AND" || aon1._op == L"OR" || aon1._op == L"XOR") && ao._args[1][0] == L'(' && ao._args[1][0] != L'[' && ao._args[1].find(L',') == std::wstring::npos && ao._args[1] != L"(X)" && ao._args[1] != L"(Y)")
				) &&
			!is_reg_used_after(next2, cs.cend(), L"A")
			)
		{
			// LD A, (...)
			// ADD A, 1  or INC A
			// LD (...), A
			// ->
			// INC (...)
			// or
			// LD A, (<mem>)
			// AND/OR/XOR A, 2^n
			// LD (<mem>), A
			// ->
			// BRES/BSET/BCPL (<mem>), n

			bool proceed = true;
			bool leave_next1 = false;

			if(aon1._op == L"INC")
			{
				ao._data = L"INC " + ao._args[1];
			}
			else
			if(aon1._op == L"ADD")
			{
				ao._data = L"INC " + ao._args[1];
				if(aon1._args[1] == L"0x2")
				{
					// B1_ASM_OP_STM8 constructor sets _parsed to false
					next1->reset(new B1_ASM_OP_STM8(ao));
					leave_next1 = true;
				}
			}
			else
			if(aon1._op == L"DEC")
			{
				ao._data = L"DEC " + ao._args[1];
			}
			else
			if(aon1._op == L"SUB")
			{
				ao._data = L"DEC " + ao._args[1];
				if(aon1._args[1] == L"0x2")
				{
					// B1_ASM_OP_STM8 constructor sets _parsed to false
					next1->reset(new B1_ASM_OP_STM8(ao));
					leave_next1 = true;
				}
			}
			else
			if(aon1._op == L"NEG" || aon1._op == L"CPL" || aon1._op == L"SRL" || aon1._op == L"SRA" || aon1._op == L"SLL" || aon1._op == L"SLA")
			{
				ao._data = aon1._op + L" " + ao._args[1];
			}
			else
			{
				int32_t n = 0;
				int bpos = -1;

				if(Utils::str2int32(aon1._args[1], n) == B1_RES_OK)
				{
					if(aon1._op == L"AND")
					{
						n = ~n;
					}
					n = n & 0xFF;

					for(int i = 0; i < 8; i++)
					{
						if(n % 2 == 1)
						{
							if(bpos < 0)
							{
								bpos = i;
							}
							else
							{
								bpos = -1;
								break;
							}
						}
						n >>= 1;
					}
				}

				if(bpos >= 0)
				{
					ao._data =	(aon1._op == L"AND")	? L"BRES " :
								(aon1._op == L"OR")	? L"BSET " : L"BCPL ";
					ao._data += ao._args[1] + L", " + Utils::str_tohex16(bpos);
				}
				else
				{
					proceed = false;
				}
			}

			if(proceed)
			{
				ao._parsed = false;
				if(!leave_next1)
				{
					cs.erase(next1);
				}
				cs.erase(next2);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if ((ao._op == L"LDW" && ao._args[1] == L"X") &&
			(aon1._op == L"LDW" && aon1._args[0] == L"X") &&
			(!aon2._volatile && aon2._op == L"SUBW" && aon2._args[0] == L"X") &&
			(ao._args[0] == aon2._args[1])
			)
		{
			// LDW <smth>, X
			// LDW X, <imm> | (<addr>) | (n, SP)
			// SUBW X, <smth>
			// ->
			// LDW <smth>, X
			// SUBW X, <imm> | (<addr>) | (n, SP)
			// NEGW X
			if((aon1._args[1][0] != L'[' && aon1._args[1][0] != L'(') || (aon1._args[1][0] == L'(' && (aon1._args[1].find(L",SP)") != std::wstring::npos || (!aon1._volatile && aon1._args[1].find(L",X)") == std::wstring::npos))))
			{
				aon1._data = L"SUBW X, " + aon1._args[1];
				aon1._parsed = false;
				aon2._data = L"NEGW X";
				aon2._parsed = false;

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (!ao._volatile &&
			(((ao._op == L"LD" && aon1._op == L"LD") && (aon2._op == L"ADD" || aon2._op == L"AND" || aon2._op == L"OR" || aon2._op == L"XOR")) ||
				((ao._op == L"LDW" && aon1._op == L"LDW") && (aon2._op == L"ADDW") &&
					aon1._args[1] != L"(X)" && aon1._args[1] != L"(Y)" && aon1._args[1].front() != L'[' &&
					aon1._args[1].find(L",X)") == std::wstring::npos && aon1._args[1].find(L",Y)") == std::wstring::npos)) &&
			(ao._args[0] == aon2._args[1])
			)
		{
			// LD/LDW (0x1, SP), A/X
			// LD/LDW A/X, <smth>
			// ADD/AND/OR/XOR/ADDW A/X, (0x1, SP)
			// ->
			// LD/LDW (0x1, SP), A/X
			// ADD/AND/OR/XOR/ADDW A/X, <smth>
			aon2._data = aon2._op + L" " + aon2._args[0] + L", " + aon1._args[1];
			aon2._parsed = false;
			cs.erase(next1);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if ((
			((ao._op == L"PUSH" || ao._op == L"PUSHW") && (ao._args[0] == L"A" || ao._args[0] == L"X" || ao._args[0] == L"Y")) ||
			((ao._op == L"LD" || ao._op == L"LDW") && ao._args[0].find(L",SP)") != std::wstring::npos)
			) &&
			(
			(aon1._op == L"SUB" || aon1._op == L"SUBW") && aon1._args[0] == L"SP" &&
			(aon2._op == L"LD" || aon2._op == L"LDW") && aon2._args[1].find(L",SP)") != std::wstring::npos
			) &&
			(aon2._args[0] == ao._args[ao._args.size() - 1])
			)
		{
			// PUSH(W) reg
			// SUB SP, 0x4
			// LD(W) reg, (0x5, SP)
			// ->
			// PUSH(W) reg
			// SUB SP, 0x4
			// or
			// LD(W) (0x1, SP), reg
			// SUB SP, 0x2
			// LD(W) reg, (0x3, SP)
			// ->
			// LD(W) (0x1, SP), reg
			// SUB SP, 0x2

			int32_t n;
			if(Utils::str2int32(aon1._args[1], n) == B1_RES_OK && n > 0 && n <= 255)
			{
				bool no_SP_off = true;
				int32_t off2 = -1;
				correct_SP_offset(aon2._args[1], 0, no_SP_off, &off2);

				int32_t new_off = -1;

				if(off2 > 0)
				{
					if(ao._args.size() == 1)
					{
						// the first op is PUSH/PUSHW
						new_off = n + 1;
					}
					else
					{
						int32_t offi = -1;
						correct_SP_offset(ao._args[0], 0, no_SP_off, &offi);
						if(offi > 0)
						{
							new_off = n + offi;
						}
					}

					if(off2 == new_off)
					{
						cs.erase(next2);

						update_opt_rule_usage_stat(rule_id);
						changed = true;
						continue;
					}
				}
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if( (ao._op == L"ADD" || ao._op == L"ADDW") && ao._args[0] == L"SP" && 
			(n1_arithm_op) &&
			(aon2._op == L"ADD" || aon2._op == L"ADDW") && aon2._args[0] == L"SP"
			)
		{
			// ADDW SP, N
			// AND A, (1, SP)
			// ADDW SP, M
			// ->
			// AND A, (N + 1, SP)
			// ADDW SP, N + M
			int32_t N, M;
			if(Utils::str2int32(ao._args[1], N) == B1_RES_OK && N > 0 && N <= 255 && Utils::str2int32(aon2._args[1], M) == B1_RES_OK && M > 0 && M <= 255 && (N + 1) <= 255 && (N + M) <= 255)
			{
				bool no_SP_off = true;
				int32_t off = 0;
				if(!correct_SP_offset(aon1._args[0], 0, no_SP_off, &off).empty())
				{
					aon1._data = aon1._op + L" (" + Utils::str_tohex16(N + 1) + L",SP)";
					if(aon1._args.size() == 2)
					{
						aon1._data += L", " + aon1._args[1];
					}
					aon1._parsed = false;
					aon2._data = L"ADDW SP, " + Utils::str_tohex16(N + M);
					aon2._parsed = false;
					cs.erase(i);
					i = next1;

					update_opt_rule_usage_stat(rule_id);
					changed = true;
					continue;
				}
				else
				if(no_SP_off && ao._args.size() == 2 && !correct_SP_offset(aon1._args[1], 0, no_SP_off, &off).empty())
				{
					aon1._data = aon1._op + L" " + aon1._args[0] + L", (" + Utils::str_tohex16(N + 1) + L",SP)";
					aon1._parsed = false;
					aon2._data = L"ADDW SP, " + Utils::str_tohex16(N + M);
					aon2._parsed = false;
					cs.erase(i);
					i = next1;

					update_opt_rule_usage_stat(rule_id);
					changed = true;
					continue;
				}
			}
		}


		auto next3 = std::next(next2);
		if(next3 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon3 = *static_cast<B1_ASM_OP_STM8 *>(next3->get());
		if(aon3._is_inline || !aon3.Parse())
		{
			i++;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (
			((ao._op == L"CLRW" || ao._op == L"LDW") && ao._args[0] == L"X") &&
			(aon1._op == L"LDW" && aon1._args[1] == L"X") &&
			(aon2._op == L"SUBW" && aon2._args[0] == L"X") &&
			(aon3._op == L"NEGW" && aon3._args[0] == L"X")
			)
		{
			// CLRW X or LDW X, <imm1>
			// LDW <smth>, X
			// SUBW X, <imm2>
			// NEGW X
			// ->
			// CLRW X or LDW X, <imm1>
			// LDW <smth>, X
			// LDW X, <imm2> - <imm1>
			// or
			// CLRW X
			// LDW <smth1>, X
			// SUBW X, <smth2>
			// NEGW X
			// ->
			// CLRW X
			// LDW <smth1>, X
			// LDW X, <smth2>
			
			int32_t n1 = 0, n2;

			bool proceed = (ao._op == L"CLRW" || Utils::str2int32(ao._args[1], n1) == B1_RES_OK) && (Utils::str2int32(aon2._args[1], n2) == B1_RES_OK);

			if(proceed || ao._op == L"CLRW")
			{
				aon2._data = L"LDW X, " + (proceed ? Utils::str_tohex16(n2 - n1) : aon2._args[1]);
				aon2._parsed = false;
				cs.erase(next3);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (
			((((ao._op == L"PUSH" && aon1._op == L"LD") && (aon2._op == L"ADD" || aon2._op == L"AND" || aon2._op == L"OR" || aon2._op == L"XOR" || aon2._op == L"SUB")) &&
			(aon2._args[1] == L"(0x1,SP)") &&
			(ao._args[0] == L"A" && aon1._args[0] == L"A" && aon2._args[0] == L"A"))) ||

			((ao._op == L"PUSHW" && aon1._op == L"LDW" && (aon2._op == L"ADDW" || aon2._op == L"SUBW")) &&
			(ao._args[0] == L"X" && aon1._args[0] == L"X" && aon2._args[0] == L"X" && aon2._args[1] == L"(0x1,SP)") &&
			(aon1._args[1][0] != L'[' && aon1._args[1].find(L",X)") == std::wstring::npos && aon1._args[1] != L"(X)"))
			)
		{
			// PUSH A
			// LD A, (0x3, SP)
			// AND/ADD/OR/XOR A, (1, SP)
			// ADDW SP, 1
			// ->
			// AND/ADD/OR/XOR A, (2, SP)
			// or
			// PUSHW X
			// LDW X, 100
			// ADDW X, (0x1, SP)
			// ADDW SP, 2
			// ->
			// ADDW X, 100
			// or
			// PUSHW X
			// LDW X, 100
			// SUBW X, (0x1, SP)
			// ADDW SP, 2
			// ->
			// NEGW X
			// ADDW X, 100

			int32_t data_size = 2;
			std::wstring reg = L"X";
			std::wstring sub_op = L"SUBW";
			std::wstring new_op = L"ADDW";
			std::wstring neg_op = L"NEGW X";

			if(ao._op == L"PUSH")
			{
				data_size = 1;
				reg = L"A";
				sub_op = L"SUB";
				new_op = L"ADD";
				neg_op = L"NEG A";
			}

			std::wstring new_arg;
			int32_t n = -1;
			bool remove_push = false;

			if((aon3._op == L"ADD" || aon3._op == L"ADDW") && aon3._args[0] == L"SP")
			{
				if(Utils::str2int32(aon3._args[1], n) == B1_RES_OK)
				{
					if(n > (data_size - 1) && n <= 255)
					{
						remove_push = true;
					}
				}
			}

			if(remove_push && aon1._args[1].find(L",SP)") != std::wstring::npos)
			{
				bool no_SP_off;
				new_arg = correct_SP_offset(aon1._args[1], data_size, no_SP_off);
			}
			else
			{
				new_arg = aon1._args[1];
			}

			if(!new_arg.empty())
			{
				if(aon2._op == sub_op)
				{
					aon1._data = neg_op;
					aon1._parsed = false;
				}
				else
				{
					new_op = aon2._op;
					cs.erase(next1);
				}
				aon2._data = new_op + L" " + reg + L", " + new_arg;
				aon2._parsed = false;

				if(remove_push)
				{
					cs.erase(i);
					i = next2;

					if(n == data_size)
					{
						cs.erase(next3);
					}
					else
					{
						aon3._data = L"ADDW SP, " + Utils::str_tohex16(n - data_size);
						aon3._parsed = false;
					}
				}

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			((ao._op == L"LDW" || ao._op == L"LD") && (ao._op == aon2._op) && (ao._args[0] == aon2._args[1]) && (ao._args[1] == aon2._args[0])) &&
			((aon1._op == L"LDW" || aon1._op == L"LD") && (aon1._op == aon3._op) && (aon1._args[0] == aon3._args[1]) && (aon1._args[1] == aon3._args[0])) &&
			(ao._args[0].find(L",X)") == std::wstring::npos && ao._args[0] != L"(X)" && ao._args[0].find(L",Y)") == std::wstring::npos && ao._args[0] != L"(Y)") &&
			(aon1._args[0].find(L",X)") == std::wstring::npos && aon1._args[0] != L"(X)" && aon1._args[0].find(L",Y)") == std::wstring::npos && aon1._args[0] != L"(Y)")
			)
		{
			// LDW (0x1, SP), Y
			// LDW (0x1 + 2, SP), X
			// LDW Y, (0x1, SP)
			// LDW X, (0x3, SP)
			// ->
			// LDW (0x1, SP), Y
			// LDW (0x1 + 2, SP), X

			if(!aon2._volatile)
			{
				cs.erase(next2);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
			if(!aon3._volatile)
			{
				cs.erase(next3);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		int n3_size = 0;
		bool n3_arithm_op = is_arithm_op(aon3, n3_size);

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (
			((aon3._op == L"ADD" || aon3._op == L"ADDW") && aon3._args[0] == L"SP" && (ao._op == L"PUSH" || ao._op == L"PUSHW")) &&
			(n1_arithm_op && n2_arithm_op)
			)
		{
			// PUSH A or PUSHW X/Y
			// AND A, 0x20
			// LD (4096), A
			// ADDW SP, 0x1 or ADDW SP, 0x2
			// ->
			// AND A, 0x20
			// LD (4096), A

			int32_t size = (ao._op == L"PUSH") ? 1 : 2;

			int32_t n;
			if(Utils::str2int32(aon3._args[1], n) == B1_RES_OK && n >= size)
			{
				bool no_SP_off_n1_0 = true;
				auto new_off_n1_0 = correct_SP_offset(aon1._args[0], size, no_SP_off_n1_0);
				bool no_SP_off_n1_1 = true;
				auto new_off_n1_1 = (aon1._args.size() > 1) ? correct_SP_offset(aon1._args[1], size, no_SP_off_n1_1) : std::wstring();

				bool no_SP_off_n2_0 = true;
				auto new_off_n2_0 = correct_SP_offset(aon2._args[0], size, no_SP_off_n2_0);
				bool no_SP_off_n2_1 = true;
				auto new_off_n2_1 = (aon2._args.size() > 1) ? correct_SP_offset(aon2._args[1], size, no_SP_off_n2_1) : std::wstring();

				if(!((!no_SP_off_n1_0 && new_off_n1_0.empty()) || (!no_SP_off_n1_1 && new_off_n1_1.empty()) || (!no_SP_off_n2_0 && new_off_n2_0.empty()) || (!no_SP_off_n2_1 && new_off_n2_1.empty())))
				{
					if(!new_off_n1_0.empty() || !new_off_n1_1.empty())
					{
						new_off_n1_0 = new_off_n1_0.empty() ? aon1._args[0] : new_off_n1_0;
						aon1._data = aon1._op + L" " + new_off_n1_0;
						if(aon1._args.size() > 1)
						{
							new_off_n1_1 = new_off_n1_1.empty() ? aon1._args[1] : new_off_n1_1;
							aon1._data += L", " + new_off_n1_1;
						}
						aon1._parsed = false;
					}

					if(!new_off_n2_0.empty() || !new_off_n2_1.empty())
					{
						new_off_n2_0 = new_off_n2_0.empty() ? aon2._args[0] : new_off_n2_0;
						aon2._data = aon2._op + L" " + new_off_n2_0;
						if(aon2._args.size() > 1)
						{
							new_off_n2_1 = new_off_n2_1.empty() ? aon2._args[1] : new_off_n2_1;
							aon2._data += L", " + new_off_n2_1;
						}
						aon2._parsed = false;
					}

					cs.erase(i);
					
					if(n == size)
					{
						cs.erase(next3);
					}
					else
					{
						aon3._data = L"ADDW SP, " + Utils::str_tohex16(n - size);
						aon3._parsed = false;
					}
					
					i = next1;

					update_opt_rule_usage_stat(rule_id);
					changed = true;
					continue;
				}
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"LDW" && aon1._op == L"LDW" && aon2._op == L"LDW" && aon3._op == L"LDW") &&
			(ao._args[0] == L"X" && aon1._args[0] == L"Y" && aon1._args[1] == L"X" && aon2._args[0] == L"X" && aon3._args[1] == L"Y") &&
			(aon2._args[1].front() == L'(' && aon2._args[1].find(L",X)") == std::wstring::npos && aon2._args[1].find(L",SP)") == std::wstring::npos) &&
			(aon3._args[0].size() >= 3 && aon3._args[0][1] != L'[' && (aon3._args[0] == L"(X)" || aon3._args[0].find(L",X)") != std::wstring::npos))
			)
		{
			// LDW X, <smth>
			// LDW Y, X
			// LDW X, (NS1::__VAR_C)
			// LDW (0x4, X), Y or LDW (X), Y
			// ->
			// LDW X, 0x4
			// LDW Y, <smth>
			// LDW ([NS1::__VAR_C], X), Y
			// or
			// LDW X, <smth>
			// LDW [NS1::__VAR_C], X

			const auto smth = ao._args[1];
			const auto addr = aon2._args[1].substr(1, aon2._args[1].length() - 2);

			if(aon3._args[0] == L"(X)")
			{
				aon1._data = L"LDW [" + addr + L"], X";
				aon1._parsed = false;
				cs.erase(next2);
				cs.erase(next3);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
			else
			{
				// no LDW Y, (X) and LDW Y, (offset, X) instructions
				if(smth != L"(X)" && smth.find(L",X)") == std::wstring::npos)
				{
					ao._data = L"LDW X, " + aon3._args[0].substr(1, aon3._args[0].length() - 4);
					ao._parsed = false;
					aon1._data = L"LDW Y, " + smth;
					aon1._parsed = false;
					aon2._data = L"LDW ([" + addr + L"], X), Y";
					aon2._parsed = false;
					cs.erase(next3);

					update_opt_rule_usage_stat(rule_id);
					changed = true;
					continue;
				}
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"POPW" && aon1._op == L"LDW" && aon2._op == L"LDW" && aon3._op == L"LDW") &&
			(ao._args[0] == L"X" && aon1._args[0] == L"X" && aon1._args[1] == L"(X)" && aon2._args[0] == L"Y" && aon2._args[1] == L"X" && aon3._args[0] == L"X")
			)
		{
			// POPW X
			// LDW X, (X)
			// LDW Y, X
			// LDW X, <smth>
			// ->
			// POPW Y
			// LDW Y, (Y)
			// LDW X, <smth>
			ao._data = L"POPW Y";
			ao._parsed = false;
			aon1._data = L"LDW Y, (Y)";
			aon1._parsed = false;
			cs.erase(next2);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"POPW" && aon1._op == L"LDW" && aon2._op == L"LDW" && aon3._op == L"LDW") &&
			(ao._args[0] == L"Y" && aon1._args[0] == L"X" && aon1._args[1].front() == L'(' && aon1._args[1][1] != L'[' &&
				aon1._args[1].find(L",X)") == std::wstring::npos && aon1._args[1].find(L",SP)") == std::wstring::npos && aon2._args[0] == L"(X)" && aon2._args[1] == L"Y" && aon3._args[0] == L"X")
			)
		{
			// POPW Y
			// LDW X, (NS1::__VAR_ARR)
			// LDW (X), Y
			// LDW X, <smth>
			// ->
			// POPW X
			// LDW [NS1::__VAR_ARR], X
			// LDW X, <smth>
			ao._data = L"POPW X";
			ao._parsed = false;
			aon1._data = L"LDW [" + aon1._args[1].substr(1, aon1._args[1].length() - 2) + L"], X";
			aon1._parsed = false;
			cs.erase(next2);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"PUSH" && ao._args[0] == L"A") &&
			(aon1._op == L"LD" && aon1._args[1] == L"A" && (aon1._args[0].front() == L'(' || aon1._args[0].front() == L'[') && aon1._args[0].find(L",SP)") == std::wstring::npos) &&
			(aon2._op == L"LD" && aon2._args[0] == L"A" && aon2._args[1] == L"(0x1,SP)") &&
			((aon3._op == L"ADD" || aon3._op == L"ADDW") && aon3._args[0] == L"SP" && aon3._args[1] == L"0x1")
			)
		{
			// PUSH A
			// LD (smth), A
			// LD A, (0x1, SP)
			// ADDW SP, 1
			// ->
			// LD (smth), A
			cs.erase(next2);
			cs.erase(next3);
			cs.erase(i);
			i = next1;

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"LDW" && ao._args[0] == L"X" && ao._args[1] != L"Y" && ao._args[1] != L"(X)" && ao._args[1].find(L",X)") == std::wstring::npos) &&
			(aon1._op == L"PUSHW" && aon1._args[0] == L"X") &&
			(aon2._op == L"LDW" && aon2._args[0] == L"X" && aon2._args[1] != L"(X)" && aon2._args[1].find(L",X)") == std::wstring::npos && aon2._args[1].find(L",SP)") == std::wstring::npos) &&
			(aon3._op == L"POPW" && aon3._args[0] == L"Y")
			)
		{
			// LDW X, smth1 not depending on X
			// PUSHW X
			// LDW X, smth2 not depending on X, Y or SP
			// POPW Y
			// ->
			// LDW Y, smth1
			// LDW X, smth2
			ao._data = L"LDW Y, " + ao._args[1];
			ao._parsed = false;
			aon2._data = L"LDW X, " + aon2._args[1];
			aon2._parsed = false;
			cs.erase(next1);
			cs.erase(next3);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"PUSHW" && aon1._op == L"LD" && aon1._args[0] == L"A" && aon1._args[1] == L"(0x2,SP)") &&
			((aon2._op == L"ADD" || aon2._op == L"ADDW") && aon2._args[0] == L"SP") &&
			(n3_arithm_op || aon3._op == L"RET" || aon3._op == L"RETF")
			)
		{
			// PUSHW X/Y
			// LD A, (2, SP)
			// ADDW SP, N (N >= 2)
			// smth changing N and Z flags, CALL or RET
			// ->
			// LD A, XL/YL
			// [ADDW SP, N - 2]
			// smth changing N and Z flags, CALL or RET
			int32_t N;

			if(Utils::str2int32(aon2._args[1], N) == B1_RES_OK && N >= 2 && N <= 255)
			{
				aon1._data = L"LD A, " + ao._args[0] + L"L";
				aon1._parsed = false;
				if(N == 2)
				{
					cs.erase(next2);
				}
				else
				{
					aon2._data = L"ADDW SP, " + Utils::str_tohex16(N - 2);
					aon2._parsed = false;
				}
				cs.erase(i);
				i = next1;

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		i++;
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::Optimize3(bool &changed)
{
	auto &cs = *_code_secs.begin();
	auto i = cs.begin();

	while(i != cs.end())
	{
		int32_t rule_id = 0x30000;

		auto &ao = *static_cast<B1_ASM_OP_STM8 *>(i->get());

		if(ao._type == AOT::AOT_LABEL)
		{
			_opt_labels[ao._data] = i;
			i++;
			continue;
		}

		if(ao._is_inline || !ao.Parse())
		{
			i++;
			continue;
		}


		auto next1 = std::next(i);
		if(next1 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon1 = *static_cast<B1_ASM_OP_STM8 *>(next1->get());
		if(aon1._is_inline || !aon1.Parse())
		{
			i++;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (
			(ao._op == L"ADD" || ao._op == L"ADDW" || ao._op == L"SUB" || ao._op == L"SUBW") && ao._args[0] == L"SP" &&
			((aon1._op == L"LD" || aon1._op == L"LDW") && aon1._args[0] == L"(0x1,SP)" && (aon1._args[1] == L"A" || aon1._args[1] == L"X"))
			)
		{
			// ADDW SP, N
			// LDW (1, SP), X
			// ->
			// ADDW SP, N + 2
			// PUSHW X
			int32_t n, n1 = (aon1._op == L"LD") ? 1 : 2;
			if(Utils::str2int32(ao._args[1], n) == B1_RES_OK)
			{
				n += (ao._op == L"ADD" || ao._op == L"ADDW") ? n1 : -n1;

				if(n > 0 && n <= 255)
				{
					bool proceed = true;

					for(auto next = std::next(next1); next != cs.cend(); next++)
					{
						auto next_ao = static_cast<B1_ASM_OP_STM8*>(next->get());
						if(next_ao->_type == AOT::AOT_LABEL)
						{
							break;
						}

						if(!next_ao->Parse())
						{
							proceed = false;
							break;
						}

						if(next_ao->_op == L"CALL" || next_ao->_op == L"CALLR" || next_ao->_op == L"CALLF" || next_ao->_op == L"LD" || next_ao->_op == L"LDW")
						{
							break;
						}

						if(next_ao->_op == L"PUSH" || next_ao->_op == L"PUSHW" || next_ao->_op == L"LD" || next_ao->_op == L"LDW")
						{
							continue;
						}

						break;
					}

					if(proceed)
					{
						ao._data = ao._op + L" SP, " + std::to_wstring(n);
						ao._parsed = false;
						aon1._data = ((aon1._op == L"LD") ? L"PUSH " : L"PUSHW ") + aon1._args[1];
						aon1._parsed = false;

						update_opt_rule_usage_stat(rule_id);
						changed = true;
						continue;
					}
				}
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (ao._op == L"CLRW" && ao._args[0] == L"X" &&
			aon1._op == L"ADDW" && aon1._args[0] == L"X"
			)
		{
			// CLRW X
			// ADDW X, <smth>
			// ->
			// LDW X, <smth>
			aon1._data = L"LDW X, " + aon1._args[1];
			aon1._parsed = false;
			cs.erase(i);
			i = next1;

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (
			((ao._op == L"CLRW" || ao._op == L"LDW") && ao._args[0] == L"X") &&
			((aon1._op == L"SLLW" || aon1._op == L"SLAW") && aon1._args[0] == L"X")
			)
		{
			// CLRW X or LDW X, <imm>
			// SLLW X
			// ->
			// CLRW X
			// or 
			// LDW X, <imm> * 2
			int32_t n = 0;

			bool proceed = (ao._op == L"CLRW" || (Utils::str2int32(ao._args[1], n) == B1_RES_OK && n > 0));

			if(proceed)
			{
				if(ao._op != L"CLRW")
				{
					ao._data = L"LDW X, " + Utils::str_tohex16(n * 2);
					ao._parsed = false;
				}
				cs.erase(next1);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		bool i_arg0_SP_based = (ao._args.size() != 0 && ao._args[0].find(L",SP)") != std::wstring::npos);
		bool i_arg1_SP_based = (ao._args.size() > 1 && ao._args[1].find(L",SP)") != std::wstring::npos);

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(ao._op == L"LD" || ao._op == L"LDW")
		{
			// LD A, smth or LD smth, A
			// [ADD/SUB SP, N]
			// [JRXX]
			// [CP/CPW]
			// [PUSH/PUSHW]
			// -LD smth, A or LD A, smth
			// CALL/RET/arithm. op.
			// or
			// LD A, smth or LD smth, A
			// [ADD/SUB SP, N]
			// [JRXX]
			// [CP/CPW]
			// [PUSH/PUSHW]
			// LD smth, A or LD A, smth -> TNZ A
			// JREQ/JRNE

			bool proceed = false;
			bool tnz = false;
			auto next = next1;
			for(; next != cs.end(); next++)
			{
				auto next_ao = static_cast<B1_ASM_OP_STM8 *>(next->get());
				if(next_ao->_type == AOT::AOT_LABEL)
				{
					break;
				}
				if(next_ao->_is_inline || !next_ao->Parse())
				{
					break;
				}

				if(!i_arg0_SP_based && !i_arg1_SP_based && (((next_ao->_op == L"SUB" || next_ao->_op == L"SUBW" || next_ao->_op == L"ADD" || next_ao->_op == L"ADDW") && next_ao->_args[0] == L"SP") || (next_ao->_op == L"PUSH" || next_ao->_op == L"PUSHW")))
				{
					continue;
				}
				if(ao._op == L"LDW" && next_ao->_op == L"LD" && next_ao->_args[0] == L"A")
				{
					continue;
				}
				if(next_ao->_op[0] == L'J')
				{
					continue;
				}
				if(next_ao->_op == L"CP" || next_ao->_op == L"CPW" || next_ao->_op == L"TNZ" || next_ao->_op == L"TNZW")
				{
					continue;
				}

				if(!next_ao->_volatile && next_ao->_op == ao._op && ((next_ao->_args[0] == ao._args[1] && next_ao->_args[1] == ao._args[0]) || (next_ao->_args[0] == ao._args[0] && next_ao->_args[1] == ao._args[1])))
				{
					// the next instruction must not read N and Z flags
					auto nextn = std::next(next);

					while(true)
					{
						if(nextn == cs.end())
						{
							proceed = true;
							break;
						}

						next_ao = static_cast<B1_ASM_OP_STM8*>(nextn->get());
						if(next_ao->_type == AOT::AOT_LABEL)
						{
							proceed = true;
							break;
						}

						if(!next_ao->Parse())
						{
							break;
						}

						if(next_ao->_op == L"PUSH" || next_ao->_op == L"PUSHW")
						{
							nextn = std::next(nextn);
							continue;
						}

						if(next_ao->_op == L"JREQ" || next_ao->_op == L"JRNE")
						{
							proceed = true;
							tnz = true;
							break;
						}

						int32_t size = 0;
						if (next_ao->_op == L"CALL" || next_ao->_op == L"CALLR" || next_ao->_op == L"CALLF" ||
							next_ao->_op == L"JRA" || next_ao->_op == L"JP" || next_ao->_op == L"JPF" || next_ao->_op == L"RET" || next_ao->_op == L"RETF" || next_ao->_op == L"IRET" ||
							next_ao->_op == L"CP" || next_ao->_op == L"CPW" || next_ao->_op == L"TNZ" || next_ao->_op == L"TNZW" ||
							is_arithm_op(*next_ao, size))
						{
							proceed = true;
							break;
						}

						break;
					}

					break;
				}

				break;
			}

			if(proceed)
			{
				if(tnz)
				{
					auto next_ao = static_cast<B1_ASM_OP_STM8 *>(next->get());
					next_ao->_data = (ao._op == L"LD") ? L"TNZ A" : L"TNZW X";
					next_ao->_parsed = false;
				}
				else
				{
					cs.erase(next);
				}

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(	!ao._volatile &&
			((ao._op == L"LD" && ao._args[1] == L"A" && !(ao._args[0][0] == L'X' || ao._args[0][0] == L'Y')) || (ao._op == L"LDW" && (ao._args[1] == L"X" || ao._args[1] == L"Y") && !(ao._args[0] == L"X" || ao._args[0] == L"Y" || ao._args[0] == L"SP"))) &&
			ao._args[0] != L"(X)" && ao._args[0] != L"(Y)" && ao._args[0].find(L",X)") == std::wstring::npos && ao._args[0].find(L",Y)") == std::wstring::npos
			)
		{
			// -LD (smth), A
			// (smth) is not read here
			// LD (smth), A

			bool proceed = false;
			auto next = next1;
			for(; next != cs.end(); next++)
			{
				auto next_ao = static_cast<B1_ASM_OP_STM8 *>(next->get());
				if(next_ao->_is_inline || !next_ao->Parse())
				{
					break;
				}

				if(ao._op == next_ao->_op && ao._args == next_ao->_args)
				{
					proceed = true;
					break;
				}

				if(	next_ao->_op[0] == L'J' || next_ao->_op == L"CALL" || next_ao->_op == L"CALLR" || next_ao->_op == L"CALLF" || next_ao->_op == L"RET" || next_ao->_op == L"RETF" ||
					next_ao->_op == L"IRET" || next_ao->_op == L"BTJF" || next_ao->_op == L"BTJT")
				{
					break;
				}

				if(i_arg0_SP_based && (next_ao->_op == L"PUSH" || next_ao->_op == L"PUSHW" || next_ao->_op == L"POP" || next_ao->_op == L"POPW"))
				{
					break;
				}

				if(next_ao->_op == L"BCPL" || next_ao->_op == L"BRES" || next_ao->_op == L"BSET" || next_ao->_op == L"BCCM")
				{
					continue;
				}

				// these instructions can read memory or stack
				bool next_1arg_op = (next_ao->_op == L"CLR" || next_ao->_op == L"CPL" || next_ao->_op == L"DEC" || next_ao->_op == L"INC" || next_ao->_op == L"NEG" || next_ao->_op == L"RLC" || next_ao->_op == L"RRC" ||
					next_ao->_op == L"SLL" || next_ao->_op == L"SLA" || next_ao->_op == L"SRA" || next_ao->_op == L"SRL" || next_ao->_op == L"SWAP" || next_ao->_op == L"TNZ");

				if(i_arg0_SP_based)
				{
					if(next_1arg_op)
					{
						if(next_ao->_args[0].find(L",SP)") != std::wstring::npos)
						{
							break;
						}
					}
					else
					if(next_ao->_args.size() > 1 && next_ao->_args[1].find(L",SP)") != std::wstring::npos)
					{
						break;
					}
				}
				else
				{
					if(next_1arg_op)
					{
						if(next_ao->_args[0][0] == L'(' || next_ao->_args[0][0] == L'[')
						{
							break;
						}
					}
					else
					if(	(next_ao->_args.size() > 0 && (next_ao->_args[0][0] == L'[')) ||
						(next_ao->_args.size() > 1 && ((next_ao->_args[1][0] == L'(' && next_ao->_args[1].find(L",SP)") == std::wstring::npos) || next_ao->_args[1][0] == L'['))
						)
					{
						break;
					}
				}
			}

			if(proceed)
			{
				cs.erase(i);
				i = next1;

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(ao._op == L"PUSHW" && ao._args[0] == L"X")
		{
			// PUSHW X
			// ...       <- these ops should not use Y register and stack
			// POPW Y
			// ->
			// LDW Y, X
			// ...

			bool proceed = false;
			auto next = next1;
			for (; next != cs.end(); next++)
			{
				auto next_ao = static_cast<B1_ASM_OP_STM8 *>(next->get());
				if(next_ao->_type == AOT::AOT_LABEL)
				{
					break;
				}
				if(next_ao->_is_inline || !next_ao->Parse())
				{
					break;
				}

				if(next_ao->_op == L"POPW" && next_ao->_args[0] == L"Y")
				{
					proceed = true;
					break;
				}

				if(	next_ao->_op == L"POPW" || next_ao->_op == L"POP" || next_ao->_op == L"PUSHW" || next_ao->_op == L"PUSH" ||
					next_ao->_op == L"CALLR" || next_ao->_op == L"CALL" || next_ao->_op == L"CALLF" || next_ao->_op == L"RET" || next_ao->_op == L"RETF" || next_ao->_op == L"IRET")
				{
					break;
				}

				int n_size = 0;
				bool n_uses_SP = false;
				is_arithm_op(*next_ao, n_size, &n_uses_SP);
				if(n_uses_SP)
				{
					break;
				}

				bool write_op = false;
				if(is_reg_used(*next_ao, L"Y", write_op) || write_op)
				{
					break;
				}
			}

			if(proceed)
			{
				ao._data = L"LDW Y, X";
				ao._parsed = false;
				cs.erase(next);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}


		auto next2 = std::next(next1);
		if(next2 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon2 = *static_cast<B1_ASM_OP_STM8 *>(next2->get());
		if(aon2._is_inline || !aon2.Parse())
		{
			i++;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			((ao._op == L"CLRW" || (ao._op == L"LDW" && ao._args[1] != L"(X)" && ao._args[1].find(L",X)") == std::wstring::npos)) && ao._args[0] == L"X") &&
			(aon1._op == L"LDW" && aon1._args[0] == L"Y" && aon1._args[1] == L"X") &&
			(aon2._op == L"LDW" && aon2._args[1] != L"(X)" && aon2._args[1].find(L",X)") == std::wstring::npos && aon2._args[0] == L"X")
			)
		{
			// LDW X, smth1
			// LDW Y, X
			// LDW X, smth2
			// ->
			// LDW Y, smth1
			// LDW X, smth2
			if(!ao._volatile)
			{
				if(ao._op == L"CLRW")
				{
					ao._data = L"CLRW Y";
				}
				else
				{
					ao._data = L"LDW Y, " + ao._args[1];
				}
				ao._parsed = false;
				cs.erase(next1);

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if ((ao._op == L"LDW" && aon1._op == L"POPW" && aon2._op == L"POPW") &&
			(ao._args[1] == aon1._args[0] && aon1._args[0] == aon2._args[0]) && ao._args[0] == L"(0x3,SP)")
		{
			// LDW (3, SP), X
			// POPW X
			// POPW X
			// ->
			// ADDW SP, 4
			ao._data = L"ADDW SP, 4";
			ao._parsed = false;
			cs.erase(next2);
			cs.erase(next1);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if ((ao._op == L"CP" || ao._op == L"CPW") && (aon2._op == ao._op) && (ao._args == aon2._args) && (aon1._op.length() > 2 && aon1._op.substr(0, 2) == L"JR"))
		{
			// CPW X, <smth>
			// JRXXX <label>
			// -CPW X, <smth>
			cs.erase(next2);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (ao._op == L"CLRW" && aon1._op == L"LD" && aon1._args[0] == ao._args[0] + L"L" && aon2._op == L"PUSHW" && aon2._args[0] == ao._args[0] &&
			!is_reg_used_after(next2, cs.cend(), ao._args[0]))
		{
			// CLRW X or Y
			// LD XL or YL, A
			// PUSHW X or Y
			// ->
			// PUSH A
			// PUSH 0
			ao._data = L"PUSH A";
			ao._parsed = false;
			aon1._data = L"PUSH 0";
			aon1._parsed = false;
			cs.erase(next2);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if((ao._op == L"ADDW" || ao._op == L"ADD") && ao._args[0] == L"SP" && ao._args[1] == L"0x2")
		{
			// ADDW SP, 2
			// LDW X, smth
			// CALL fn
			// PUSHW X
			// ->
			// LDW X, smth
			// CALL fn
			// LDW (1, SP), X
			bool proceed = false;
			auto aon = &aon1;
			auto next = next1;

			while(true)
			{
				if(
					((aon->_op == L"LDW" || aon->_op == L"ADDW") && !(aon->_args[0] == L"SP" || aon->_args[1] == L"SP" || aon->_args[0].find(L",SP)") != std::wstring::npos || aon->_args[1].find(L",SP)") != std::wstring::npos)) ||
					aon->_op == L"CALL" || aon->_op == L"CALLR" || aon->_op == L"CALLF" || aon->_op == L"SLAW"
					)
				{
					next = std::next(next);
					if(next == cs.cend())
					{
						break;
					}
					aon = static_cast<B1_ASM_OP_STM8 *>(next->get());
					if(aon->_is_inline || !aon->Parse())
					{
						break;
					}
					continue;
				}

				if(aon->_op == L"PUSHW")
				{
					auto nextn = std::next(next);
					if(nextn == cs.cend())
					{
						proceed = true;
					}
					else
					{
						auto aonn = static_cast<B1_ASM_OP_STM8 *>(nextn->get());
						if(aonn->_is_inline || !aon->Parse())
						{
							break;
						}

						int size = 0;
						if(aonn->_op == L"CALL" || aonn->_op == L"CALLR" || aonn->_op == L"CALLF" || aonn->_op == L"RET" ||aonn->_op == L"RETF" || aonn->_op == L"IRET" || is_arithm_op(*aonn, size))
						{
							proceed = true;
							break;
						}
					}
				}

				break;
			}

			if(proceed)
			{
				aon->_data = L"LDW (1, SP), " + aon->_args[0];
				aon->_parsed = false;
				cs.erase(i);
				i = next1;

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}


		auto next3 = std::next(next2);
		if(next3 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon3 = *static_cast<B1_ASM_OP_STM8 *>(next3->get());
		if(aon3._is_inline || !aon3.Parse())
		{
			i++;
			continue;
		}

		int n3_size = 0;
		bool n3_arithm_op = is_arithm_op(aon3, n3_size);

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if ((ao._op == L"ADD" || ao._op == L"ADDW") && ao._args[0] == L"SP" &&
			aon1._op == L"LDW" && aon1._args[0] == L"(0x1,SP)" && aon1._args[1] == L"Y" &&
			aon2._op == L"LDW" && aon2._args[0] == L"(0x3,SP)" && aon2._args[1] == L"X" &&
			(n3_arithm_op || aon3._op == L"CALL" || aon3._op == L"CALLR" || aon3._op == L"CALLF")
			)
		{
			// ADDW SP, N
			// LDW (0x1, SP), Y
			// LDW (0x3, SP), X
			// ->
			// ADDW SP, N + 4
			// PUSHW X
			// PUSHW Y
			int32_t n;
			if(Utils::str2int32(ao._args[1], n) == B1_RES_OK)
			{
				ao._data = L"ADDW SP, " + std::to_wstring(n + 4);
				ao._parsed = false;
				aon1._data = L"PUSHW X";
				aon1._parsed = false;
				aon2._data = L"PUSHW Y";
				aon2._parsed = false;

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(	ao._op == L"PUSHW" && ao._args[0] == L"X" && aon1._op == L"PUSHW" && aon1._args[0] == L"Y" && aon2._op == L"LDW" && aon2._args[0] == L"X" && aon2._args[1] == L"(0x3,SP)" &&
			(aon3._op == L"ADD" || aon3._op == L"ADDW") && aon3._args[0] == L"SP")
		{
			// PUSHW X
			// PUSHW Y
			// LDW X, (0x3, SP)
			// ADDW SP, 4
			// ->
			// PUSHW X
			// PUSHW Y
			// ADDW SP, 4

			int32_t n = 0;
			if(Utils::str2int32(aon3._args[1], n) == B1_RES_OK && n >= 4)
			{
				cs.erase(next2);
				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if((ao._op == L"SUB" || ao._op == L"SUBW") && ao._args[0] == L"SP" && (ao._args[1] == L"0x1" || ao._args[1] == L"0x2" || ao._args[1] == L"0x4"))
		{
			// SUB SP, 1 / 2 / 4													(1) remove
			// [some LD A, <smth> / LDW X, <smth> / LDW Y, <smth> / PUSH / PUSHW]	(2) correct SP offset
			// one or several CALL(R, F) fn
			// [ADDW SP, <n>]
			// LD (1, SP), A / LDW (1, SP), X / LDW (1, SP), Y + LDW (3, SP), X		(3) change to PUSH / PUSHW
			// ->
			// [some LD A, <smth> / LDW X, <smth> / LDW Y, <smth> / PUSH / PUSHW]	(2)
			// one or several CALL(R, F) fn
			// [ADDW SP, <n>]
			// PUSH A / PUSHW X / PUSHW X + PUSHW Y									(3)

			bool proceed = true;

			// skip optional arguments passing
			auto call = next1;
			for(; call != cs.end(); call++)
			{
				auto call_ao = static_cast<B1_ASM_OP_STM8 *>(call->get());

				if(call_ao->_type == AOT::AOT_LABEL)
				{
					proceed = false;
					break;
				}

				if(call_ao->_is_inline || !call_ao->Parse())
				{
					proceed = false;
					break;
				}

				if((call_ao->_op == L"LD" && call_ao->_args[0] == L"A") || (call_ao->_op == L"LDW" && (call_ao->_args[0] == L"X" || call_ao->_args[0] == L"Y")) ||
					call_ao->_op == L"PUSH" || call_ao->_op == L"PUSHW")
				{
					continue;
				}

				if(call_ao->_op == L"CALLR" || call_ao->_op == L"CALL" || call_ao->_op == L"CALLF")
				{
					break;
				}

				proceed = false;
				break;
			}

			for(; call != cs.end(); call++)
			{
				auto call_ao = static_cast<B1_ASM_OP_STM8 *>(call->get());

				if(call_ao->_type == AOT::AOT_LABEL)
				{
					proceed = false;
					break;
				}

				if(call_ao->_is_inline || !call_ao->Parse())
				{
					proceed = false;
					break;
				}

				if(call_ao->_op == L"CALLR" || call_ao->_op == L"CALL" || call_ao->_op == L"CALLF")
				{
					continue;
				}

				call = std::prev(call);
				break;
			}

			if(proceed && call != cs.end())
			{
				auto retval = std::next(call);

				if(retval != cs.end() && (*retval)->_type != AOT::AOT_LABEL && !(*retval)->_is_inline && static_cast<B1_ASM_OP_STM8 *>(retval->get())->Parse())
				{
					auto rv_ao = static_cast<B1_ASM_OP_STM8 *>(retval->get());

					if((rv_ao->_op == L"ADD" || rv_ao->_op == L"ADDW") && rv_ao->_args[0] == L"SP")
					{
						retval = std::next(retval);
						if(retval == cs.end() || (*retval)->_type == AOT::AOT_LABEL || (*retval)->_is_inline || !static_cast<B1_ASM_OP_STM8 *>(retval->get())->Parse())
						{
							proceed = false;
						}
					}

					int32_t n = 0;

					if(proceed)
					{
						auto retval1 = std::next(retval);

						rv_ao = static_cast<B1_ASM_OP_STM8 *>(retval->get());
						auto rv_ao1 = (retval1 == cs.end()) ? nullptr : static_cast<B1_ASM_OP_STM8 *>(retval1->get());

						if(ao._args[1] == L"0x1" && rv_ao->_op == L"LD" && rv_ao->_args[0] == L"(0x1,SP)")
						{
							n = 1;
						}
						else
						if(ao._args[1] == L"0x2" && rv_ao->_op == L"LDW" && rv_ao->_args[0] == L"(0x1,SP)" && rv_ao->_args[1] == L"X")
						{
							n = 2;
						}
						else
						if(	rv_ao1 != nullptr &&
							ao._args[1] == L"0x4" && rv_ao1->_type != AOT::AOT_LABEL && !rv_ao1->_is_inline && rv_ao1->Parse() &&
							rv_ao->_op == L"LDW" && rv_ao1->_op == L"LDW" &&
							rv_ao->_args[0] == L"(0x1,SP)" && rv_ao->_args[1] == L"Y" &&
							rv_ao1->_args[0] == L"(0x3,SP)" && rv_ao1->_args[1] == L"X")
						{
							n = 4;
						}
						else
						{
							proceed = false;
						}

						std::vector<std::pair<B1_ASM_OPS::iterator, std::wstring>> new_offsets;

						if(proceed)
						{
							// correct offsets
							for(auto i1 = next1; i1 != call; i1++)
							{
								auto i1ao = static_cast<B1_ASM_OP_STM8 *>(i1->get());

								if(i1ao->_op == L"LD" || i1ao->_op == L"LDW")
								{
									bool no_SP_off = true;
									auto new_off = correct_SP_offset(i1ao->_args[1], n, no_SP_off);
									if(!no_SP_off && new_off.empty())
									{
										proceed = false;
										break;
									}

									if(!new_off.empty())
									{
										new_offsets.push_back(std::make_pair(i1, new_off));
									}
								}
							}

							if(proceed && !new_offsets.empty())
							{
								for(auto &i1: new_offsets)
								{
									auto i1ao = static_cast<B1_ASM_OP_STM8 *>(i1.first->get());
									i1ao->_data = i1ao->_op + L" " + i1ao->_args[0] + L", " + i1.second;
									i1ao->_parsed = false;
								}
							}

							if(proceed)
							{
								cs.erase(i);
								i = next1;

								if(n == 1)
								{
									rv_ao->_data = L"PUSH A";
									rv_ao->_parsed = false;
								}
								else
								if(n == 2)
								{
									rv_ao->_data = L"PUSHW X";
									rv_ao->_parsed = false;
								}
								else
								if(n == 4)
								{
									rv_ao->_data = L"PUSHW X";
									rv_ao->_parsed = false;
									rv_ao1->_data = L"PUSHW Y";
									rv_ao1->_parsed = false;
								}

								update_opt_rule_usage_stat(rule_id);
								changed = true;
								continue;
							}
						}
					}
				}
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"PUSHW" && ao._args[0] == L"X") &&
			(aon1._op == L"LD" && aon1._args[1][0] == L'(' && aon1._args[1] != L"(X)" && aon1._args[1] != L"(Y)" &&
				aon1._args[1].find(L",SP") == std::wstring::npos && aon1._args[1].find(L",X") == std::wstring::npos && aon1._args[1].find(L",Y") == std::wstring::npos) &&
			(aon2._op == L"PUSH" && aon2._args[0] == L"A") &&
			(aon3._op == L"PUSH" && aon3._args[0] == L"0x0")
			)
		{
			// PUSHW X
			// LD A, (addr)
			// PUSH A
			// PUSH 0
			// ->
			// PUSHW X
			// PUSH (addr)
			// PUSH 0
			aon1._data = L"PUSH " + aon1._args[1];
			aon1._parsed = false;
			cs.erase(next2);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}


		auto next4 = std::next(next3);
		if(next4 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon4 = *static_cast<B1_ASM_OP_STM8 *>(next4->get());
		if(aon4._is_inline || !aon4.Parse())
		{
			i++;
			continue;
		}

		int n4_size = 0;
		bool n4_arithm_op = is_arithm_op(aon4, n4_size);

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"PUSHW" && ao._args[0] == L"X") &&
			((aon1._op == L"CALL" || aon1._op == L"CALLR" || aon1._op == L"CALLF") && aon1._args[0] == L"__LIB_STR_CPY") &&
			(aon2._op == L"CALL" || aon2._op == L"CALLR" || aon2._op == L"CALLF") &&
			(aon3._op == L"POPW" && aon3._args[0] == L"X") &&
			((aon4._op == L"CALL" || aon4._op == L"CALLR" || aon4._op == L"CALLF") && aon4._args[0] == L"__LIB_STR_RLS")
			)
		{
			// PUSHW X
			// CALLR __LIB_STR_CPY
			// CALLR __LIB_UART2_OUT
			// POPW X
			// CALLR __LIB_STR_RLS
			// ->
			// CALLR __LIB_UART2_OUT
			cs.erase(next4);
			cs.erase(next3);
			cs.erase(next1);
			cs.erase(i);
			i = next2;

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"PUSH" && ao._args[0] == L"A") &&
			(aon1._op == L"PUSH" && aon1._args[0] == L"0x0") &&
			(aon2._op == L"LDW" && aon2._args[0] == L"X" && aon2._args[1] != L"Y" && aon2._args[1] != L"SP" && aon2._args[1].find(L",SP)") == std::wstring::npos) &&
			((aon3._op == L"SLAW" || aon3._op == L"SLLW") && aon3._args[0] == L"X") &&
			(aon4._op == L"POPW" && aon4._args[0] == L"Y")
			)
		{
			// PUSH A
			// PUSH 0
			// LDW X, smth not depending on Y or SP
			// SLAW X or SLLW X
			// POPW Y
			// ->
			// CLRW Y
			// LD YL, A
			// LDW X, smth
			// SLAW X or SLLW X
			ao._data = L"CLRW Y ";
			ao._parsed = false;
			aon1._data = L"LD YL, A";
			aon1._parsed = false;
			cs.erase(next4);

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(	ao._op == L"PUSHW" && ao._args[0] == L"X" && aon1._op == L"CLRW" && aon1._args[0] == L"Y" && aon2._op == L"LDW" && aon2._args[0] == L"X" && aon2._args[1] == L"(0x1,SP)" &&
			(aon3._op == L"ADD" || aon3._op == L"ADDW") && aon3._args[0] == L"SP" && aon3._args[1] == L"0x2" &&
			(n4_arithm_op || aon4._op == L"RET" || aon4._op == L"RETF" || aon4._op == L"CALLR" || aon4._op == L"CALL" || aon4._op == L"CALLF")
			)
		{
			// PUSHW X
			// CLRW Y
			// LDW X, (0x1, SP)
			// ADDW SP, 2
			// something changing N and Z flags, CALL or RET
			// ->
			// CLRW Y
			// something changing N and Z flags, CALL or RET
			cs.erase(i);
			cs.erase(next2);
			cs.erase(next3);
			i = next1;
			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if ((
				((!ao._volatile && !ao._is_inline && ((ao._op == L"LDW" && ao._args[0] == L"Y" && ao._args[1] != L"(Y)" && ao._args[1].find(L",Y)") == std::wstring::npos && ao._args[1] != L"X") || (ao._op == L"CLRW" && ao._args[0] == L"Y"))) &&
				(!aon1._volatile && !aon1._is_inline && aon1._op == L"LDW" && aon1._args[0] == L"X")) ||
				((ao._op == L"CLRW" && ao._args[0] == L"X") && (aon1._op == L"LD" && aon1._args[0] == L"XL"))
			)
			&&
			(
				((aon2._op == L"PUSHW" && aon2._args[0] == L"X") && (aon3._op == L"PUSHW" && aon3._args[0] == L"Y")) ||
				(
					(!aon2._volatile && !aon2._is_inline && aon2._op == L"LDW" && aon2._args[1] == L"Y" && aon2._args[0] != L"(X)" && aon2._args[0].find(L",X)") == std::wstring::npos && aon2._args[0] != L"X") &&
					(!aon3._volatile && !aon3._is_inline && aon3._op == L"LDW" && aon3._args[1] == L"X" && aon3._args[0] != L"(Y)" && aon3._args[0].find(L",Y)") == std::wstring::npos && aon3._args[0] != L"Y")
				)
			))
		{
			// LDW Y, smth1 or CLRW Y
			// LDW X, smth2
			// PUSHW X / LDW smth3, Y
			// PUSHW Y / LDW smth4, X
			// [operations that do not use Y]
			// LDW Y, smth5 or CLRW Y
			// LDW X, smth6
			// ->
			// LDW X, smth2
			// PUSHW X / LDW smth4, X
			// LDW X, smth1 or CLRW X
			// PUSHW X / LDW smth3, X
			// [operations that do not use Y]
			// LDW Y, smth5 or CLRW Y
			// LDW X, smth6
			bool proceed = true;

			bool no_SP_off = true;
			std::wstring new_off;

			if(aon1._op == L"LD")
			{
				if(i == cs.begin() || aon2._op == L"PUSHW")
				{
					proceed = false;
				}
				else
				{
					auto pr_it = std::prev(i);
					auto &pr = *static_cast<B1_ASM_OP_STM8 *>(pr_it->get());
					if(!pr._parsed || pr._op != L"CLRW" || pr._args[0] != L"Y")
					{
						proceed = false;
					}
				}
			}
			else
			if(ao._op == L"LDW")
			{
				new_off = correct_SP_offset(ao._args[1], -2, no_SP_off);
				if(!no_SP_off && new_off.empty())
				{
					proceed = false;
				}
			}

			if(proceed)
			{
				auto nexti = next4;

				for(; nexti != cs.end(); nexti++)
				{
					auto next_ao = static_cast<B1_ASM_OP_STM8 *>(nexti->get());

					if(next_ao->_type == AOT::AOT_LABEL)
					{
						proceed = false;
						break;
					}

					if(next_ao->_is_inline || !next_ao->Parse())
					{
						proceed = false;
						break;
					}

					if(next_ao->_op.front() == L'J' || next_ao->_op == L"CALL" || next_ao->_op == L"CALLR" || next_ao->_op == L"CALLF" || next_ao->_op == L"RET" || next_ao->_op == L"IRET" || next_ao->_op == L"TRAP")
					{
						proceed = false;
						break;
					}

					if((next_ao->_op == L"LDW" && next_ao->_args[0] == L"Y" && next_ao->_args[1] != L"(Y)" && next_ao->_args[1].find(L",Y)") == std::wstring::npos) || (next_ao->_op == L"CLRW" && next_ao->_args[0] == L"Y"))
					{
						auto nexti1 = std::next(nexti);

						if(nexti1 == cs.end())
						{
							break;
						}

						auto next_aon1 = static_cast<B1_ASM_OP_STM8 *>(nexti1->get());

						if(next_aon1->_type == AOT::AOT_LABEL)
						{
							proceed = false;
							break;
						}

						if(next_aon1->_is_inline || !next_ao->Parse())
						{
							proceed = false;
							break;
						}

						if(!(next_aon1->_op == L"LDW" && next_aon1->_args[0] == L"X"))
						{
							proceed = false;
						}

						break;
					}

					bool write_op = false;

					if(is_reg_used(*next_ao, L"Y", write_op) || write_op)
					{
						proceed = false;
						break;
					}
				}
			}

			if(proceed)
			{
				if(aon1._op == L"LD")
				{
					aon1._data = L"LDW " + aon2._args[0] + L", X";
					aon1._parsed = false;
					aon2._data = L"LD XL, A";
					aon2._parsed = false;
					cs.erase(std::prev(i));
				}
				else
				if(aon2._op == L"PUSHW")
				{
					ao._data = aon1._data;
					ao._parsed = false;
					aon1._data = L"PUSHW X";
					aon1._parsed = false;
					aon2._data = (ao._op == L"LDW") ? (L"LDW X, " + (no_SP_off ? ao._args[1] : new_off)) : L"CLRW X";
					aon2._parsed = false;
					aon3._data = L"PUSHW X";
					aon3._parsed = false;
				}
				else
				{
					ao._data = aon1._data;
					ao._parsed = false;
					aon1._data = aon3._data;
					aon1._parsed = false;
					aon2._data = (ao._op == L"LDW") ? L"LDW X, " + ao._args[1] : L"CLRW X";
					aon2._parsed = false;
					aon3._data = L"LDW " + aon2._args[0] + L", X";
					aon3._parsed = false;
				}

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}


		auto next5 = std::next(next4);
		if(next5 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon5 = *static_cast<B1_ASM_OP_STM8 *>(next5->get());
		if(aon5._is_inline || !aon5.Parse())
		{
			i++;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"PUSHW" && ao._args[0] == L"X") &&
			(aon1._op == L"LD" && aon1._args[0] == L"A") &&
			(aon2._op == L"CLRW" && aon2._args[0] == L"X") &&
			(aon3._op == L"LD" && aon3._args[0] == L"XL") &&
			(aon4._op == L"ADDW" && aon4._args[0] == L"X" && aon4._args[1] == L"(0x1,SP)") &&
			((aon5._op == L"ADDW" || aon5._op == L"ADD") && aon5._args[0] == L"SP" && aon5._args[1] == L"0x2")
			)
		{
			// PUSHW X
			// LD A, smth
			// CLRW X
			// LD XL, A
			// ADDW X, (0x1, SP)
			// ADDW SP, 2
			// ->
			// LD A, XL
			// ADD A, smth
			// RLWA X, A
			// ADC A, 0
			// LD XH, A
			bool no_SP_off = true;
			auto new_off = correct_SP_offset(aon1._args[1], -2, no_SP_off);

			ao._data = L"LD A, XL";
			ao._parsed = false;
			aon1._data = L"ADD A, " + (no_SP_off ? aon1._args[1] : new_off);
			aon1._parsed = false;
			aon2._data = L"RLWA X, A";
			aon2._parsed = false;
			aon3._data = L"ADC A, 0";
			aon3._parsed = false;
			aon4._data = L"LD XH, A";
			aon4._parsed = false;
			cs.erase(next5);
			
			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"LDW" && ao._args[0] == L"X" && ao._args[1] != L"(X)" && ao._args[1].find(L",X)") == std::wstring::npos) &&
			(aon1._op == L"PUSHW" && aon1._args[0] == L"X") &&
			(aon2._op == L"LDW" && aon2._args[0] == L"X" && aon2._args[1].front() == L'(') &&
			((aon3._op == L"CALL" || aon3._op == L"CALLR" || aon3._op == L"CALLF") && aon3._args[0] == L"__LIB_STR_RLS") &&
			(aon4._op == L"POPW" && aon4._args[0] == L"X") &&
			(aon5._op == L"LDW" && aon5._args[1] == L"X" && aon5._args[0] == aon2._args[1])
			)
		{
			// LDW X, __STR_0
			// PUSHW X
			// LDW X, (NS1::__VAR_S_S)
			// CALLR __LIB_STR_RLS
			// POPW X
			// LDW (NS1::__VAR_S_S), X
			// ->
			// LDW X, (NS1::__VAR_S_S)
			// CALLR __LIB_STR_RLS
			// LDW X, __STR_0
			// LDW (NS1::__VAR_S_S), X
			aon4._data = ao._data;
			aon4._parsed = false;
			cs.erase(next1);
			cs.erase(i);
			i = next2;
			
			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if (
			(!ao._volatile && ao._op == L"LD" && ao._args[0] == L"A" && ao._args[1] != L"XL" && ao._args[1] != L"XH") &&
			(aon1._op == L"PUSH" && aon1._args[0] == L"A") &&
			(!aon2._volatile && aon2._op == L"LD" && aon2._args[0] == L"A") &&
			(aon3._op == L"CLRW" && aon3._args[0] == L"X") &&
			(aon4._op == L"LD" && aon4._args[0] == L"XL") &&
			(aon5._op == L"POP" && aon5._args[0] == L"A")
			)
		{
			// LD A, smth1 (not XL and XH)
			// PUSH A
			// LD A, smth2
			// CLRW X
			// LD XL, A
			// POP A
			// ->
			// LD A, smth2
			// CLRW X
			// LD XL, A
			// LD A, smth1 (not XL and XH)

			bool no_SP_off = true;
			auto new_off = correct_SP_offset(aon2._args[1], 1, no_SP_off);
			if(no_SP_off || !new_off.empty())
			{
				aon5._data = ao._data;
				aon5._parsed = false;
				if(!no_SP_off)
				{
					aon2._data = L"LD A, " + new_off;
					aon2._parsed = false;
				}
				cs.erase(next1);
				cs.erase(i);
				i = next2;

				update_opt_rule_usage_stat(rule_id);
				changed = true;
				continue;
			}
		}

		auto next6 = std::next(next5);
		if(next6 == cs.end())
		{
			i++;
			continue;
		}

		auto &aon6 = *static_cast<B1_ASM_OP_STM8 *>(next6->get());
		if(aon6._is_inline || !aon6.Parse())
		{
			i++;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"POPW" && ao._args[0] == L"X") &&
			(aon1._op == L"LDW" && aon1._args[0] == L"X" && (aon1._args[1] == L"(X)" || aon1._args[1].find(L",X)") != std::wstring::npos)) &&
			(aon2._op == L"PUSHW" && aon2._args[0] == L"X") &&
			(aon3._op == L"LDW" && aon3._args[0] == L"X" && aon3._args[1].front() == L'(') &&
			((aon4._op == L"CALL" || aon4._op == L"CALLR" || aon4._op == L"CALLF") && aon4._args[0] == L"__LIB_STR_RLS") &&
			(aon5._op == L"POPW" && aon5._args[0] == L"X") &&
			(aon6._op == L"LDW" && aon6._args[1] == L"X" && aon6._args[0] == aon3._args[1])
			)
		{
			// POPW X
			// LDW X, (X)
			// PUSHW X
			// LDW X, (NS1::__VAR_S_S)
			// CALLR __LIB_STR_RLS
			// POPW X
			// LDW (NS1::__VAR_S_S), X
			// ->
			// LDW X, (NS1::__VAR_S_S)
			// CALLR __LIB_STR_RLS
			// POPW X
			// LDW X, (X)
			// LDW (NS1::__VAR_S_S), X
			aon2._data = aon3._data;
			aon2._parsed = false;
			aon3._data = aon4._data;
			aon3._parsed = false;
			aon4._data = L"POPW X";
			aon4._parsed = false;
			aon5._data = L"LDW X, (X)";
			aon5._parsed = false;
			cs.erase(next1);
			cs.erase(i);
			i = next2;
			
			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"LD" && ao._args[0] == L"A") &&
			(aon1._op == L"PUSH" && aon1._args[0] == L"A") &&
			(aon2._op == L"PUSH" && aon2._args[0] == L"0x0") &&
			(aon3._op == L"LDW" && aon3._args[0] == L"X") &&
			(aon4._op == L"CPW" && aon4._args[0] == L"X") &&
			(aon5._op == L"POPW" && aon5._args[0] == L"X") &&
			(aon6._op[0] == L'J' && aon6._op[1] == L'R')
		)
		{
			// LD A, (1, SP)
			// PUSH A
			// PUSH 0
			// LDW X, smth
			// CPW X, (1, SP)
			// POPW X
			// relative jump
			// ->
			// PUSH 0
			// LDW X, smth
			// CPW X, (1, SP)
			// POP A
			// relative jump

			bool no_SP_off = true;
			int32_t off = -1;
			correct_SP_offset(ao._args[1], 0, no_SP_off, &off);

			if(!no_SP_off && off == 1)
			{
				no_SP_off = true;
				off = -1;
				correct_SP_offset(aon4._args[1], 0, no_SP_off, &off);

				if(!no_SP_off && off == 1)
				{
					no_SP_off = true;
					off = -1;
					std::wstring new_arg = correct_SP_offset(aon3._args[1], -1, no_SP_off, &off);

					if(no_SP_off || !new_arg.empty())
					{
						if(!no_SP_off)
						{
							aon3._data = L"LDW X, " + new_arg;
							aon3._parsed = false;
						}
						aon5._data = L"POP A";
						aon5._parsed = false;
						cs.erase(next1);
						cs.erase(i);
						i = next2;

						update_opt_rule_usage_stat(rule_id);
						changed = true;
						continue;
					}
				}
			}
		}

		rule_id++;
		update_opt_rule_usage_stat(rule_id, true);
		if(
			(ao._op == L"LDW" && ao._args[0] == L"X") &&
			((aon1._op == L"CALL" || aon1._op == L"CALLR" || aon1._op == L"CALLF") && aon1._args[0] == L"__LIB_STR_CPY") &&
			(aon2._op == L"CALL" || aon2._op == L"CALLR" || aon2._op == L"CALLF") &&
			(aon3._op == L"LDW" && aon3._args[0] == L"(0x1,SP)" && aon3._args[1] == L"X") &&
			(aon4._op == L"LDW" && aon4._args[0] == L"X" && ao._args[1] == aon4._args[1]) &&
			((aon5._op == L"CALL" || aon5._op == L"CALLR" || aon5._op == L"CALLF") && aon5._args[0] == L"__LIB_STR_RLS") &&
			(aon6._op == L"POPW" && aon6._args[0] == L"X")
			)
		{
			// LDW X, (NS1::__VAR_S4_S)
			// CALLR __LIB_STR_CPY
			// CALLR __LIB_STR_APD
			// LDW (1, SP), X
			// LDW X, (NS1::__VAR_S4_S)
			// CALLR __LIB_STR_RLS
			// POPW X
			// ->
			// LDW X, (NS1::__VAR_S4_S)
			// CALLR __LIB_STR_APD
			// ADDW SP, 2

			cs.erase(next1);
			cs.erase(next3);
			cs.erase(next4);
			cs.erase(next5);
			aon6._data = L"ADDW SP, 2";
			aon6._parsed = false;

			update_opt_rule_usage_stat(rule_id);
			changed = true;
			continue;
		}


		i++;
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1STM8Compiler::Save(const std::string &file_name, bool overwrite_existing /*= true*/)
{
	std::FILE *ofs = std::fopen(file_name.c_str(), overwrite_existing ? "w" : "a");
	if(ofs == nullptr)
	{
		return C1_T_ERROR::C1_RES_EFOPEN;
	}

	auto err = save_section(L".DATA PAGE0", _page0_sec, ofs);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	std::fclose(ofs);

	return C1Compiler::Save(file_name, false);
}


static void c1stm8_print_version(FILE *fstr)
{
	std::fputs("BASIC1 STM8 compiler\n", fstr);
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

static void c1_print_warnings(const std::vector<std::tuple<int32_t, std::string, C1_T_WARNING>> &wrns)
{
	for(auto &w: wrns)
	{
		c1_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
	}
}

static int optimize(C1STM8Compiler &c1stm8, const std::string &opt_log_file_name, bool print_err_desc)
{
	int retcode = 0;

	if(!opt_log_file_name.empty())
	{
		auto err = c1stm8.ReadOptLogFile(opt_log_file_name);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			c1_print_warnings(c1stm8.GetWarnings());
			c1_print_error(err, -1, "", print_err_desc);
			retcode = 14;
			return retcode;
		}
	}

	bool changed = true;

	while(changed)
	{
		changed = false;

		bool changed1 = true;
		while(changed1)
		{
			changed1 = false;

			auto err = c1stm8.Optimize1(changed1);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				c1_print_warnings(c1stm8.GetWarnings());
				c1_print_error(err, -1, "", print_err_desc);
				retcode = 15;
				return retcode;
			}
			if(changed1)
			{
				changed = true;
			}
		}

		bool changed2 = true;
		while(changed2)
		{
			changed2 = false;

			auto err = c1stm8.Optimize2(changed2);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				c1_print_warnings(c1stm8.GetWarnings());
				c1_print_error(err, -1, "", print_err_desc);
				retcode = 16;
				return retcode;
			}
			if(changed2)
			{
				changed = true;
			}
		}

		bool changed3 = true;
		while(changed3)
		{
			changed3 = false;

			auto err = c1stm8.Optimize3(changed3);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				c1_print_warnings(c1stm8.GetWarnings());
				c1_print_error(err, -1, "", print_err_desc);
				retcode = 17;
				return retcode;
			}
			if(changed3)
			{
				changed = true;
			}
		}
	}

	if(!opt_log_file_name.empty())
	{
		auto err = c1stm8.WriteOptLogFile(opt_log_file_name);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			c1_print_warnings(c1stm8.GetWarnings());
			c1_print_error(err, -1, "", print_err_desc);
			retcode = 25;
			return retcode;
		}
	}

	return retcode;
}


int main(int argc, char** argv)
{
	int i;
	int retcode = 0;
	bool print_err_desc = false;
	bool print_version = false;
	bool out_src_lines = false;
	bool no_asm = false;
	bool no_opt = false;
	std::string ofn;
	bool args_error = false;
	std::string args_error_txt;
	std::string lib_dir;
	std::string MCU_name;
	int32_t stack_size = -1;
	int32_t heap_size = -1;
	bool opt_nocheck = false;
	std::string opt_log_file_name;
	std::string args;


	// use current locale
	std::setlocale(LC_ALL, "");


	// options
	for(i = 1; i < argc; i++)
	{
		// print error description
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'D' || argv[i][1] == 'd') &&
			argv[i][2] == 0)
		{
			print_err_desc = true;
			args = args + " -d";

			continue;
		}

		// fix stack pointer on RET statement (possible non-released local variables)
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'F' || argv[i][1] == 'f') &&
			(argv[i][2] == 'R' || argv[i][2] == 'r') &&
			argv[i][3] == 0)
		{
			_global_settings.SetFixRetStackPtr();
			continue;
		}

		// specify heap size
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'H' || argv[i][1] == 'h') &&
			(argv[i][2] == 'S' || argv[i][2] == 's') &&
			argv[i][3] == 0)
		{
			if(i == argc - 1)
			{
				args_error = true;
				args_error_txt = "missing heap size";
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
					args_error_txt = "wrong heap size";
				}
				heap_size = n;
			}

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
				args = args + " -l " + argv[i];
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
				args = args + " -m " + MCU_name;
			}

			continue;
		}

		// memory model
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'M' || argv[i][1] == 'm') &&
			(argv[i][2] == 'S' || argv[i][2] == 's' || argv[i][2] == 'L' || argv[i][2] == 'l') &&
			argv[i][3] == 0)
		{
			if(argv[i][2] == 'S' || argv[i][2] == 's')
			{
				_global_settings.SetMemModelSmall();
			}
			else
			{
				_global_settings.SetMemModelLarge();
			}

			args = args + " " + argv[i];
			continue;
		}

		// print memory usage
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'M' || argv[i][1] == 'm') &&
			(argv[i][2] == 'U' || argv[i][2] == 'u') &&
			argv[i][3] == 0)
		{
			args = args + " -mu";
			continue;
		}

		// don't call assembler
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'N' || argv[i][1] == 'n') &&
			(argv[i][2] == 'A' || argv[i][2] == 'a') &&
			argv[i][3] == 0)
		{
			no_asm = true;
			continue;
		}

		// disable optimizations
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'N' || argv[i][1] == 'n') &&
			(argv[i][2] == 'O' || argv[i][2] == 'o') &&
			argv[i][3] == 0)
		{
			no_opt = true;;
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

		// specify optimizer log file
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'O' || argv[i][1] == 'o') &&
			(argv[i][2] == 'L' || argv[i][2] == 'l') &&
			argv[i][3] == 0)
		{
			if(i == argc - 1)
			{
				args_error = true;
				args_error_txt = "missing optimizer log file name";
			}
			else
			{
				i++;
				opt_log_file_name = argv[i];
			}

			continue;
		}

		// options
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'O' || argv[i][1] == 'o') &&
			(argv[i][2] == 'P' || argv[i][2] == 'p') &&
			argv[i][3] == 0)
		{
			if(i == argc - 1)
			{
				args_error = true;
				args_error_txt = "missing option";
			}
			else
			{
				i++;
				auto opt = Utils::str_toupper(argv[i]);
				if(opt == "EXPLICIT")
				{
					b1_opt_explicit_val = 1;
				}
				else
				if(opt == "BASE1")
				{
					b1_opt_base_val = 1;
				}
				else
				if(opt == "NOCHECK")
				{
					opt_nocheck = true;
				}
				else
				{
					args_error = true;
					args_error_txt = "unknown option";
				}
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
				args = args + " -ram_size " + argv[i];
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
				args = args + " -ram_start " + argv[i];
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
				args = args + " -rom_size " + argv[i];
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
				args = args + " -rom_start " + argv[i];
			}

			continue;
		}

		// output source lines
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'S' || argv[i][1] == 's') &&
			argv[i][2] == 0)
		{
			out_src_lines = true;
			continue;
		}

		// specify stack size
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'S' || argv[i][1] == 's') &&
			(argv[i][2] == 'S' || argv[i][2] == 's') &&
			argv[i][3] == 0)
		{
			if(i == argc - 1)
			{
				args_error = true;
				args_error_txt = "missing stack size";
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
					args_error_txt = "wrong stack size";
				}
				stack_size = n;
			}

			continue;
		}

		// target (the only supported target is STM8)
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
				if(Utils::str_toupper(Utils::str_trim(argv[i])) != "STM8")
				{
					args_error = true;
					args_error_txt = "invalid target";
				}
			}

			continue;
		}

		// print compiler version
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'V' || argv[i][1] == 'v') &&
			argv[i][2] == 0)
		{
			print_version = true;
			continue;
		}

		break;
	}


	_global_settings.SetTargetName("STM8");
	_global_settings.SetMCUName(MCU_name);
	_global_settings.SetLibDirRoot(lib_dir);

	// load target-specific stuff
	if(!select_target(global_settings))
	{
		args_error = true;
		args_error_txt = "invalid target";
	}

	if((args_error || i == argc) && !(print_version))
	{
		c1stm8_print_version(stderr);
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
		std::fputs(" [options] filename\n", stderr);
		std::fputs("options:\n", stderr);
		std::fputs("-d or /d - print error description\n", stderr);
		std::fputs("-hs or /hs - set heap size (in bytes), e.g. -hs 1024\n", stderr);
		std::fputs("-l or /l - libraries directory, e.g. -l \"../lib\"\n", stderr);
		std::fputs("-m or /m - specify MCU name, e.g. -m STM8S103F3\n", stderr);
		std::fputs("-ml or /ml - set large memory model\n", stderr);
		std::fputs("-ms or /ms - set small memory model (default)\n", stderr);
		std::fputs("-mu or /mu - print memory usage\n", stderr);
		std::fputs("-na or /na - don't run assembler\n", stderr);
		std::fputs("-no or /no - disable optimizations\n", stderr);
		std::fputs("-o or /o - output file name, e.g.: -o out.asm\n", stderr);
		std::fputs("-op or /op - specify option (EXPLICIT, BASE1 or NOCHECK), e.g. -op NOCHECK\n", stderr);
		std::fputs("-ram_size or /ram_size - specify RAM size, e.g.: -ram_size 0x400\n", stderr);
		std::fputs("-ram_start or /ram_start - specify RAM starting address, e.g.: -ram_start 0\n", stderr);
		std::fputs("-rom_size or /rom_size - specify ROM size, e.g.: -rom_size 0x2000\n", stderr);
		std::fputs("-rom_start or /rom_start - specify ROM starting address, e.g.: -rom_start 0x8000\n", stderr);
		std::fputs("-s or /s - output source lines\n", stderr);
		std::fputs("-ss or /ss - set stack size (in bytes), e.g. -ss 256\n", stderr);
		std::fputs("-t or /t - set target (default STM8), e.g.: -t STM8\n", stderr);
		std::fputs("-v or /v - show compiler version\n", stderr);
		return 1;
	}


	if(print_version)
	{
		// just print version and stop executing
		c1stm8_print_version(stdout);
		return 0;
	}


	// list of source files
	std::vector<std::string> src_files;
	for(int j = i; j < argc; j++)
	{
		src_files.push_back(argv[j]);
	}

	_global_settings.InitLibDirs();

	// read settings file if specified
	if(!MCU_name.empty())
	{
		bool cfg_file_read = false;

		auto file_name = _global_settings.GetLibFileName(MCU_name, ".cfg");
		if(!file_name.empty())
		{
			auto err = static_cast<C1_T_ERROR>(_global_settings.Read(file_name));
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				c1_print_error(err, -1, file_name, print_err_desc);
				return 2;
			}
			cfg_file_read = true;
		}

		// initialize library directories a time more to take into account additional ones read from cfg file
		_global_settings.InitLibDirs();

		file_name = _global_settings.GetLibFileName(MCU_name, ".io");
		if(!file_name.empty())
		{
			auto err = static_cast<C1_T_ERROR>(_global_settings.ReadIoSettings(file_name));
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				c1_print_error(err, -1, file_name, print_err_desc);
				return 3;
			}
			cfg_file_read = true;
		}

		if(!cfg_file_read)
		{
			//warning: unknown MCU name
			c1_print_warnings(std::vector<std::tuple<int32_t, std::string, C1_T_WARNING>>({ std::make_tuple((int32_t)-1, MCU_name, C1_T_WARNING::C1_WRN_WUNKNMCU) }));
		}
	}

	if(heap_size >= 0)
	{
		_global_settings.SetHeapSize(heap_size);
	}

	if(stack_size >= 0)
	{
		_global_settings.SetStackSize(stack_size);
	}

	// prepare output file name
	if(ofn.empty())
	{
		// no output file, use input file's directory and name but with asm extension
		ofn = src_files.front();
		auto delpos = ofn.find_last_of("\\/");
		auto pntpos = ofn.find_last_of('.');
		if(pntpos != std::string::npos && (delpos == std::string::npos || pntpos > delpos))
		{
			ofn.erase(pntpos, std::string::npos);
		}
		ofn += ".asm";
	}
	else
	if(ofn.back() == '\\' || ofn.back() == '/')
	{
		// output directory only, use input file name but with asm extension
		std::string tmp = src_files.front();
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
		tmp += ".asm";
		ofn += tmp;
	}


	_B1C_consts[L"__TARGET_NAME"].first = "STM8";
	_B1C_consts[L"__MCU_NAME"].first = MCU_name;


	C1STM8Compiler c1stm8(out_src_lines, opt_nocheck);

	std::set<std::wstring> undef;
	std::set<std::wstring> resolved;
	
	std::vector<std::wstring> init;
	
	init.push_back(L"__INI_STK");
	init.push_back(L"__INI_SYS");
	init.push_back(L"__INI_DATA");

	bool code_init_first = true;
	bool code_init = false;
	bool first_run = true;

	while(true)
	{
		auto err = c1stm8.Load(src_files);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			c1_print_warnings(c1stm8.GetWarnings());
			c1_print_error(err, c1stm8.GetCurrLineNum(), c1stm8.GetCurrFileName(), print_err_desc);
			retcode = 4;
			break;
		}

		err = c1stm8.Compile();
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			c1_print_warnings(c1stm8.GetWarnings());
			c1_print_error(err, c1stm8.GetCurrLineNum(), c1stm8.GetCurrFileName(), print_err_desc);
			retcode = 5;
			break;
		}

		err = c1stm8.WriteCode(code_init, code_init ? -1 : 0);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			c1_print_warnings(c1stm8.GetWarnings());
			c1_print_error(err, c1stm8.GetCurrLineNum(), c1stm8.GetCurrFileName(), print_err_desc);
			retcode = 6;
			break;
		}

		if(first_run)
		{
			if(!no_opt)
			{
				retcode = optimize(c1stm8, opt_log_file_name, print_err_desc);
				if(retcode != 0)
				{
					return retcode;
				}
			}

			first_run = false;
		}

		c1stm8.AddFunctionsSymbols();

		err = c1stm8.GetUndefinedSymbols(undef);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			c1_print_warnings(c1stm8.GetWarnings());
			c1_print_error(err, -1, c1stm8.GetCurrFileName(), print_err_desc);
			retcode = 7;
			break;
		}

		err = c1stm8.GetResolvedSymbols(resolved);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			c1_print_warnings(c1stm8.GetWarnings());
			c1_print_error(err, -1, c1stm8.GetCurrFileName(), print_err_desc);
			retcode = 8;
			break;
		}

		err = c1stm8.GetInitFiles(init);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			c1_print_warnings(c1stm8.GetWarnings());
			c1_print_error(err, -1, c1stm8.GetCurrFileName(), print_err_desc);
			retcode = 9;
			break;
		}

		for(const auto &r: resolved)
		{
			undef.erase(r);
		}

		for(const auto &i: init)
		{
			undef.erase(i);
		}

		src_files.clear();

		if(undef.empty())
		{
			if(code_init_first)
			{
				// write interrupt vector table
				err = c1stm8.WriteCodeInitBegin();
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					c1_print_warnings(c1stm8.GetWarnings());
					c1_print_error(err, -1, "", print_err_desc);
					retcode = 10;
					break;
				}

				code_init_first = false;
			}

			for(const auto &fn: init)
			{
				if(resolved.find(fn) == resolved.end())
				{
					src_files.push_back(Utils::wstr2str(fn));
					break;
				}
			}
			if(src_files.empty())
			{
				break;
			}

			code_init = true;
		}
		else
		{
			src_files.push_back(Utils::wstr2str(*undef.begin()));
			code_init = false;
		}
		
		const auto err_file_name = src_files[0];

		src_files[0] = _global_settings.GetLibFileName(err_file_name, ".b1c");

		if(src_files[0].empty())
		{
			c1_print_warnings(c1stm8.GetWarnings());
			c1_print_error(C1_T_ERROR::C1_RES_EUNRESSYMBOL, -1, err_file_name, print_err_desc);
			retcode = 11;
			break;
		}

		if(undef.empty())
		{
			resolved.insert(Utils::str2wstr(err_file_name));
		}
		else
		{
			resolved.insert(*undef.begin());
			undef.erase(undef.begin());
		}
	}

	if(retcode != 0)
	{
		return retcode;
	}

	// write DAT stmts initialization
	auto err = c1stm8.WriteCodeInitDAT();
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		c1_print_warnings(c1stm8.GetWarnings());
		c1_print_error(err, -1, "", print_err_desc);
		retcode = 12;
		return retcode;
	}

	err = c1stm8.WriteCodeInitEnd();
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		c1_print_warnings(c1stm8.GetWarnings());
		c1_print_error(err, -1, "", print_err_desc);
		retcode = 13;
		return retcode;
	}

	if(!no_opt)
	{
		retcode = optimize(c1stm8, opt_log_file_name, print_err_desc);
		if(retcode != 0)
		{
			return retcode;
		}
	}

	err = c1stm8.Save(ofn);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		c1_print_warnings(c1stm8.GetWarnings());
		c1_print_error(err, -1, ofn, print_err_desc);
		retcode = 26;
		return retcode;
	}

	c1_print_warnings(c1stm8.GetWarnings());

	if(!no_asm)
	{
		std::fputs("running assembler...\n", stdout);
		std::fflush(stdout);
		std::fflush(stderr);

		std::string cwd = argv[0];
		auto delpos = cwd.find_last_of("\\/");
		if(delpos != std::string::npos)
		{
			cwd.erase(delpos + 1, std::string::npos);
		}
		else
		{
			cwd.clear();
		}

		int sc = std::system((cwd + "a1stm8" + args + " -f " + ofn).c_str());
		if(sc == -1)
		{
			std::perror("fail");
			retcode = 27;
		}
	}

	return retcode;
}
