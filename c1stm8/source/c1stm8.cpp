/*
 STM8 intermediate code compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 c1stm8.cpp: STM8 intermediate code compiler
*/


#include <clocale>
#include <cwctype>
#include <cstring>
#include <set>
#include <iterator>
#include <algorithm>

#include "../../common/source/version.h"
#include "../../common/source/stm8.h"
#include "../../common/source/gitrev.h"
#include "../../common/source/Utils.h"
#include "../../common/source/moresym.h"

#include "c1stm8.h"


static const char *version = B1_CMP_VERSION;


// default values: 2 kB of RAM, 16 kB of FLASH, 256 bytes of stack
Settings _global_settings = { 0x0, 0x0800, 0x8000, 0x4000, 0x100, 0x0, STM8_RET_ADDR_SIZE_MM_SMALL };


C1STM8_T_ERROR C1STM8Compiler::find_first_of(const std::wstring &str, const std::wstring &delimiters, size_t &off) const
{
	auto b = str.begin() + off;
	auto e = str.end();

	// skip leading blanks
	while(b != e && std::iswspace(*b)) b++;

	if(b == e)
	{
		off = std::wstring::npos;
		return C1STM8_T_ERROR::C1STM8_RES_OK;
	}

	if(*b == L'\"')
	{
		// quoted string, look for the delimiters right after it
		b++;

		bool open_quote = false;

		while(true)
		{
			if(b == e)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			auto c = *b;

			if(c == L'\"')
			{
				open_quote = !open_quote;
			}
			else
			if(open_quote)
			{
				break;
			}

			b++;
		}

		// skip blanks
		while(b != e && std::iswspace(*b)) b++;
		if(b == e)
		{
			off = std::wstring::npos;
			return C1STM8_T_ERROR::C1STM8_RES_OK;
		}

		if(delimiters.find(*b) == std::wstring::npos)
		{
			return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
		}

		off = b - str.begin();
	}
	else
	{
		off = str.find_first_of(delimiters, off);
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

std::wstring C1STM8Compiler::get_next_value(const std::wstring &str, const std::wstring &delimiters, size_t &next_off) const
{
	auto b = next_off;
	std::wstring nv;

	find_first_of(str, delimiters, next_off);
	if(next_off == std::wstring::npos)
	{
		nv = std::wstring(str.begin() + b, str.end());
	}
	else
	{
		nv = std::wstring(str.begin() + b, str.begin() + next_off++);
	}

	if(!B1CUtils::is_str_val(nv))
	{
		nv = Utils::str_toupper(nv);
	}

	return nv;
}

bool C1STM8Compiler::check_label_name(const std::wstring &name) const
{
	return (name.find_first_not_of(L"_:0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::wstring::npos);
}

bool C1STM8Compiler::check_stdfn_name(const std::wstring &name) const
{
	return (name.find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz$") == std::wstring::npos);
}

bool C1STM8Compiler::check_cmd_name(const std::wstring &name) const
{
	if(B1CUtils::is_bin_op(name) || B1CUtils::is_un_op(name) || B1CUtils::is_log_op(name))
	{
		return true;
	}
	return (name.find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::wstring::npos);
}

bool C1STM8Compiler::check_type_name(const std::wstring &name) const
{
	return (Utils::get_type_by_name(name) != B1Types::B1T_UNKNOWN);
}

bool C1STM8Compiler::check_namespace_name(const std::wstring &name) const
{
	return (name.find_first_not_of(L"_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::wstring::npos);
}

bool C1STM8Compiler::check_address(const std::wstring &address) const
{
	int32_t n;
	return (Utils::str2int32(address, n) == B1_RES_OK);
}

bool C1STM8Compiler::check_num_val(const std::wstring &numval) const
{
	int32_t n;
	return (Utils::str2int32(numval, n) == B1_RES_OK);
}

bool C1STM8Compiler::check_str_val(const std::wstring &strval) const
{
	std::wstring s;
	return (B1CUtils::get_string_data(strval, s) == B1_RES_OK);
}

C1STM8_T_ERROR C1STM8Compiler::get_cmd_name(const std::wstring &str, std::wstring &name, size_t &next_off) const
{
	name = Utils::str_trim(get_next_value(str, L",", next_off));
	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::get_simple_arg(const std::wstring &str, B1_TYPED_VALUE &arg, size_t &next_off) const
{
	auto sval = Utils::str_trim(get_next_value(str, L",)", next_off));
	arg.value = sval;
	arg.type = B1Types::B1T_UNKNOWN;
	return sval.empty() ? static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX) : C1STM8_T_ERROR::C1STM8_RES_OK;
}

std::wstring C1STM8Compiler::gen_next_tmp_namespace()
{
	return L"NS" + std::to_wstring(_next_temp_namespace_id++);
}

// replaces default namespace mark (::) with namespace name
std::wstring C1STM8Compiler::add_namespace(const std::wstring &name) const
{
	if(name.size() > 2 && name[0] == L':' && name[1] == L':')
	{
		return _curr_name_space.empty() ? name.substr(2) : (_curr_name_space + name);
	}

	return name;
}

C1STM8_T_ERROR C1STM8Compiler::get_arg(const std::wstring &str, B1_CMP_ARG &arg, size_t &next_off) const
{
	bool check_optional = false;

	arg.clear();
	
	auto name = Utils::str_trim(get_next_value(str, L"<", next_off));
	if(next_off == std::wstring::npos)
	{
		return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
	}
	if(!check_label_name(name) && !check_num_val(name) && !check_str_val(name) && !check_stdfn_name(name))
	{
		return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
	}
	name = add_namespace(name);

	auto type = Utils::str_trim(get_next_value(str, L">", next_off));
	if(next_off == std::wstring::npos)
	{
		return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
	}
	if(!check_type_name(type))
	{
		return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
	}

	arg.push_back(B1_TYPED_VALUE(name, Utils::get_type_by_name(type)));

	name = Utils::str_trim(get_next_value(str, L"(,", next_off));
	if(!name.empty())
	{
		return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
	}

	if(next_off != std::wstring::npos && str[next_off - 1] == L'(')
	{
		while(true)
		{
			name = Utils::str_trim(get_next_value(str, L"<,)", next_off));
			if(next_off == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			if(name.empty())
			{
				auto dc = str[next_off - 1];

				if(dc == L'<')
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}

				// probably omitted function argument
				arg.push_back(B1_TYPED_VALUE(L""));
				check_optional = true;

				if(dc == L')')
				{
					name = Utils::str_trim(get_next_value(str, L",", next_off));
					if(!name.empty())
					{
						return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
					}

					break;
				}

				continue;
			}

			if(!check_label_name(name) && !check_num_val(name) && !check_str_val(name) && !check_stdfn_name(name))
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}
			name = add_namespace(name);

			type = Utils::str_trim(get_next_value(str, L">", next_off));
			if(next_off == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}
			if(!check_type_name(type))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
			}

			arg.push_back(B1_TYPED_VALUE(name, Utils::get_type_by_name(type)));

			name = Utils::str_trim(get_next_value(str, L",)", next_off));
			if(!name.empty() || next_off == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			if(str[next_off - 1] == L')')
			{
				name = Utils::str_trim(get_next_value(str, L",", next_off));
				if(!name.empty())
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}

				break;
			}
		}
	}

	if(check_optional)
	{
		auto fn = get_fn(arg);
		if(fn == nullptr)
		{
			return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
		}

		for(int i = 0; i < fn->args.size(); i++)
		{
			if(arg[i + 1].value.empty())
			{
				if(fn->args[i].optional)
				{
					arg[i + 1].value = fn->args[i].defval;
					arg[i + 1].type = fn->args[i].type;
				}
				else
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}
			}
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::process_asm_cmd(const std::wstring &line)
{
	if(!line.empty())
	{
		size_t offset = 0, prev_off, len;
		std::wstring cmd;
		int lbl_off = -1;

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
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
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
					return C1STM8_T_ERROR::C1STM8_RES_EINVLBNAME;
				}
				cmd = add_namespace(cmd);

				_req_symbols.insert(cmd);

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

				_asm_stmt_it->args.push_back(B1_CMP_ARG(std::wstring(line.begin(), line.begin() + prev_off) + cmd + std::wstring(line.begin() + prev_off - 1 + len, line.end())));
			}
		}
		else
		{
			_asm_stmt_it->args.push_back(B1_CMP_ARG(line));
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::replace_inline(std::wstring &line, const std::map<std::wstring, std::wstring> &inl_params)
{
	for(const auto &ip: inl_params)
	{
		auto val_start = L"{" + ip.first;
		auto offset = line.find(val_start);
		
		while(offset != std::wstring::npos)
		{
			auto val_len = val_start.length();

			if(offset + val_len == line.length())
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			int32_t start = 0, charnum = -1;

			auto c = line[offset + val_len];
			
			if(c == L'}')
			{
				val_len++;
			}
			else
			if(c == L',')
			{
				auto offset1 = offset + val_len + 1;

				auto str = Utils::str_trim(get_next_value(line, L",", offset1));
				if(offset1 == std::wstring::npos)
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}
				auto err = Utils::str2int32(str, start);
				if(err != B1_RES_OK)
				{
					return static_cast<C1STM8_T_ERROR>(err);
				}
				
				str = Utils::str_trim(get_next_value(line, L"}", offset1));
				err = Utils::str2int32(str, charnum);
				if(err != B1_RES_OK)
				{
					return static_cast<C1STM8_T_ERROR>(err);
				}
				
				if(offset1 == std::wstring::npos)
				{
					val_len = line.length() - offset;
				}
				else
				{
					val_len = offset1 - offset;
				}
			}
			else
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			line.replace(offset, val_len, (start > ip.second.length()) ? L"" : ip.second.substr(start, (charnum < 0) ? std::wstring::npos : (size_t)charnum));
			offset = line.find(val_start);
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::load_inline(size_t offset, const std::wstring &line, const_iterator pos, const std::map<std::wstring, std::wstring> &inl_params)
{
	B1_TYPED_VALUE tv;

	// read file name
	auto err = get_simple_arg(line, tv, offset);
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}
	if(offset != std::wstring::npos)
	{
		return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
	}

	const auto file_name = _global_settings.GetLibFileName(Utils::wstr2str(tv.value), ".b1c");
	if(file_name.empty())
	{
		return C1STM8_T_ERROR::C1STM8_RES_EFOPEN;
	}

	if(_inline_code.find(file_name) != _inline_code.end())
	{
		return C1STM8_T_ERROR::C1STM8_RES_ERECURINL;
	}

	_inline_code.insert(file_name);

	std::FILE *fp = std::fopen(file_name.c_str(), "r");
	if(fp == nullptr)
	{
		return C1STM8_T_ERROR::C1STM8_RES_EFOPEN;
	}

	auto saved_ns = _curr_name_space;
	_curr_name_space = gen_next_tmp_namespace();

	std::wstring inl_line;

	while(true)
	{
		err = static_cast<C1STM8_T_ERROR>(Utils::read_line(fp, inl_line));
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			break;
		}

		err = replace_inline(inl_line, inl_params);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			break;
		}

		err = load_next_command(inl_line, pos);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			break;
		}
	}

	_curr_name_space = saved_ns;

	std::fclose(fp);

	if(err == static_cast<C1STM8_T_ERROR>(B1_RES_EEOF))
	{
		err = C1STM8_T_ERROR::C1STM8_RES_OK;
	}

	if(_inline_asm && err == C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
	}

	_inline_code.erase(file_name);

	return err;
}

C1STM8_T_ERROR C1STM8Compiler::load_next_command(const std::wstring &line, const_iterator pos)
{
	auto b = line.begin();
	auto e = line.end();

	// skip leading and trailing spaces
	while(b != e && std::iswspace(*b)) b++;
	while(b != e && std::iswspace(*(e - 1))) e--;

	if(b != e)
	{
		size_t offset = 0;
		std::wstring cmd;
		B1_TYPED_VALUE tv;
		B1_CMP_ARG arg;
		std::vector<B1_CMP_ARG> args;

		// label
		if(*b == L':')
		{
			b++;
			std::wstring lname(b, e);
			lname = Utils::str_trim(get_next_value(lname, L";", offset));
			if(!check_label_name(lname))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVLBNAME;
			}
			lname = add_namespace(lname);
			
			if(_inline_asm)
			{
				_asm_stmt_it->args.push_back(L":" + lname + L"\n");
			}
			else
			{
				emit_label(lname, pos, true);
			}

			_all_symbols.insert(lname);

			return C1STM8_T_ERROR::C1STM8_RES_OK;
		}

		// comment
		if(*b == L';')
		{
			return C1STM8_T_ERROR::C1STM8_RES_OK;
		}

		// command
		auto err = get_cmd_name(line, cmd, offset);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			return err;
		}

		if(_inline_asm)
		{
			if(cmd == L"ENDASM")
			{
				_inline_asm = false;

				if(offset != std::wstring::npos)
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}

				return C1STM8_T_ERROR::C1STM8_RES_OK;
			}

			return process_asm_cmd(line);
		}

		if(!check_cmd_name(cmd))
		{
			return C1STM8_T_ERROR::C1STM8_RES_EINVCMDNAME;
		}

		if(cmd == L"ASM")
		{
			_asm_stmt_it = emit_inline_asm(pos);

			_inline_asm = true;

			if(offset != std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			return C1STM8_T_ERROR::C1STM8_RES_OK;
		}

		if(cmd == L"DEF")
		{
			// read fn name
			err = get_simple_arg(line, tv, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			if(!check_label_name(tv.value))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVLBNAME;
			}
			tv.value = add_namespace(tv.value);
			args.push_back(tv.value);

			// read fn return type
			err = get_simple_arg(line, tv, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			if(!check_type_name(tv.value))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
			}
			args.push_back(B1_CMP_ARG(tv.value, Utils::get_type_by_name(tv.value)));

			// read fn arguments types
			while(offset != std::wstring::npos)
			{
				err = get_simple_arg(line, tv, offset);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
				if(!check_type_name(tv.value))
				{
					return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
				}
				args.push_back(B1_CMP_ARG(tv.value, Utils::get_type_by_name(tv.value)));
			}
		}
		else
		if(cmd == L"GA" || cmd == L"MA")
		{
			// read var. name
			err = get_simple_arg(line, tv, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			if(!check_label_name(tv.value))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVLBNAME;
			}
			tv.value = add_namespace(tv.value);
			args.push_back(tv.value);

			// read var. type
			auto sval = Utils::str_trim(get_next_value(line, L",(", offset));
			if(sval.empty())
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}
			if(!check_type_name(sval))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
			}
			args.push_back(B1_CMP_ARG(sval, Utils::get_type_by_name(sval)));

			bool is_static = false;

			// read optional type modifiers (V - stands for volatile, S - static)
			if(offset != std::wstring::npos && line[offset - 1] == L'(')
			{
				sval = Utils::str_trim(get_next_value(line, L")", offset));

				std::wstring type_mod;
				auto lpos = sval.find(L'V');
				if(lpos != std::wstring::npos)
				{
					type_mod += L'V';
					sval.erase(lpos, 1);
				}
				lpos = sval.find(L'S');
				if(lpos != std::wstring::npos)
				{
					// here tv.value already contains variable name
					is_static = true;
					cmd = L"MA";
					sval.erase(lpos, 1);
				}

				if(!sval.empty())
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}

				if(!type_mod.empty())
				{
					args.back().push_back(B1_TYPED_VALUE(type_mod));
				}

				sval = Utils::str_trim(get_next_value(line, L",", offset));
				if(!sval.empty())
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}
			}

			// read var. address
			if(cmd == L"MA")
			{
				if(!is_static)
				{
					err = get_simple_arg(line, tv, offset);
					if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
					{
						return err;
					}
					if(!Utils::check_const_name(tv.value) && !check_address(tv.value))
					{
						return static_cast<C1STM8_T_ERROR>(B1_RES_EINVNUM);
					}
				}
				// for static variable save its name as address
				args.push_back(tv.value);
			}

			// get var. size
			int argnum = 0;
			while(offset != std::wstring::npos)
			{
				err = get_arg(line, arg, offset);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
				args.push_back(arg);
				argnum++;
			}
			if(argnum % 2 != 0)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_EWRARGCNT);
			}
		}
		else
		if(cmd == L"LA")
		{
			// read var. name
			err = get_simple_arg(line, tv, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			if(!check_label_name(tv.value))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVLBNAME;
			}
			tv.value = add_namespace(tv.value);
			args.push_back(tv.value);

			// read var. type
			err = get_simple_arg(line, tv, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			if(!check_type_name(tv.value))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
			}
			args.push_back(B1_CMP_ARG(tv.value, Utils::get_type_by_name(tv.value)));
		}
		else
		if(cmd == L"NS")
		{
			err = get_simple_arg(line, tv, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			if(!check_namespace_name(tv.value))
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}
			args.push_back(tv.value);

			// set namespace
			_curr_name_space = tv.value;
		}
		else
		if(cmd == L"OUT" || cmd == L"IN")
		{
			if(offset == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			auto sval = Utils::str_trim(get_next_value(line, L",", offset));
			args.push_back(sval);
			if(offset == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			err = get_arg(line, arg, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			args.push_back(arg);
		}
		else
		if(cmd == L"IOCTL")
		{
			if(offset == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			// read device name
			err = get_arg(line, arg, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			if(!B1CUtils::is_str_val(arg[0].value))
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}
			args.push_back(arg);
			auto dev_name = _global_settings.GetIoDeviceName(arg[0].value.substr(1, arg[0].value.length() - 2));

			if(offset == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			// read command
			err = get_arg(line, arg, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			if(!B1CUtils::is_str_val(arg[0].value))
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}
			args.push_back(arg);
			auto cmd_name = arg[0].value.substr(1, arg[0].value.length() - 2);

			// check data
			Settings::IoCmd iocmd;
			if(!_global_settings.GetIoCmd(dev_name, cmd_name, iocmd))
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}
			if(iocmd.accepts_data)
			{
				if(offset == std::wstring::npos)
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}

				// read data
				err = get_arg(line, arg, offset);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}

				if(iocmd.predef_only)
				{
					if(!B1CUtils::is_str_val(arg[0].value))
					{
						return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
					}

					auto cmd_data = arg[0].value.substr(1, arg[0].value.length() - 2);
					if(iocmd.values.find(cmd_data) == iocmd.values.end())
					{
						return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
					}
				}
				else
				{
					if(iocmd.data_type == B1Types::B1T_LABEL)
					{
						const auto label = (arg[0].value.length() >= 3 && arg[0].value[0] == L'\"') ? arg[0].value.substr(1, arg[0].value.length() - 2) : arg[0].value;
						
						if(!check_label_name(label))
						{
							return C1STM8_T_ERROR::C1STM8_RES_EINVLBNAME;
						}
						_req_symbols.insert(label);
						arg[0].value = label;
					}
					else
					if(!B1CUtils::are_types_compatible(arg[0].type, iocmd.data_type))
					{
						return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
					}
				}

				args.push_back(arg);
			}
		}
		else
		if(cmd == L"END" || cmd == L"RET" || cmd == L"RST")
		{
			if(cmd == L"RST")
			{
				// get mandatory namespace name
				if(offset == std::wstring::npos)
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}

				err = get_simple_arg(line, tv, offset);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
				if(!check_namespace_name(tv.value))
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}
				args.push_back(tv.value);
			}

			if(offset != std::wstring::npos)
			{
				if(cmd == L"RST")
				{
					err = get_simple_arg(line, tv, offset);
					if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
					{
						return err;
					}
					if(!check_label_name(tv.value))
					{
						return C1STM8_T_ERROR::C1STM8_RES_EINVLBNAME;
					}
					tv.value = add_namespace(tv.value);
					args.push_back(tv.value);
				}
			}
		}
		else
		if(cmd == L"RETVAL")
		{
			err = get_arg(line, arg, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			args.push_back(arg);

			err = get_simple_arg(line, tv, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			if(!check_type_name(tv.value))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
			}
			args.push_back(B1_CMP_ARG(tv.value, Utils::get_type_by_name(tv.value)));
		}
		else
		if(cmd == L"SET")
		{
			if(offset == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			err = get_simple_arg(line, tv, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			args.push_back(tv.value);

			if(tv.value == L"ERR")
			{
				err = get_arg(line, arg, offset);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
				args.push_back(arg);
			}
			else
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}
		}
		else
		if(cmd == L"JMP" || cmd == L"JF" || cmd == L"JT" || cmd == L"CALL" || cmd == L"GF" || cmd == L"LF" || cmd == L"IMP" || cmd == L"INI" || cmd == L"INT")
		{
			// read label name
			err = get_simple_arg(line, tv, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			if(!check_label_name(tv.value))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVLBNAME;
			}
			
			if(cmd == L"IMP")
			{
				_req_symbols.insert(tv.value);
			}
			else
			if(cmd == L"INT")
			{
				_req_symbols.insert(L"__" + tv.value);
			}
			else
			if(cmd == L"INI")
			{
				_init_files.push_back(tv.value);
			}
			else
			{
				tv.value = add_namespace(tv.value);
			}

			args.push_back(tv.value);
		}
		else
		if(cmd == L"INL")
		{
			return load_inline(offset, line, pos);
		}
		else
		if(cmd == L"ERR")
		{
			// read error code (can be absent)
			if(offset == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			auto sval = Utils::str_trim(get_next_value(line, L",", offset));
			args.push_back(sval);
			if(offset == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			// read label name
			err = get_simple_arg(line, tv, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			if(!check_label_name(tv.value))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVLBNAME;
			}
			tv.value = add_namespace(tv.value);
			args.push_back(tv.value);
		}
		else
		if(cmd == L"DAT")
		{
			// get mandatory namespace name
			if(offset == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			err = get_simple_arg(line, tv, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			args.push_back(tv.value);

			while(offset != std::wstring::npos)
			{
				err = get_arg(line, arg, offset);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
				args.push_back(arg);
			}
		}
		else
		if(cmd == L"READ")
		{
			// get mandatory namespace name
			if(offset == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			err = get_simple_arg(line, tv, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			args.push_back(tv.value);

			if(offset == std::wstring::npos)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}
			err = get_arg(line, arg, offset);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			args.push_back(arg);
		}
		else
		if(B1CUtils::is_bin_op(cmd) || B1CUtils::is_log_op(cmd) || B1CUtils::is_un_op(cmd))
		{
			while(offset != std::wstring::npos)
			{
				err = get_arg(line, arg, offset);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
				args.push_back(arg);
			}

			if(!((B1CUtils::is_bin_op(cmd) && args.size() == 3) || args.size() == 2))
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}
		}
		else
		{
			return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
		}

		if(offset != std::wstring::npos)
		{
			return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
		}

		emit_command(cmd, pos, args);
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

// function without arguments
const B1_CMP_FN *C1STM8Compiler::get_fn(const B1_TYPED_VALUE &val) const
{
	// check standard functions first
	auto fn = B1_CMP_FNS::get_fn(val);

	if(fn == nullptr)
	{
		// check user functions
		for(const auto &ufn: _ufns)
		{
			if(ufn.first == val.value && ufn.second.args.size() == 0)
			{
				return &ufn.second;
			}
		}
	}

	return fn;
}

const B1_CMP_FN *C1STM8Compiler::get_fn(const B1_CMP_ARG &arg) const
{
	// check standard functions first
	auto fn = B1_CMP_FNS::get_fn(arg);

	if(fn == nullptr)
	{
		// check user functions
		for(const auto &ufn: _ufns)
		{
			if(ufn.first == arg[0].value && ufn.second.args.size() == arg.size() - 1)
			{
				fn = &ufn.second;
				// check arguments types
				for(auto a = arg.begin() + 1; a != arg.end(); a++)
				{
					if(!B1CUtils::are_types_compatible(a->type, fn->args[a - arg.begin() - 1].type))
					{
						fn = nullptr;
						break;
					}
				}
				if(fn != nullptr)
				{
					break;
				}
			}
		}
	}

	return fn;
}

// checks if the arg is variable or function call, arg can be scalar or subscripted variable or function call with
// omitted arguments. the function inserts default values for omitted arguments and put found variables into _vars map
// e.g.:	INSTR<INT>(, "133"<STRING>, "3"<STRING>) -> INSTR<INT>(1<INT>, "133"<STRING>, "3"<STRING>)
//			ARR<WORD>(I<INT>) -> check ARR and I types and put them into _vars if necessary
C1STM8_T_ERROR C1STM8Compiler::check_arg(B1_CMP_ARG &arg)
{
	// check function arguments/array subscripts, their types should be defined first
	for(auto a = arg.begin() + 1; a != arg.end(); a++)
	{
		if(_locals.find(a->value) != _locals.end() || B1CUtils::is_fn_arg(a->value) || B1CUtils::is_imm_val(a->value))
		{
			continue;
		}

		auto fn = get_fn(*a);

		if(fn == nullptr)
		{
			// simple variable
			auto ma = _mem_areas.find(a->value);
			if(ma == _mem_areas.end())
			{
				auto v = _vars.find(a->value);
				if(v == _vars.end())
				{
					if(Utils::check_const_name(a->value))
					{
						a->type = Utils::get_const_type(a->value);
					}
					else
					{
						if(_vars_order_set.find(a->value) == _vars_order_set.end())
						{
							_vars_order_set.insert(a->value);
							_vars_order.push_back(a->value);
						}
						
						_vars[a->value] = B1_CMP_VAR(a->value, a->type, 0, false, _curr_src_file_id, _curr_line_cnt);
					}
				}
				else
				{
					if(v->second.type != a->type)
					{
						return C1STM8_T_ERROR::C1STM8_RES_EVARTYPMIS;
					}
					if(v->second.dim_num != 0)
					{
						return C1STM8_T_ERROR::C1STM8_RES_EVARDIMMIS;
					}
				}
			}
			else
			{
				if(ma->second.type != a->type)
				{
					return C1STM8_T_ERROR::C1STM8_RES_EVARTYPMIS;
				}
				if(ma->second.dim_num != 0)
				{
					return C1STM8_T_ERROR::C1STM8_RES_EVARDIMMIS;
				}
			}
		}
	}

	if(_locals.find(arg[0].value) != _locals.end() || B1CUtils::is_fn_arg(arg[0].value) || B1CUtils::is_imm_val(arg[0].value))
	{
		return C1STM8_T_ERROR::C1STM8_RES_OK;
	}

	auto fn = get_fn(arg);

	if(fn != nullptr)
	{
		// check function arg. count and their types
		if(arg.size() - 1 != fn->args.size())
		{
			return static_cast<C1STM8_T_ERROR>(B1_RES_EWRARGCNT);
		}

		for(int a = 0; a < fn->args.size(); a++)
		{
			if(arg[a + 1].value.empty())
			{
				if(!fn->args[a].optional)
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}

				arg[a + 1].value = fn->args[a].defval;
				arg[a + 1].type = fn->args[a].type;
			}
			else
			// STRING value cannot be passed to a function as non-STRING argument
			if(fn->args[a].type != B1Types::B1T_STRING && arg[a + 1].type == B1Types::B1T_STRING)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_EWARGTYPE);
			}
		}
	}
	else
	{
		// variable
		auto ma = _mem_areas.find(arg[0].value);
		if(ma == _mem_areas.end())
		{
			auto v = _vars.find(arg[0].value);
			if(v == _vars.end())
			{
				if(Utils::check_const_name(arg[0].value))
				{
					if(arg.size() != 1)
					{
						return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
					}

					arg[0].type = Utils::get_const_type(arg[0].value);
				}
				else
				{
					if(_vars_order_set.find(arg[0].value) == _vars_order_set.end())
					{
						_vars_order_set.insert(arg[0].value);
						_vars_order.push_back(arg[0].value);
					}

					_vars[arg[0].value] = B1_CMP_VAR(arg[0].value, arg[0].type, arg.size() - 1, false, _curr_src_file_id, _curr_line_cnt);
				}
			}
			else
			{
				if(v->second.type != arg[0].type)
				{
					return C1STM8_T_ERROR::C1STM8_RES_EVARTYPMIS;
				}
				if(v->second.dim_num != arg.size() - 1)
				{
					return C1STM8_T_ERROR::C1STM8_RES_EVARDIMMIS;
				}
			}
		}
		else
		{
			if(ma->second.type != arg[0].type)
			{
				return C1STM8_T_ERROR::C1STM8_RES_EVARTYPMIS;
			}
			if(ma->second.dim_num != arg.size() - 1)
			{
				return C1STM8_T_ERROR::C1STM8_RES_EVARDIMMIS;
			}
		}

		// check subscript types (should be numeric)
		for(auto a = arg.begin() + 1; a != arg.end(); a++)
		{
			if(a->type == B1Types::B1T_STRING)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
			}
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::read_ufns()
{
	_ufns.clear();

	for(const auto &cmd: *this)
	{
		_curr_src_file_id = cmd.src_file_id;
		_curr_line_cnt = cmd.line_cnt;

		if(!B1CUtils::is_cmd(cmd))
		{
			continue;
		}

		if(cmd.cmd == L"DEF")
		{
			const auto &fname = cmd.args[0][0].value;

			// function name can't be one from the predifined constants list
			if(Utils::check_const_name(fname))
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_EIDINUSE);
			}

			if(_ufns.find(fname) != _ufns.end())
			{
				return C1STM8_T_ERROR::C1STM8_RES_EUFNREDEF;
			}

			B1_CMP_FN fn(fname, cmd.args[1][0].type, std::vector<B1Types>(), fname, false);
			for(auto at = cmd.args.begin() + 2; at != cmd.args.end(); at++)
			{
				fn.args.push_back(B1_CMP_FN_ARG(at->at(0).type, false, L""));
			}

			_ufns.emplace(std::make_pair(fname, fn));
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::read_and_check_locals()
{
	_locals.clear();

	for(const auto &cmd: *this)
	{
		_curr_src_file_id = cmd.src_file_id;
		_curr_line_cnt = cmd.line_cnt;

		if(!B1CUtils::is_cmd(cmd))
		{
			continue;
		}

		if(cmd.cmd == L"LA")
		{
			if(_locals.find(cmd.args[0][0].value) != _locals.end())
			{
				return C1STM8_T_ERROR::C1STM8_RES_ELCLREDEF;
			}

			_locals[cmd.args[0][0].value] = B1_CMP_VAR(cmd.args[0][0].value, cmd.args[1][0].type, 0, false, _curr_src_file_id, _curr_line_cnt);
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

// check variables types and sizes, set values of optional function arguments, build variable list
C1STM8_T_ERROR C1STM8Compiler::read_and_check_vars()
{
	//       name          GAs number
	std::map<std::wstring, int32_t> exp_alloc;

	_vars.clear();
	_vars_order.clear();
	_vars_order_set.clear();
	_mem_areas.clear();
	_data_stmts.clear();

	for(auto ci = begin(); ci != end(); ci++)
	{
		auto &cmd = *ci;

		_curr_src_file_id = cmd.src_file_id;
		_curr_line_cnt = cmd.line_cnt;

		if(!B1CUtils::is_cmd(cmd))
		{
			continue;
		}

		if(	cmd.cmd == L"LA" || cmd.cmd == L"LF" || cmd.cmd == L"NS" || cmd.cmd == L"JMP" || cmd.cmd == L"JF" || cmd.cmd == L"JT" ||
			cmd.cmd == L"CALL" || cmd.cmd == L"RET" || cmd.cmd == L"DAT" || cmd.cmd == L"RST" || cmd.cmd == L"END" || cmd.cmd == L"DEF" ||
			cmd.cmd == L"ERR" || cmd.cmd == L"IMP" || cmd.cmd == L"INI" || cmd.cmd == L"INT")
		{
			if(cmd.cmd == L"DAT")
			{
				_data_stmts[cmd.args[0][0].value].push_back(ci);
				_data_stmts_init.insert(cmd.args[0][0].value);
			}

			continue;
		}

		if(cmd.cmd == L"GA" || cmd.cmd == L"MA")
		{
			bool is_ma = (cmd.cmd == L"MA");
			bool check_sizes = false;

			auto &vars = is_ma ? _mem_areas : _vars;

			const auto &vname = cmd.args[0][0].value;

			// variable name can't be one from the predifined constants list
			if(Utils::check_const_name(vname))
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_EIDINUSE);
			}

			const auto vtype = cmd.args[1][0].type;
			auto v = vars.find(vname);
			int32_t dims_off = is_ma ? 3 : 2;
			int32_t dims = cmd.args.size() - dims_off;
			bool is_volatile = (cmd.args[1].size() > 1) && (cmd.args[1][1].value.find(L'V') != std::wstring::npos);

			if(is_ma)
			{
				// allow for mem. references to be temporarily added to variables (if the reference is used prior to MA statement)
				if(v != vars.end())
				{
					return C1STM8_T_ERROR::C1STM8_RES_EVARREDEF;
				}
			}
			else
			{
				if(_mem_areas.find(vname) != _mem_areas.end())
				{
					return C1STM8_T_ERROR::C1STM8_RES_EVARREDEF;
				}

				auto ea = exp_alloc.find(vname);
				if(ea == exp_alloc.end())
				{
					exp_alloc[vname] = 1;
					check_sizes = true;
				}
				else
				{
					ea->second++;
				}
			}

			if(v == vars.end())
			{
				vars[vname] = B1_CMP_VAR(vname, vtype, dims / 2, is_volatile, _curr_src_file_id, _curr_line_cnt);
				v = vars.find(vname);

				if(is_ma)
				{
					int32_t addr = 0, size = 0;
					// vname == cmd.args[2][0].value for static variables
					bool is_static = vname == cmd.args[2][0].value;
					
					if(is_static || Utils::check_const_name(cmd.args[2][0].value))
					{
						v->second.use_symbol = true;
						v->second.symbol = cmd.args[2][0].value;
					}
					else
					{
						auto err = Utils::str2int32(cmd.args[2][0].value, addr);
						if(err != B1_RES_OK)
						{
							return static_cast<C1STM8_T_ERROR>(err);
						}
					}

					// write address and size for MA variables
					v->second.address = addr;

					if(!B1CUtils::get_asm_type(vtype, nullptr, &size))
					{
						C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
					}
					// single element size (even for subscripted variables)
					v->second.size = size;
					v->second.fixed_size = true;

					if(is_static && _vars_order_set.find(vname) == _vars_order_set.end())
					{
						_vars_order_set.insert(vname);
						_vars_order.push_back(vname);
					}
				}
				else
				{
					if(_vars_order_set.find(vname) == _vars_order_set.end())
					{
						_vars_order_set.insert(vname);
						_vars_order.push_back(vname);
					}
				}
			}
			else
			{
				if(v->second.type != B1Types::B1T_UNKNOWN && v->second.type != vtype)
				{
					return C1STM8_T_ERROR::C1STM8_RES_EVARTYPMIS;
				}
				v->second.type = vtype;

				if(v->second.dim_num >= 0 && v->second.dim_num != dims / 2)
				{
					return C1STM8_T_ERROR::C1STM8_RES_EVARDIMMIS;
				}
				v->second.dim_num = dims / 2;

				if(v->second.type != B1Types::B1T_UNKNOWN && v->second.is_volatile != is_volatile)
				{
					return C1STM8_T_ERROR::C1STM8_RES_EVARTYPMIS;
				}
				v->second.is_volatile = is_volatile;
			}

			for(auto a = cmd.args.begin() + dims_off; a != cmd.args.end(); a++)
			{
				auto err = check_arg(*a);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}

				if(is_ma || check_sizes)
				{
					if(a->size() > 1)
					{
						if(is_ma)
						{
							return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
						}
						else
						{
							exp_alloc.find(vname)->second++;
							v->second.dims.clear();
							check_sizes = false;
							continue;
						}
					}

					int32_t n;

					auto err = Utils::str2int32((*a)[0].value, n);
					if(err != B1_RES_OK)
					{
						if(is_ma)
						{
							return static_cast<C1STM8_T_ERROR>(err);
						}
						else
						{
							exp_alloc.find(vname)->second++;
							v->second.dims.clear();
							check_sizes = false;
							continue;
						}
					}

					v->second.dims.push_back(n);
				}
			}

			continue;
		}

		if(cmd.cmd == L"GF")
		{
			const auto &vname = cmd.args[0][0].value;

			// variable name can't be one from the predifined constants list
			if(Utils::check_const_name(vname))
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_EIDINUSE);
			}

			auto v = _vars.find(vname);
			if(v == _vars.end())
			{
				if(_vars_order_set.find(vname) == _vars_order_set.end())
				{
					_vars_order_set.insert(vname);
					_vars_order.push_back(vname);
				}
				_vars[vname] = B1_CMP_VAR(vname, B1Types::B1T_UNKNOWN, 0, false, _curr_src_file_id, _curr_line_cnt);
			}

			continue;
		}

		if(cmd.cmd == L"OUT")
		{
			auto err = check_arg(cmd.args[1]);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"IN")
		{
			auto err = check_arg(cmd.args[1]);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"RETVAL")
		{
			auto err = check_arg(cmd.args[0]);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"READ")
		{
			auto err = check_arg(cmd.args[1]);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"SET")
		{
			auto err = check_arg(cmd.args[1]);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"IOCTL")
		{
			if(cmd.args.size() > 2)
			{
				auto dev_name = _global_settings.GetIoDeviceName(cmd.args[0][0].value.substr(1, cmd.args[0][0].value.length() - 2));
				auto cmd_name = cmd.args[1][0].value.substr(1, cmd.args[1][0].value.length() - 2);
				Settings::IoCmd iocmd;
				if(!_global_settings.GetIoCmd(dev_name, cmd_name, iocmd))
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}

				if(iocmd.data_type != B1Types::B1T_LABEL)
				{
					auto err = check_arg(cmd.args[2]);
					if (err != C1STM8_T_ERROR::C1STM8_RES_OK)
					{
						return err;
					}
				}
			}

			continue;
		}

		for(auto &a: cmd.args)
		{
			auto err = check_arg(a);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
		}
	}

	// remove mem. references from variables list
	for(auto &ma: _mem_areas)
	{
		_vars.erase(ma.first);

		// leave static variables only
		if(!ma.second.use_symbol || ma.second.symbol != ma.first)
		{
			auto vo = std::find(_vars_order.cbegin(), _vars_order.cend(), ma.first);
			if(vo != _vars_order.cend())
			{
				_vars_order.erase(vo);
			}
			_vars_order_set.erase(ma.first);
		}
	}

	for(auto &var: _vars)
	{
		auto ea = exp_alloc.find(var.second.name);

		var.second.fixed_size = (ea == exp_alloc.end());

		// implicitly allocated variables
		if(var.second.fixed_size)
		{
			for(int d = 0; d < var.second.dim_num; d++)
			{
				var.second.dims.push_back(b1_opt_base_val);
				var.second.dims.push_back(10);
			}
		}
		else
		// OPTION EXPLICIT and single GA (DIM) with fixed sizes
		if(b1_opt_explicit_val != 0 && ea->second == 1)
		{
			var.second.fixed_size = true;
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::process_imm_str_value(const B1_CMP_ARG &arg)
{
	for(auto &a: arg)
	{
		if(B1CUtils::is_str_val(a.value))
		{
			if(_str_labels.find(a.value) == _str_labels.cend())
			{
				const auto label = L"__STR_" + std::to_wstring(_str_labels.size());
				_str_labels[a.value] = std::make_tuple(label, false, _curr_src_file_id, _curr_line_cnt);
				_req_symbols.insert(label);
			}
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

// build label list for all imm. string values (__STR_XXX labels)
C1STM8_T_ERROR C1STM8Compiler::process_imm_str_values()
{
	for(const auto &cmd: *this)
	{
		_curr_src_file_id = cmd.src_file_id;
		_curr_line_cnt = cmd.line_cnt;

		if(!B1CUtils::is_cmd(cmd))
		{
			continue;
		}

		if(cmd.cmd == L"GA" || cmd.cmd == L"MA")
		{
			for(auto a = cmd.args.cbegin() + (cmd.cmd == L"GA" ? 2 : 3); a != cmd.args.cend(); a++)
			{
				auto err = process_imm_str_value(*a);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
			}
			continue;
		}

		if(cmd.cmd == L"OUT")
		{
			auto err = process_imm_str_value(cmd.args[1]);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			continue;
		}

		if(cmd.cmd == L"IN")
		{
			auto err = process_imm_str_value(cmd.args[1]);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"RETVAL")
		{
			auto err = process_imm_str_value(cmd.args[0]);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			continue;
		}

		if(cmd.cmd == L"READ")
		{
			auto err = process_imm_str_value(cmd.args[1]);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			continue;
		}

		if(cmd.cmd == L"SET")
		{
			auto err = process_imm_str_value(cmd.args[1]);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"IOCTL")
		{
			if(cmd.args.size() > 2)
			{
				auto dev_name = _global_settings.GetIoDeviceName(cmd.args[0][0].value.substr(1, cmd.args[0][0].value.length() - 2));
				auto cmd_name = cmd.args[1][0].value.substr(1, cmd.args[1][0].value.length() - 2);
				Settings::IoCmd iocmd;
				if(!_global_settings.GetIoCmd(dev_name, cmd_name, iocmd))
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
				}

				if(!iocmd.predef_only && iocmd.data_type != B1Types::B1T_LABEL)
				{
					auto err = process_imm_str_value(cmd.args[2]);
					if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
					{
						return err;
					}
				}
			}

			continue;
		}

		if(B1CUtils::is_un_op(cmd) || B1CUtils::is_bin_op(cmd) || B1CUtils::is_log_op(cmd))
		{
			for(const auto &a: cmd.args)
			{
				auto err = process_imm_str_value(a);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
			}
			continue;
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::write_data_sec()
{
	B1_ASM_OPS *data = &_page0_sec;

	for(const auto &vn: _vars_order)
	{
		bool is_static = false;
		int32_t size, rep;
		std::wstring type;

		auto v = _mem_areas.find(vn);
		if(v != _mem_areas.end())
		{
			is_static = true;
		}
		else
		{
			v = _vars.find(vn);
			if(v == _vars.end())
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
				return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
			}
		}
		else
		if(is_static)
		{
			if(!B1CUtils::get_asm_type(v->second.type, &type, &size, &rep))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
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
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			// correct size for arrays with known sizes (address only, no dimensions)
			if(v->second.fixed_size)
			{
				size = size / rep;
				rep = 1;
			}
		}

		if(_page0 && _data_size + size > STM8_PAGE0_SIZE)
		{
			_page0 = false;
			data = &_data_sec;
		}

		data->add_lbl(v->first);
		data->add_op(type + (rep == 1 ? std::wstring() : L" (" + std::to_wstring(rep) + L")"));

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
			label = label.empty() ? std::wstring() : (label + L"::");

			label = label + L"__DAT_PTR";
			B1_CMP_VAR var(label, B1Types::B1T_WORD, 0, false, -1, 0);
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

			data->add_lbl(label);
			data->add_data(L"DW");

			_all_symbols.insert(label);

			_data_size += 2;
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::write_const_sec()
{
	_dat_rst_labels.clear();

	// DAT statements
	if(!_data_stmts.empty())
	{
		for(auto const &dt: _data_stmts)
		{
			bool dat_start = true;

			std::wstring name_space = dt.first;
			name_space = name_space.empty() ? std::wstring() : (name_space + L"::");

			for(const auto &i: dt.second)
			{
				const auto &cmd = *i;

				_curr_src_file_id = cmd.src_file_id;
				_curr_line_cnt = cmd.line_cnt;

				if(B1CUtils::is_label(cmd))
				{
					continue;
				}

				if(dat_start)
				{
					_const_sec.add_lbl(name_space + L"__DAT_START");
					_all_symbols.insert(name_space + L"__DAT_START");
					dat_start = false;
				}

				std::wstring dat_label;
				iterator prev = i;
				while(prev != begin() && B1CUtils::is_label(*std::prev(prev)))
				{
					prev--;
					if(dat_label.empty())
					{
						dat_label = L"__DAT_" + std::to_wstring(_dat_rst_labels.size());
						_const_sec.add_lbl(dat_label);
						_all_symbols.insert(dat_label);
					}
					_dat_rst_labels[prev->cmd] = dat_label;
				}

				bool skip_nmspc = true;

				if(_out_src_lines)
				{
					_const_sec.add_comment(Utils::str_trim(_src_lines[i->src_line_id]));
				}

				for(const auto &a: cmd.args)
				{
					if(skip_nmspc)
					{
						skip_nmspc = false;
						continue;
					}

					if(a[0].type == B1Types::B1T_STRING)
					{
						auto err = process_imm_str_value(a);
						if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
						{
							return err;
						}

						_const_sec.add_data(L"DW " + std::get<0>(_str_labels[a[0].value]));
						_const_size += 2;
					}
					else
					{
						std::wstring asmtype;
						int32_t size;

						// store bytes as words (for all types to be 2 bytes long, to simplify READ statement)
						if(!B1CUtils::get_asm_type(a[0].type == B1Types::B1T_BYTE ? B1Types::B1T_WORD : a[0].type, &asmtype, &size))
						{
							return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
						}

						_const_sec.add_data(asmtype + L" " + a[0].value);
						_const_size += size;
					}
				}
			}
		}
	}

	if(_str_labels.size() > 0)
	{
		for(auto &sl: _str_labels)
		{
			std::wstring sdata;

			if(std::get<1>(sl.second))
			{
				continue;
			}

			_curr_src_file_id = std::get<2>(sl.second);
			_curr_line_cnt = std::get<3>(sl.second);

			auto err = B1CUtils::get_string_data(sl.first, sdata);
			if(err != B1_RES_OK)
			{
				return static_cast<C1STM8_T_ERROR>(err);
			}

			if(sdata.length() > B1C_T_CONST::B1C_MAX_STR_LEN)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESTRLONG);
			}

			_const_sec.add_lbl(std::get<0>(sl.second));
			std::get<1>(sl.second) = true;
			_all_symbols.insert(std::get<0>(sl.second));

			_const_sec.add_data(L"DB " + Utils::str_tohex16(sdata.length()) + L", " + sl.first);
			_const_size += sdata.length();
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::calc_array_size(const B1_CMP_VAR &var, int32_t size1)
{
	if(var.fixed_size)
	{
		int32_t arr_size = 1;

		for(int32_t i = 0; i < var.dim_num; i++)
		{
			arr_size *= (var.dims[i * 2 + 1] - var.dims[i * 2] + 1);
		}

		arr_size *= size1;

		_curr_code_sec->add_op(L"LDW X, " + Utils::str_tohex16(arr_size)); //AE WORD_VALUE
	}
	else
	{
		_curr_code_sec->add_op(L"LDW X, (" + var.name + L" + 0x4)"); //BE SHORT_ADDRESS, CE LONG_ADDRESS

		for(int32_t i = 1; i < var.dim_num; i++)
		{
			_curr_code_sec->add_op(L"PUSHW X"); //89
			_stack_ptr += 2;
			_curr_code_sec->add_op(L"LDW X, (" + var.name + L" + " + Utils::str_tohex16(4 * i + 4) + L")"); //BE SHORT_ADDRESS, CE LONG_ADDRESS
			_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_COM_MUL16"); //AD SIGNED_BYTE_OFFSET (CALLR)
			_req_symbols.insert(L"__LIB_COM_MUL16");
			_curr_code_sec->add_op(L"ADDW SP, 2"); //5B BYTE_VALUE
			_stack_ptr -= 2;
		}

		if(size1 == 2)
		{
			_curr_code_sec->add_op(L"SLAW X"); //58
		}
		else
		if(size1 == 4)
		{
			_curr_code_sec->add_op(L"SLAW X"); //58
			_curr_code_sec->add_op(L"SLAW X"); //58
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::stm8_st_gf(const B1_CMP_VAR &var, bool is_ma)
{
	int32_t size1;

	if(!B1CUtils::get_asm_type(var.type, nullptr, &size1))
	{
		return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
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
			_curr_code_sec->add_op(L"MOV (" + v + L"), 0"); //35 00 LONG_ADDRESS
		}
		else
		{
			// other types are 2-byte for STM8
			if(var.type == B1Types::B1T_STRING)
			{
				// release string
				_curr_code_sec->add_op(L"LDW X, (" + v + L")"); //BE SHORT_ADDRESS, CE LONG_ADDRESS
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_STR_RLS");
			}
			_curr_code_sec->add_op(L"CLRW X"); //5F
			_curr_code_sec->add_op(L"LDW (" + v + L"), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
			if(var.type == B1Types::B1T_LONG)
			{
				_curr_code_sec->add_op(L"LDW (" + v + L" + 2), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
			}
		}
	}
	else
	{
		// array
		if(is_ma || var.type == B1Types::B1T_STRING)
		{
			auto err = calc_array_size(var, size1);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			_curr_code_sec->add_op(L"PUSHW X"); //89
			_stack_ptr += 2;
		}

		if(var.type == B1Types::B1T_STRING)
		{
			if(is_ma)
			{
				_curr_code_sec->add_op(L"LDW X, " + v); //AE WORD_VALUE
			}
			else
			{
				_curr_code_sec->add_op(L"LDW X, (" + v + L")"); //BE SHORT_ADDRESS, CE LONG_ADDRESS
			}
			_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_ARR_DAT_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
			_req_symbols.insert(L"__LIB_STR_ARR_DAT_RLS");
		}

		if(is_ma)
		{
			_curr_code_sec->add_op(L"LDW X, " + v); //AE WORD_VALUE
			_curr_code_sec->add_op(L"PUSH 0"); //4B BYTE_VALUE
			_stack_ptr += 1;
			_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_MEM_SET"); //AD SIGNED_BYTE_OFFSET (CALLR)
			_req_symbols.insert(L"__LIB_MEM_SET");
			_curr_code_sec->add_op(L"ADDW SP, 3"); //5B BYTE_VALUE
			_stack_ptr -= 3;
		}
		else
		{
			_curr_code_sec->add_op(L"LDW X, (" + v + L")"); //BE SHORT_ADDRESS, CE LONG_ADDRESS
			_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_MEM_FRE"); //AD SIGNED_BYTE_OFFSET (CALLR)
			_req_symbols.insert(L"__LIB_MEM_FRE");
			_curr_code_sec->add_op(L"CLRW X"); //5F
			_curr_code_sec->add_op(L"LDW (" + v + L"), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
			if(var.type == B1Types::B1T_STRING)
			{
				_curr_code_sec->add_op(L"POPW X"); //85
				_stack_ptr -= 2;
			}
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::stm8_arrange_types(const B1Types type_from, const B1Types type_to)
{
	if(type_from != type_to)
	{
		if(type_from == B1Types::B1T_BYTE)
		{
			if(type_to == B1Types::B1T_LONG)
			{
				//A -> Y:X
				_curr_code_sec->add_op(L"CLRW Y"); //90 5F
			}
			// A -> X
			_curr_code_sec->add_op(L"CLRW X"); //5F
			_curr_code_sec->add_op(L"LD XL, A"); //97

			if(type_to == B1Types::B1T_STRING)
			{
				// BYTE to STRING
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_I"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_STR_STR_I");
			}
		}
		else
		if(type_from == B1Types::B1T_INT || type_from == B1Types::B1T_WORD)
		{
			if(type_to == B1Types::B1T_BYTE)
			{
				// X -> A
				_curr_code_sec->add_op(L"LD A, XL"); //9F
			}
			else
			if(type_to == B1Types::B1T_LONG)
			{
				// X -> Y:X
				_curr_code_sec->add_op(L"CLRW Y"); //90 5F
				if(type_from == B1Types::B1T_INT)
				{
					_curr_code_sec->add_op(L"TNZW X"); //5D
					const auto label = emit_label(true);
					_curr_code_sec->add_op(L"JRPL " + label); //2A SIGNED_BYTE_OFFSET
					_req_symbols.insert(label);
					_curr_code_sec->add_op(L"DECW Y"); //90 5A
					_curr_code_sec->add_lbl(label);
					_all_symbols.insert(label);
				}
			}
			else
			if(type_to == B1Types::B1T_STRING)
			{
				if(type_from == B1Types::B1T_INT)
				{
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_I"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_STR_I");
				}
				else
				{
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_W"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_STR_W");
				}
			}
		}
		else
		if(type_from == B1Types::B1T_LONG)
		{
			if(type_to == B1Types::B1T_BYTE)
			{
				// Y:X -> A
				_curr_code_sec->add_op(L"LD A, XL"); //9F
			}
			else
			if(type_to == B1Types::B1T_STRING)
			{
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_L"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_STR_STR_L");
			}
		}
		else
		{
			// string, can't convert to any other type
			return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::stm8_load_from_stack(int32_t offset, const B1Types init_type, const B1Types req_type, LVT req_valtype, LVT &rvt, std::wstring &rv)
{
	if(offset < 0 || offset > 255)
	{
		return C1STM8_T_ERROR::C1STM8_RES_ESTCKOVF;
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

			_curr_code_sec->add_op(L"LD A, (" + Utils::str_tohex16(offset) + L", SP)"); //7B BYTE_OFFSET

			if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD || req_type == B1Types::B1T_STRING)
			{
				_curr_code_sec->add_op(L"CLRW X"); //5F
				_curr_code_sec->add_op(L"LD XL, A"); //97
				if(req_type == B1Types::B1T_STRING)
				{
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_I"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_STR_I");
				}
			}
			else
			if(req_type == B1Types::B1T_LONG)
			{
				_curr_code_sec->add_op(L"CLRW Y"); //90 5F
				_curr_code_sec->add_op(L"CLRW X"); //5F
				_curr_code_sec->add_op(L"LD XL, A"); //97
			}
		}
		else
		{
			return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
		}
	}
	else
	if(init_type == B1Types::B1T_INT || init_type == B1Types::B1T_WORD)
	{
		offset += (req_type == B1Types::B1T_BYTE) ? 1 : 0;
		if(offset > 255)
		{
			return C1STM8_T_ERROR::C1STM8_RES_ESTCKOVF;
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
				_curr_code_sec->add_op(L"LD A, (" + Utils::str_tohex16(offset) + L", SP)"); //7B BYTE_OFFSET
			}
			else
			{
				if(req_type == B1Types::B1T_LONG)
				{
					_curr_code_sec->add_op(L"CLRW Y"); //90 5F
				}

				_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)"); //1E BYTE_OFFSET

				if(req_type == B1Types::B1T_STRING)
				{
					if(init_type == B1Types::B1T_INT)
					{
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_I"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_STR_I");
					}
					else
					{
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_W"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_STR_W");
					}
				}
				else
				if(req_type == B1Types::B1T_LONG)
				{
					if(init_type == B1Types::B1T_INT)
					{
						const auto label = emit_label(true);
						_curr_code_sec->add_op(L"JRPL " + label); //2A SIGNED_BYTE_OFFSET
						_req_symbols.insert(label);
						_curr_code_sec->add_op(L"DECW Y"); //90 5A
						_curr_code_sec->add_lbl(label);
						_all_symbols.insert(label);
					}
				}
			}
		}
		else
		{
			return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
		}
	}
	else
	if(init_type == B1Types::B1T_LONG)
	{
		offset += (req_type == B1Types::B1T_BYTE) ? 3 : ((req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD) ? 2 : 0);
		if(offset > (req_type == B1Types::B1T_LONG ? 253 : 255))
		{
			return C1STM8_T_ERROR::C1STM8_RES_ESTCKOVF;
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
				_curr_code_sec->add_op(L"LD A, (" + Utils::str_tohex16(offset) + L", SP)"); //7B BYTE_OFFSET
			}
			else
			if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
			{
				_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)"); //1E BYTE_OFFSET
			}
			else
			if(req_type == B1Types::B1T_STRING)
			{
				_curr_code_sec->add_op(L"LDW Y, (" + Utils::str_tohex16(offset) + L", SP)"); //16 BYTE_OFFSET
				_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset + 2) + L", SP)"); //1E BYTE_OFFSET
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_L"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_STR_STR_L");
			}
			else
			{
				_curr_code_sec->add_op(L"LDW Y, (" + Utils::str_tohex16(offset) + L", SP)"); //16 BYTE_OFFSET
				_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset + 2) + L", SP)"); //1E BYTE_OFFSET
			}
		}
		else
		{
			return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
		}
	}
	else
	{
		// string
		if(req_type != B1Types::B1T_STRING)
		{
			return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
		}

		if(req_valtype & LVT::LVT_REG)
		{
			rvt = LVT::LVT_REG;
			// STRING variable, copy value
			_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)"); //1E BYTE_OFFSET
			_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_CPY"); //AD SIGNED_BYTE_OFFSET (CALLR)
			_req_symbols.insert(L"__LIB_STR_CPY");
		}
		else
		{
			return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::stm8_load(const B1_TYPED_VALUE &tv, const B1Types req_type, LVT req_valtype, LVT *res_valtype /*= nullptr*/, std::wstring *res_val /*= nullptr*/)
{
	std::wstring rv;
	LVT rvt;
	auto init_type = tv.type;

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
					_curr_code_sec->add_op(L"LD A, " + tv.value); //A6 BYTE_VALUE
				}
				else
				if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
				{
					_curr_code_sec->add_op(L"LDW X, " + tv.value); //AE WORD_VALUE
				}
				else
				if(req_type == B1Types::B1T_LONG)
				{
					_curr_code_sec->add_op(L"LDW Y, " + tv.value + L".h"); //90 AE WORD_VALUE
					_curr_code_sec->add_op(L"LDW X, " + tv.value + L".l"); //AE WORD_VALUE
				}
				else
				{
					_curr_code_sec->add_op(L"LDW X, " + tv.value); //AE WORD_VALUE
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_I"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_STR_I");
				}
			}
			else
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
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
					_curr_code_sec->add_op(L"LD A, " + tv.value); //A6 BYTE_VALUE
				}
				else
				if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
				{
					_curr_code_sec->add_op(L"LDW X, " + tv.value); //AE WORD_VALUE
				}
				else
				if(req_type == B1Types::B1T_LONG)
				{
					_curr_code_sec->add_op(L"LDW Y, " + tv.value + L".h"); //90 AE WORD_VALUE
					_curr_code_sec->add_op(L"LDW X, " + tv.value + L".l"); //AE WORD_VALUE
				}
				else
				{
					_curr_code_sec->add_op(L"LDW X, " + tv.value); //AE WORD_VALUE
					if(init_type == B1Types::B1T_INT)
					{
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_I"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_STR_I");
					}
					else
					{
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_W"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_STR_W");
					}
				}
			}
			else
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
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
					_curr_code_sec->add_op(L"LD A, " + tv.value); //A6 BYTE_VALUE
				}
				else
				if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
				{
					_curr_code_sec->add_op(L"LDW X, " + tv.value); //AE WORD_VALUE
				}
				else
				if(req_type == B1Types::B1T_LONG)
				{
					_curr_code_sec->add_op(L"LDW Y, " + tv.value + L".h"); //90 AE WORD_VALUE
					_curr_code_sec->add_op(L"LDW X, " + tv.value + L".l"); //AE WORD_VALUE
				}
				else
				{
					_curr_code_sec->add_op(L"LDW Y, " + tv.value + L".h"); //90 AE WORD_VALUE
					_curr_code_sec->add_op(L"LDW X, " + tv.value + L".l"); //AE WORD_VALUE
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_L"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_STR_L");
				}
			}
			else
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
			}
		}
		else
		if(init_type == B1Types::B1T_STRING)
		{
			if(req_type != B1Types::B1T_STRING)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
			}

			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;
				_curr_code_sec->add_op(L"LDW X, " + std::get<0>(_str_labels[tv.value])); //AE WORD_VALUE
				_req_symbols.insert(std::get<0>(_str_labels[tv.value]));
			}
			else
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
			}
		}
		else
		{
			return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
		}
	}
	else
	if(_locals.find(tv.value) != _locals.end())
	{
		// local variable
		int32_t offset = -1;

		for(const auto &loc: _local_offset)
		{
			if(loc.first.value == tv.value)
			{
				offset = _stack_ptr - loc.second;
			}
		}

		auto err = stm8_load_from_stack(offset, init_type, req_type, req_valtype, rvt, rv);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			return err;
		}
	}
	else
	if(B1CUtils::is_fn_arg(tv.value))
	{
		int32_t offset = -1;

		if(_curr_udef_arg_offsets.size() == 1)
		{
			// temporary solution for a single argument case: function prologue code stores it in stack
			offset = _stack_ptr - _curr_udef_args_size + 1;
		}
		else
		{
			int arg_num = B1CUtils::get_fn_arg_index(tv.value);
			int arg_off = _curr_udef_arg_offsets[arg_num];
			offset = _stack_ptr + _global_settings.GetRetAddressSize() + arg_off;
		}

		auto err = stm8_load_from_stack(offset, init_type, req_type, req_valtype, rvt, rv);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			return err;
		}
	}
	else
	{
		auto fn = get_fn(tv);

		if(fn == nullptr)
		{
			// simple variable
			auto ma = _mem_areas.find(tv.value);
			bool is_ma = ma != _mem_areas.end();
			std::wstring str_off;
			int int_off = 0;

			if(init_type == B1Types::B1T_LONG)
			{
				if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
				{
					str_off = L" + 0x2";
					int_off = 2;
				}
				else
				if(req_type == B1Types::B1T_BYTE)
				{
					str_off = L" + 0x3";
					int_off = 3;
				}
			}
			else
			if(init_type == B1Types::B1T_INT || init_type == B1Types::B1T_WORD)
			{
				if(req_type == B1Types::B1T_BYTE)
				{
					str_off = L" + 0x1";
					int_off = 1;
				}
			}

			rv = is_ma ?
				(ma->second.use_symbol ? (ma->second.symbol + str_off) : std::to_wstring(ma->second.address + int_off)) :
				(tv.value + str_off);

			if(!is_ma)
			{
				_req_symbols.insert(tv.value);
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

					_curr_code_sec->add_op(L"LD A, (" + rv + L")"); //B6 SHORT_ADDRESS C6 LONG_ADDRESS

					if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD || req_type == B1Types::B1T_STRING)
					{
						_curr_code_sec->add_op(L"CLRW X"); //5F
						_curr_code_sec->add_op(L"LD XL, A"); //97
						if(req_type == B1Types::B1T_STRING)
						{
							_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_I"); //AD SIGNED_BYTE_OFFSET (CALLR)
							_req_symbols.insert(L"__LIB_STR_STR_I");
						}
					}
					else
					if(req_type == B1Types::B1T_LONG)
					{
						_curr_code_sec->add_op(L"CLRW Y"); //90 5F
						_curr_code_sec->add_op(L"CLRW X"); //5F
						_curr_code_sec->add_op(L"LD XL, A"); //97
					}

					rv.clear();
				}
				else
				{
					return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
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
						_curr_code_sec->add_op(L"LD A, (" + rv + L")"); //B6 SHORT_ADDRESS C6 LONG_ADDRESS
					}
					else
					{
						if(req_type == B1Types::B1T_LONG)
						{
							_curr_code_sec->add_op(L"CLRW Y"); //90 5F
						}

						_curr_code_sec->add_op(L"LDW X, (" + rv + L")"); //BE SHORT_ADDRESS CE LONG_ADDRESS

						if(req_type == B1Types::B1T_STRING)
						{
							if(init_type == B1Types::B1T_INT)
							{
								_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_I"); //AD SIGNED_BYTE_OFFSET (CALLR)
								_req_symbols.insert(L"__LIB_STR_STR_I");
							}
							else
							{
								_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_W"); //AD SIGNED_BYTE_OFFSET (CALLR)
								_req_symbols.insert(L"__LIB_STR_STR_W");
							}
						}
						else
						if(req_type == B1Types::B1T_LONG)
						{
							if(init_type == B1Types::B1T_INT)
							{
								const auto label = emit_label(true);
								_curr_code_sec->add_op(L"JRPL " + label); //2A SIGNED_BYTE_OFFSET
								_req_symbols.insert(label);
								_curr_code_sec->add_op(L"DECW Y"); //90 5A
								_curr_code_sec->add_lbl(label);
								_all_symbols.insert(label);
							}
						}
					}

					rv.clear();
				}
				else
				{
					return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
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
						_curr_code_sec->add_op(L"LD A, (" + rv + L")"); //B6 SHORT_ADDRESS C6 LONG_ADDRESS
					}
					else
					if(req_type == B1Types::B1T_INT || req_type == B1Types::B1T_WORD)
					{
						_curr_code_sec->add_op(L"LDW X, (" + rv + L")"); //BE SHORT_ADDRESS CE LONG_ADDRESS
					}
					else
					if(req_type == B1Types::B1T_STRING)
					{
						_curr_code_sec->add_op(L"LDW Y, (" + rv + L")"); //90 BE SHORT_ADDRESS 90 CE LONG_ADDRESS
						_curr_code_sec->add_op(L"LDW X, (" + rv + L" + 2)"); //BE SHORT_ADDRESS CE LONG_ADDRESS
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_L"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_STR_L");
					}
					else
					if(req_type == B1Types::B1T_LONG)
					{
						_curr_code_sec->add_op(L"LDW Y, (" + rv + L")"); //90 BE SHORT_ADDRESS 90 CE LONG_ADDRESS
						_curr_code_sec->add_op(L"LDW X, (" + rv + L" + 2)"); //BE SHORT_ADDRESS CE LONG_ADDRESS
					}

					rv.clear();
				}
				else
				{
					return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
				}
			}
			else
			{
				if(req_type != B1Types::B1T_STRING)
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
				}

				if(req_valtype & LVT::LVT_REG)
				{
					rvt = LVT::LVT_REG;

					// STRING variable, copy value
					_curr_code_sec->add_op(L"LDW X, (" + rv + L")"); //BE SHORT_ADDRESS CE LONG_ADDRESS
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_CPY"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_CPY");

					rv.clear();
				}
				else
				{
					return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
				}
			}
		}
		else
		{
			// function without arguments
			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				_curr_code_sec->add_op(_call_stmt + L" " + fn->iname); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(fn->iname);

				auto err = stm8_arrange_types(init_type, req_type);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
			}
			else
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
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

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

// allocates array of default size if necessary
C1STM8_T_ERROR C1STM8Compiler::stm8_arr_alloc_def(const B1_CMP_ARG &arg, const B1_CMP_VAR &var)
{
	int32_t size1 = (10 - b1_opt_base_val) + 1;
	int32_t dimnum, size = 1;

	dimnum = arg.size() - 1;

	if(dimnum < 1 || dimnum > B1_MAX_VAR_DIM_NUM)
	{
		return static_cast<C1STM8_T_ERROR>(B1_RES_EWSUBSCNT);
	}

	if((_opt_nocheck && b1_opt_explicit_val != 0) || (!var.is_volatile && _allocated_arrays.find(arg[0].value) != _allocated_arrays.end()))
	{
		return C1STM8_T_ERROR::C1STM8_RES_OK;
	}

	// check if memory is allocated
	const auto label = emit_label(true);
	_curr_code_sec->add_op(L"LDW X, (" + arg[0].value + L")"); //BE SHORT_ADDRESS, CE LONG_ADDRESS
	_req_symbols.insert(arg[0].value);
	_curr_code_sec->add_op(L"JRNE " + label); //26 SIGNED_BYTE_OFFSET
	_req_symbols.insert(label);

	if(b1_opt_explicit_val == 0)
	{
		for(int i = 0; i < dimnum; i++)
		{
			size *= size1;
		}

		if(arg[0].type == B1Types::B1T_BYTE)
		{
			_curr_code_sec->add_op(L"LDW X, " + Utils::str_tohex16(size)); //AE WORD_VALUE
		}
		else
		if(arg[0].type == B1Types::B1T_INT || arg[0].type == B1Types::B1T_WORD || arg[0].type == B1Types::B1T_STRING)
		{
			_curr_code_sec->add_op(L"LDW X, " + Utils::str_tohex16(size * 2)); //AE WORD_VALUE
		}
		else
		{
			// LONG type
			_curr_code_sec->add_op(L"LDW X, " + Utils::str_tohex16(size * 4)); //AE WORD_VALUE
		}

		_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_MEM_ALC"); //AD SIGNED_BYTE_OFFSET (CALLR)
		_req_symbols.insert(L"__LIB_MEM_ALC");

		// save address
		_curr_code_sec->add_op(L"LDW (" + arg[0].value + L"), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS

		// save array sizes if necessary
		if(!var.fixed_size)
		{
			_curr_code_sec->add_op(L"CLRW X"); //5F
			if(b1_opt_base_val == 1)
			{
				_curr_code_sec->add_op(L"INCW X"); //5C
			}
			for(int i = 0; i < dimnum; i++)
			{
				_curr_code_sec->add_op(L"LDW (" + arg[0].value + L" + " + Utils::str_tohex16((i + 1) * 4 - 2) + L"), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
			}

			_curr_code_sec->add_op(L"LDW X, " + Utils::str_tohex16(size1)); //AE WORD_VALUE
			for(int i = 0; i < dimnum; i++)
			{
				_curr_code_sec->add_op(L"LDW (" + arg[0].value + L" + " + Utils::str_tohex16((i + 1) * 4) + L"), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
			}
		}
	}
	else
	{
		_curr_code_sec->add_op(L"MOV (__LIB_ERR_LAST_ERR), " + _RTE_error_names[B1C_T_RTERROR::B1C_RTE_ARR_UNALLOC]); //35 BYTE_VALUE LONG_ADDRESS
		_init_files.push_back(L"__LIB_ERR_LAST_ERR");
		_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_ERR_HANDLER"); //20 SIGNED_BYTE_OFFSET
		_req_symbols.insert(L"__LIB_ERR_HANDLER");
	}

	_curr_code_sec->add_lbl(label);
	_all_symbols.insert(label);

	_allocated_arrays.insert(arg[0].value);

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

// known size and known arguments: no code, offset is returned
// known size and unknown arguments: code, no offset
// if the function sets imm_offset to true, offset variable contains offset from array's base address
C1STM8_T_ERROR C1STM8Compiler::stm8_arr_offset(const B1_CMP_ARG &arg, bool &imm_offset, int32_t &offset)
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
				return static_cast<C1STM8_T_ERROR>(err);
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
		return C1STM8_T_ERROR::C1STM8_RES_OK;
	}

	if(arg.size() == 2)
	{
		// one-dimensional array
		const auto &tv = arg[1];

		auto err = stm8_load(tv, B1Types::B1T_INT, LVT::LVT_REG);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			return err;
		}

		if(known_size)
		{
			if(var->second.dims[0] != 0)
			{
				_curr_code_sec->add_op(L"SUBW X, " + Utils::str_tohex16(var->second.dims[0])); //1D WORD_VALUE
			}
		}
		else
		{
			_curr_code_sec->add_op(L"SUBW X, (" + arg[0].value + L" + " + Utils::str_tohex16(2) + L")"); //72 B0 LONG_ADDRESS
		}
	}
	else
	if(known_size)
	{
		// multidimensional array of fixed size
		dims_size = 1;

		// offset
		_curr_code_sec->add_op(L"PUSHW X"); //89
		_stack_ptr += 2;

		for(int32_t i = (arg.size() - 2); i >= 0; i--)
		{
			const auto &tv = arg[i + 1];

			if(i != (arg.size() - 2))
			{
				_curr_code_sec->add_op(L"LDW X, " + Utils::str_tohex16(dims_size)); //AE WORD_VALUE
				_curr_code_sec->add_op(L"PUSHW X"); //89
				_stack_ptr += 2;
			}

			auto err = stm8_load(tv, B1Types::B1T_INT, LVT::LVT_REG);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			if(var->second.dims[i * 2] != 0)
			{
				_curr_code_sec->add_op(L"SUBW X, " + Utils::str_tohex16(var->second.dims[i * 2])); //1D WORD_VALUE
			}

			if(i != (arg.size() - 2))
			{
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_COM_MUL16"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_COM_MUL16");
				_curr_code_sec->add_op(L"ADDW X, (3, SP)"); //72 FB BYTE_OFFSET
				_curr_code_sec->add_op(L"LDW (3, SP), X"); //1F BYTE_OFFSET
				_curr_code_sec->add_op(L"POPW X"); //85
				_stack_ptr -= 2;
			}
			else
			{
				_curr_code_sec->add_op(L"LDW (1, SP), X"); //1F BYTE_OFFSET
			}

			dims_size *= var->second.dims[i * 2 + 1] - var->second.dims[i * 2] + 1;
		}

		_curr_code_sec->add_op(L"POPW X"); //85
		_stack_ptr -= 2;
	}
	else
	{
		// multidimensional array of any size
		// offset
		_curr_code_sec->add_op(L"CLRW X"); //5F
		_curr_code_sec->add_op(L"PUSHW X"); //89
		_stack_ptr += 2;

		// dimensions size
		_curr_code_sec->add_op(L"INCW X"); //5C
		_curr_code_sec->add_op(L"PUSHW X"); //89
		_stack_ptr += 2;

		for(int32_t i = (arg.size() - 2); i >= 0; i--)
		{
			const auto &tv = arg[i + 1];

			auto err = stm8_load(tv, B1Types::B1T_INT, LVT::LVT_REG);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			_curr_code_sec->add_op(L"SUBW X, (" + arg[0].value + L" + " + Utils::str_tohex16(2 + i * 4) + L")"); //72 B0 LONG_ADDRESS

			if(i != (arg.size() - 2))
			{
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_COM_MUL16"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_COM_MUL16");
				_curr_code_sec->add_op(L"ADDW X, (3, SP)"); //72 FB BYTE_OFFSET
			}
			
			_curr_code_sec->add_op(L"LDW (3, SP), X"); //1F BYTE_OFFSET

			if(i != 0)
			{
				_curr_code_sec->add_op(L"LDW X, (" + arg[0].value + L" + " + Utils::str_tohex16(2 + 2 + i * 4) + L")"); //BE SHORT_ADDRESS CE LONG_ADDRESS
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_COM_MUL16"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_COM_MUL16");
				_curr_code_sec->add_op(L"LDW (1, SP), X"); //1F BYTE_OFFSET
			}
		}

		_curr_code_sec->add_op(L"POPW X"); //85
		_stack_ptr -= 2;
		_curr_code_sec->add_op(L"POPW X"); //85
		_stack_ptr -= 2;
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::stm8_load(const B1_CMP_ARG &arg, const B1Types req_type, LVT req_valtype, LVT *res_valtype /*= nullptr*/, std::wstring *res_val /*= nullptr*/)
{
	if(arg.size() == 1)
	{
		return stm8_load(arg[0], req_type, req_valtype, res_valtype, res_val);
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

		if(is_ma)
		{
			if(ma->second.dim_num != arg.size() - 1)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_EWRARGCNT);
			}
		}
		else
		{
			const auto &var = _vars.find(arg[0].value)->second;

			if(var.dim_num != arg.size() - 1)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_EWRARGCNT);
			}

			// allocate array of default size if necessary
			auto err = stm8_arr_alloc_def(arg, var);
			if(err != static_cast<C1STM8_T_ERROR>(B1_RES_OK))
			{
				return err;
			}

			_req_symbols.insert(arg[0].value);
		}

		// calculate memory offset
		bool imm_offset = false;
		int32_t offset = 0;
		auto err = stm8_arr_offset(arg, imm_offset, offset);
		if(err != static_cast<C1STM8_T_ERROR>(B1_RES_OK))
		{
			return err;
		}

		rv = is_ma ? (ma->second.use_symbol ? ma->second.symbol : std::to_wstring(ma->second.address)) :
			arg[0].value;

		// get value
		if(init_type == B1Types::B1T_BYTE)
		{
			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				if(is_ma)
				{
					if(imm_offset)
					{
						_curr_code_sec->add_op(L"LD A, (" + rv + L" + " + Utils::str_tohex16(offset) + L")"); //B6 SHORT_ADDRESS C6 LONG_ADDRESS
					}
					else
					{
						_curr_code_sec->add_op(L"LD A, (" + rv + L", X)"); //E6 BYTE_OFFSET D6 WORD_OFFSET
					}
				}
				else
				{
					if(imm_offset)
					{
						_curr_code_sec->add_op(L"LDW X, (" + rv + L")"); //BE BYTE_OFFSET CE WORD_OFFSET
						_curr_code_sec->add_op(L"LD A, (" + Utils::str_tohex16(offset) + L", X)"); //E6 BYTE_OFFSET D6 WORD_OFFSET
					}
					else
					{
						_curr_code_sec->add_op(L"LD A, ([" + rv + L"], X)"); //92 D6 BYTE_OFFSET 72 D6 WORD_OFFSET
					}
				}

				rv.clear();

				if(req_type != B1Types::B1T_BYTE)
				{
					if(req_type == B1Types::B1T_LONG)
					{
						_curr_code_sec->add_op(L"CLRW Y"); //90 5F
					}
					_curr_code_sec->add_op(L"CLRW X"); //5F
					_curr_code_sec->add_op(L"LD XL, A"); //97
				}
				if(req_type == B1Types::B1T_STRING)
				{
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_I"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_STR_I");
				}
			}
			else
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
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
				_curr_code_sec->add_op(L"SLAW X"); //58
			}

			if(req_type == B1Types::B1T_BYTE)
			{
				if(imm_offset)
				{
					offset++;
				}
				else
				{
					_curr_code_sec->add_op(L"INCW X"); //5C
				}
			}

			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				if(req_type == B1Types::B1T_BYTE)
				{
					if(is_ma)
					{
						if(imm_offset)
						{
							_curr_code_sec->add_op(L"LD A, (" + rv + L" + " + Utils::str_tohex16(offset) + L")"); //B6 SHORT_ADDRESS C6 LONG_ADDRESS
						}
						else
						{
							_curr_code_sec->add_op(L"LD A, (" + rv + L", X)"); //E6 BYTE_OFFSET D6 WORD_OFFSET
						}
					}
					else
					{
						if(imm_offset)
						{
							_curr_code_sec->add_op(L"LDW X, (" + rv + L")"); //BE BYTE_OFFSET CE WORD_OFFSET
							_curr_code_sec->add_op(L"LD A, (" + Utils::str_tohex16(offset) + L", X)"); //E6 BYTE_OFFSET D6 WORD_OFFSET
						}
						else
						{
							_curr_code_sec->add_op(L"LD A, ([" + rv + L"], X)"); //92 D6 BYTE_OFFSET 72 D6 WORD_OFFSET
						}
					}
				}
				else
				{
					if(req_type == B1Types::B1T_LONG)
					{
						_curr_code_sec->add_op(L"CLRW Y"); //90 5F
					}

					if(is_ma)
					{
						if(imm_offset)
						{
							_curr_code_sec->add_op(L"LDW X, (" + rv + L" + " + Utils::str_tohex16(offset) + L")"); //BE SHORT_ADDRESS CE LONG_ADDRESS
						}
						else
						{
							_curr_code_sec->add_op(L"LDW X, (" + rv + L", X)"); //EE BYTE_OFFSET DE WORD_OFFSET
						}
					}
					else
					{
						if(imm_offset)
						{
							_curr_code_sec->add_op(L"LDW X, (" + rv + L")"); //BE BYTE_OFFSET CE WORD_OFFSET
							_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", X)"); //EE BYTE_OFFSET DE WORD_OFFSET
						}
						else
						{
							_curr_code_sec->add_op(L"LDW X, ([" + rv + L"], X)"); //92 DE BYTE_OFFSET 72 DE WORD_OFFSET
						}
					}

					if(req_type == B1Types::B1T_LONG)
					{
						if(init_type == B1Types::B1T_INT)
						{
							const auto label = emit_label(true);
							_curr_code_sec->add_op(L"JRPL " + label); //2A SIGNED_BYTE_OFFSET
							_req_symbols.insert(label);
							_curr_code_sec->add_op(L"DECW Y"); //90 5A
							_curr_code_sec->add_lbl(label);
							_all_symbols.insert(label);
						}
					}
					else
					if(req_type == B1Types::B1T_STRING)
					{
						if(init_type == B1Types::B1T_INT)
						{
							_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_I"); //AD SIGNED_BYTE_OFFSET (CALLR)
							_req_symbols.insert(L"__LIB_STR_STR_I");
						}
						else
						{
							_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_W"); //AD SIGNED_BYTE_OFFSET (CALLR)
							_req_symbols.insert(L"__LIB_STR_STR_W");
						}
					}
				}

				rv.clear();
			}
			else
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
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
				_curr_code_sec->add_op(L"SLAW X"); //58
				_curr_code_sec->add_op(L"SLAW X"); //58
			}

			if(req_type == B1Types::B1T_BYTE)
			{
				if(imm_offset)
				{
					offset += 3;
				}
				else
				{
					_curr_code_sec->add_op(L"ADDW X, 3"); //5C
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
					_curr_code_sec->add_op(L"INCW X"); //5C
					_curr_code_sec->add_op(L"INCW X"); //5C
				}
			}

			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				if(req_type == B1Types::B1T_BYTE)
				{
					if(is_ma)
					{
						if(imm_offset)
						{
							_curr_code_sec->add_op(L"LD A, (" + rv + L" + " + Utils::str_tohex16(offset) + L")"); //B6 SHORT_ADDRESS C6 LONG_ADDRESS
						}
						else
						{
							_curr_code_sec->add_op(L"LD A, (" + rv + L", X)"); //E6 BYTE_OFFSET D6 WORD_OFFSET
						}
					}
					else
					{
						if(imm_offset)
						{
							_curr_code_sec->add_op(L"LDW X, (" + rv + L")"); //BE BYTE_OFFSET CE WORD_OFFSET
							_curr_code_sec->add_op(L"LD A, (" + Utils::str_tohex16(offset) + L", X)"); //E6 BYTE_OFFSET D6 WORD_OFFSET
						}
						else
						{
							_curr_code_sec->add_op(L"LD A, ([" + rv + L"], X)"); //92 D6 BYTE_OFFSET 72 D6 WORD_OFFSET
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
							_curr_code_sec->add_op(L"LDW X, (" + rv + L" + " + Utils::str_tohex16(offset) + L")"); //BE SHORT_ADDRESS CE LONG_ADDRESS
						}
						else
						{
							_curr_code_sec->add_op(L"LDW X, (" + rv + L", X)"); //EE BYTE_OFFSET DE WORD_OFFSET
						}
					}
					else
					{
						if(imm_offset)
						{
							_curr_code_sec->add_op(L"LDW X, (" + rv + L")"); //BE BYTE_OFFSET CE WORD_OFFSET
							_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", X)"); //EE BYTE_OFFSET DE WORD_OFFSET
						}
						else
						{
							_curr_code_sec->add_op(L"LDW X, ([" + rv + L"], X)"); //92 DE BYTE_OFFSET 72 DE WORD_OFFSET
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
							_curr_code_sec->add_op(L"LDW Y, (" + rv + L" + " + Utils::str_tohex16(offset) + L")"); //90 BE SHORT_ADDRESS 90 CE LONG_ADDRESS
							_curr_code_sec->add_op(L"LDW X, (" + rv + L" + " + Utils::str_tohex16(offset) + L" + 2)"); //BE SHORT_ADDRESS CE LONG_ADDRESS
						}
						else
						{
							_curr_code_sec->add_op(L"LDW Y, X"); //90 93
							_curr_code_sec->add_op(L"LDW Y, (" + rv + L", Y)"); //90 EE BYTE_OFFSET 90 DE WORD_OFFSET
							_curr_code_sec->add_op(L"LDW X, (" + rv + L" + 2, X)"); //EE BYTE_OFFSET DE WORD_OFFSET
						}
					}
					else
					{
						if(imm_offset)
						{
							_curr_code_sec->add_op(L"LDW X, (" + rv + L")"); //BE BYTE_OFFSET CE WORD_OFFSET
							_curr_code_sec->add_op(L"LDW Y, X"); //90 93
							_curr_code_sec->add_op(L"LDW Y, (" + Utils::str_tohex16(offset) + L", Y)"); //90 EE BYTE_OFFSET 90 DE WORD_OFFSET
							_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L" + 2, X)"); //EE BYTE_OFFSET DE WORD_OFFSET
						}
						else
						{
							_curr_code_sec->add_op(L"ADDW X, (" + rv + L")"); //72 BB WORD_OFFSET
							_curr_code_sec->add_op(L"LDW Y, X"); //90 93
							_curr_code_sec->add_op(L"LDW Y, (Y)"); //90 FE
							_curr_code_sec->add_op(L"LDW X, (2, X)"); //EE BYTE_OFFSET
						}
					}


					if(req_type == B1Types::B1T_STRING)
					{
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR_L"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_STR_L");
					}
				}

				rv.clear();
			}
			else
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
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
				_curr_code_sec->add_op(L"SLAW X"); //58
			}

			if(req_type != B1Types::B1T_STRING)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
			}

			if(req_valtype & LVT::LVT_REG)
			{
				rvt = LVT::LVT_REG;

				if(imm_offset)
				{
					_curr_code_sec->add_op(L"LDW X, (" + rv + L")"); //BE BYTE_OFFSET CE WORD_OFFSET
					_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", X)"); //EE BYTE_OFFSET DE WORD_OFFSET
				}
				else
				{
					_curr_code_sec->add_op(L"LDW X, ([" + rv + L"], X)"); //92 DE BYTE_OFFSET 72 DE WORD_OFFSET
				}

				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_CPY"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_STR_CPY");

				rv.clear();
			}
			else
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
			}
		}
	}
	else
	{
		// function call
		
		// check for special data type conversion functions (inline)
		if(fn->args.size() == 1 && fn->isstdfn && fn->iname.empty())
		{
			if((fn->name == L"CBYTE" || fn->name == L"CINT" || fn->name == L"CWRD" || fn->name == L"CLNG") && (req_valtype & LVT::LVT_REG))
			{
				auto err = stm8_load(arg[1], fn->rettype, LVT::LVT_REG);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}

				if(fn->rettype != req_type)
				{
					auto err = stm8_arrange_types(fn->rettype, req_type);
					if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
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

				return C1STM8_T_ERROR::C1STM8_RES_OK;
			}

			return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
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

			auto err = stm8_load(arg[arg_ind + 1], farg.type, LVT::LVT_REG | LVT::LVT_IMMVAL, &lvt, &res_val);
			if(err != static_cast<C1STM8_T_ERROR>(B1_RES_OK))
			{
				return err;
			}

			if(lvt == LVT::LVT_IMMVAL)
			{
				if(farg.type == B1Types::B1T_BYTE)
				{
					_curr_code_sec->add_op(L"PUSH " + res_val); //4B BYTE_VALUE
					_stack_ptr++;
					args_size++;
				}
				else
				{
					_curr_code_sec->add_op(L"PUSH " + res_val + L".ll"); //4B BYTE_VALUE
					_curr_code_sec->add_op(L"PUSH " + res_val + L".lh"); //4B BYTE_VALUE
					_stack_ptr += 2;
					args_size += 2;

					if(req_type == B1Types::B1T_LONG)
					{
						_curr_code_sec->add_op(L"PUSH " + res_val + L".hl"); //4B BYTE_VALUE
						_curr_code_sec->add_op(L"PUSH " + res_val + L".hh"); //4B BYTE_VALUE
						_stack_ptr += 2;
						args_size += 2;
					}
				}
			}
			else
			{
				if(farg.type == B1Types::B1T_BYTE)
				{
					_curr_code_sec->add_op(L"PUSH A"); //88
					_stack_ptr++;
					args_size++;
				}
				else
				{
					_curr_code_sec->add_op(L"PUSHW X"); //89
					_stack_ptr += 2;
					args_size += 2;

					if(req_type == B1Types::B1T_LONG)
					{
						_curr_code_sec->add_op(L"PUSHW Y"); //90 89
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
			if(err != static_cast<C1STM8_T_ERROR>(B1_RES_OK))
			{
				return err;
			}
		}

		if(req_valtype & LVT::LVT_REG)
		{
			rvt = LVT::LVT_REG;
			_curr_code_sec->add_op(_call_stmt + L" " + fn->iname); //AD SIGNED_BYTE_OFFSET (CALLR)
			_req_symbols.insert(fn->iname);

			if(fn->args.size() > 1)
			{
				// cleanup stack
				_curr_code_sec->add_op(L"ADDW SP, " + Utils::str_tohex16(args_size)); //5B BYTE_VALUE
				_stack_ptr -= args_size;
			}

			auto err = stm8_arrange_types(init_type, req_type);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
		}
		else
		{
			return C1STM8_T_ERROR::C1STM8_RES_EINTERR;
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

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::stm8_init_array(const B1_CMP_CMD &cmd, const B1_CMP_VAR &var)
{
	int32_t data_size;

	if(!B1CUtils::get_asm_type(cmd.args[1][0].type, nullptr, &data_size))
	{
		return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
	}

	_req_symbols.insert(var.name);

	if(var.fixed_size)
	{
		auto err = calc_array_size(var, data_size);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
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
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			// save lbound
			_curr_code_sec->add_op(L"LDW (" + cmd.args[0][0].value + L" + " + Utils::str_tohex16((i * 2 + 1) * 2) + L"), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS

			// ubound
			err = stm8_load(cmd.args[2 + i * 2 + 1], B1Types::B1T_INT, LVT::LVT_REG);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			// subtract lbound value
			_curr_code_sec->add_op(L"SUBW X, (" + cmd.args[0][0].value + L" + " + Utils::str_tohex16((i * 2 + 1) * 2) + L")"); //72 B0 WORD_OFFSET
			_curr_code_sec->add_op(L"INCW X"); //5C
			// save dimension size
			_curr_code_sec->add_op(L"LDW (" + cmd.args[0][0].value + L" + " + Utils::str_tohex16((i * 2 + 2) * 2) + L"), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS

			if(i != 0)
			{
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_COM_MUL16"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_COM_MUL16");
				
				if(i == dims - 1)
				{
					_curr_code_sec->add_op(L"ADDW SP, 2"); //5B BYTE_VALUE
					_stack_ptr -= 2;
				}
				else
				{
					_curr_code_sec->add_op(L"LDW (1, SP), X"); //1F BYTE_OFFSET
				}
			}

			if(i == 0 && i != dims - 1)
			{
				_curr_code_sec->add_op(L"PUSHW X"); //89
				_stack_ptr += 2;
			}
		}

		if(data_size == 2)
		{
			// 2-byte types: data size = arr. size * 2
			_curr_code_sec->add_op(L"SLAW X"); //58
		}
		else
		if(data_size == 4)
		{
			// 4-byte types: data size = arr. size * 4
			_curr_code_sec->add_op(L"SLAW X"); //58
			_curr_code_sec->add_op(L"SLAW X"); //58
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::stm8_st_ga(const B1_CMP_CMD &cmd, const B1_CMP_VAR &var)
{
	// report error if the array is already allocated
	_curr_code_sec->add_op(L"LDW X, (" + cmd.args[0][0].value + L")"); //BE SHORT_ADDRESS CE LONG_ADDRESS
	_req_symbols.insert(cmd.args[0][0].value);
	const auto label = emit_label(true);
	_curr_code_sec->add_op(L"JREQ " + label); //27 SIGNED_BYTE_OFFSET
	_req_symbols.insert(label);
	_curr_code_sec->add_op(L"MOV (__LIB_ERR_LAST_ERR), " + _RTE_error_names[B1C_T_RTERROR::B1C_RTE_ARR_ALLOC]); //35 BYTE_VALUE LONG_ADDRESS
	_init_files.push_back(L"__LIB_ERR_LAST_ERR");
	_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_ERR_HANDLER"); //AD SIGNED_BYTE_OFFSET (CALLR)
	_req_symbols.insert(L"__LIB_ERR_HANDLER");
	_curr_code_sec->add_lbl(label);
	_all_symbols.insert(label);

	auto err = stm8_init_array(cmd, var);
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_MEM_ALC"); //AD SIGNED_BYTE_OFFSET (CALLR)
	_req_symbols.insert(L"__LIB_MEM_ALC");

	// save address
	_curr_code_sec->add_op(L"LDW (" + cmd.args[0][0].value + L"), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::stm8_store(const B1_TYPED_VALUE &tv)
{
	if(Utils::check_const_name(tv.value))
	{
		return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
	}

	if(_locals.find(tv.value) != _locals.end())
	{
		// local variable
		int32_t offset = -1;

		for(const auto &loc: _local_offset)
		{
			if(loc.first.value == tv.value)
			{
				offset = _stack_ptr - loc.second;
			}
		}

		if(offset < 0 || offset > (tv.type == B1Types::B1T_LONG ? 253 : 255))
		{
			return C1STM8_T_ERROR::C1STM8_RES_ESTCKOVF;
		}

		if(tv.type == B1Types::B1T_BYTE)
		{
			_curr_code_sec->add_op(L"LD (" + Utils::str_tohex16(offset) + L", SP), A"); //6B BYTE_OFFSET
		}
		else
		if(tv.type == B1Types::B1T_INT || tv.type == B1Types::B1T_WORD)
		{
			_curr_code_sec->add_op(L"LDW (" + Utils::str_tohex16(offset) + L", SP), X"); //1F BYTE_OFFSET
		}
		else
		if(tv.type == B1Types::B1T_LONG)
		{
			_curr_code_sec->add_op(L"LDW (" + Utils::str_tohex16(offset) + L", SP), Y"); //17 BYTE_OFFSET
			_curr_code_sec->add_op(L"LDW (" + Utils::str_tohex16(offset) + L" + 2, SP), X"); //1F BYTE_OFFSET
		}
		else
		{
			// string
			if(_clear_locals.find(tv.value) == _clear_locals.end())
			{
				// release previous string value
				_curr_code_sec->add_op(L"PUSHW X"); //89
				_stack_ptr += 2;
				offset += 2;
				if(offset > 255)
				{
					return C1STM8_T_ERROR::C1STM8_RES_ESTCKOVF;
				}

				_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)"); //1E BYTE_OFFSET
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_STR_RLS");
				_curr_code_sec->add_op(L"POPW X"); //85
				_stack_ptr -= 2;
				offset -= 2;
			}
			else
			{
				_clear_locals.erase(tv.value);
			}

			_curr_code_sec->add_op(L"LDW (" + Utils::str_tohex16(offset) + L", SP), X"); //1F BYTE_OFFSET
		}
	}
	else
	{
		// simple variable
		auto ma = _mem_areas.find(tv.value);
		bool is_ma = ma != _mem_areas.end();
		std::wstring dst;

		if(is_ma)
		{
			dst = ma->second.use_symbol ? ma->second.symbol : std::to_wstring(ma->second.address);
		}
		else
		{
			dst = tv.value;
			_req_symbols.insert(dst);
		}

		if(tv.type == B1Types::B1T_BYTE)
		{
			_curr_code_sec->add_op(L"LD (" + dst + L"), A"); //B7 SHORT_ADDRESS C7 LONG_ADDRESS
		}
		else
		if(tv.type == B1Types::B1T_INT || tv.type == B1Types::B1T_WORD)
		{
			_curr_code_sec->add_op(L"LDW (" + dst + L"), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
		}
		else
		if(tv.type == B1Types::B1T_LONG)
		{
			_curr_code_sec->add_op(L"LDW (" + dst + L"), Y"); //90 BF SHORT_ADDRESS 90 CF LONG_ADDRESS
			_curr_code_sec->add_op(L"LDW (" + dst + L" + 2), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
		}
		else
		{
			// STRING variable
			// release previous string value
			_curr_code_sec->add_op(L"PUSHW X"); //89
			_stack_ptr += 2;
			_curr_code_sec->add_op(L"LDW X, (" + dst + L")"); //BE SHORT_ADDRESS, CE LONG_ADDRESS
			_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
			_req_symbols.insert(L"__LIB_STR_RLS");
			_curr_code_sec->add_op(L"POPW X"); //85
			_stack_ptr -= 2;

			_curr_code_sec->add_op(L"LDW (" + dst + L"), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::stm8_store(const B1_CMP_ARG &arg)
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
			return static_cast<C1STM8_T_ERROR>(B1_RES_EWRARGCNT);
		}

		dst = ma->second.use_symbol ? ma->second.symbol : std::to_wstring(ma->second.address);
	}
	else
	{
		if(_vars.find(arg[0].value)->second.dim_num != arg.size() - 1)
		{
			return static_cast<C1STM8_T_ERROR>(B1_RES_EWRARGCNT);
		}

		dst = arg[0].value;
		_req_symbols.insert(dst);
	}

	if(arg[0].type == B1Types::B1T_BYTE)
	{
		_curr_code_sec->add_op(L"PUSH A"); //88
		_stack_ptr++;
	}
	else
	{
		_curr_code_sec->add_op(L"PUSHW X"); //89
		_stack_ptr += 2;


		if(arg[0].type == B1Types::B1T_LONG)
		{
			_curr_code_sec->add_op(L"PUSHW Y"); //90 89
			_stack_ptr += 2;
		}
	}

	if(!is_ma)
	{
		const auto &var = _vars.find(arg[0].value)->second;

		// allocate array of default size if necessary
		auto err = stm8_arr_alloc_def(arg, var);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			return err;
		}
	}

	// calculate memory offset
	bool imm_offset = false;
	int32_t offset = 0;
	auto err = stm8_arr_offset(arg, imm_offset, offset);
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	// store value
	if(arg[0].type == B1Types::B1T_BYTE)
	{
		_curr_code_sec->add_op(L"POP A"); //84
		_stack_ptr--;

		if(is_ma)
		{
			if(imm_offset)
			{
				_curr_code_sec->add_op(L"LD (" + dst + L" + " + Utils::str_tohex16(offset) + L"), A"); //B7 SHORT_ADDRESS C7 LONG_ADDRESS
			}
			else
			{
				_curr_code_sec->add_op(L"LD (" + dst + L", X), A"); //E7 BYTE_OFFSET D7 WORD_OFFSET
			}
		}
		else
		{
			if(imm_offset)
			{
				_curr_code_sec->add_op(L"LDW X, (" + dst + L")"); //BE BYTE_OFFSET CE WORD_OFFSET
				_curr_code_sec->add_op(L"LD (" + Utils::str_tohex16(offset) + L", X), A"); //E7 BYTE_OFFSET D7 WORD_OFFSET
			}
			else
			{
				_curr_code_sec->add_op(L"LD ([" + dst + L"], X), A"); //92 D7 BYTE_OFFSET 72 D7 WORD_OFFSET
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
			_curr_code_sec->add_op(L"SLAW X"); //58
		}

		if(is_ma)
		{
			if(imm_offset)
			{
				// release previous string
				if(arg[0].type == B1Types::B1T_STRING)
				{
					_curr_code_sec->add_op(L"LDW X, (" + dst + L" + " + Utils::str_tohex16(offset) + L")"); //BE BYTE_OFFSET CE WORD_OFFSET
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_RLS");
				}

				_curr_code_sec->add_op(L"POPW X"); //85
				_stack_ptr -= 2;

				_curr_code_sec->add_op(L"LDW (" + dst + L" + " + Utils::str_tohex16(offset) + L"), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
			}
			else
			{
				// release previous string
				if(arg[0].type == B1Types::B1T_STRING)
				{
					_curr_code_sec->add_op(L"PUSHW X"); //89
					_stack_ptr += 2;
					_curr_code_sec->add_op(L"LDW X, (" + dst + L", X)"); //EE BYTE_OFFSET DE WORD_OFFSET
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_RLS");
					_curr_code_sec->add_op(L"POPW X"); //85
					_stack_ptr -= 2;
				}

				_curr_code_sec->add_op(L"POPW Y"); //90 85
				_stack_ptr -= 2;

				_curr_code_sec->add_op(L"LDW (" + dst + L", X), Y"); //EF BYTE_OFFSET DF WORD_OFFSET
			}
		}
		else
		{
			if(imm_offset)
			{
				// release previous string
				if(arg[0].type == B1Types::B1T_STRING)
				{
					_curr_code_sec->add_op(L"LDW X, (" + dst + L")"); //BE BYTE_OFFSET CE WORD_OFFSET
					_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", X)"); //EE BYTE_OFFSET DE WORD_OFFSET
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_RLS");
				}

				_curr_code_sec->add_op(L"POPW Y"); //90 85
				_stack_ptr -= 2;

				_curr_code_sec->add_op(L"LDW X, (" + dst + L")"); //BE BYTE_OFFSET CE WORD_OFFSET
				_curr_code_sec->add_op(L"LDW (" + Utils::str_tohex16(offset) + L", X), Y"); //EF BYTE_OFFSET DF WORD_OFFSET
			}
			else
			{
				// release previous string
				if(arg[0].type == B1Types::B1T_STRING)
				{
					_curr_code_sec->add_op(L"PUSHW X"); //89
					_stack_ptr += 2;
					_curr_code_sec->add_op(L"LDW X, ([" + dst + L"], X)"); //92 DE BYTE_OFFSET 72 DE WORD_OFFSET
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_RLS");
					_curr_code_sec->add_op(L"POPW X"); //85
					_stack_ptr -= 2;
				}

				_curr_code_sec->add_op(L"POPW Y"); //90 85
				_stack_ptr -= 2;

				_curr_code_sec->add_op(L"LDW ([" + dst + L"], X), Y"); //92 DF BYTE_OFFSET 72 DF WORD_OFFSET
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
			_curr_code_sec->add_op(L"SLAW X"); //58
			_curr_code_sec->add_op(L"SLAW X"); //58
		}

		if(is_ma)
		{
			if(imm_offset)
			{
				_curr_code_sec->add_op(L"POPW X"); //85
				_stack_ptr -= 2;
				_curr_code_sec->add_op(L"LDW (" + dst + L" + " + Utils::str_tohex16(offset) + L"), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
				_curr_code_sec->add_op(L"POPW X"); //85
				_stack_ptr -= 2;
				_curr_code_sec->add_op(L"LDW (" + dst + L" + " + Utils::str_tohex16(offset) + L" + 2), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
			}
			else
			{
				_curr_code_sec->add_op(L"POPW Y"); //90 85
				_stack_ptr -= 2;
				_curr_code_sec->add_op(L"LDW (" + dst + L", X), Y"); //EF BYTE_OFFSET DF WORD_OFFSET
				_curr_code_sec->add_op(L"POPW Y"); //90 85
				_stack_ptr -= 2;
				_curr_code_sec->add_op(L"LDW (" + dst + L" + 2, X), Y"); //EF BYTE_OFFSET DF WORD_OFFSET
			}
		}
		else
		{
			if(imm_offset)
			{
				_curr_code_sec->add_op(L"LDW X, (" + dst + L")"); //BE BYTE_OFFSET CE WORD_OFFSET
				_curr_code_sec->add_op(L"POPW Y"); //90 85
				_stack_ptr -= 2;
				_curr_code_sec->add_op(L"LDW (" + Utils::str_tohex16(offset) + L", X), Y"); //EF BYTE_OFFSET DF WORD_OFFSET
				_curr_code_sec->add_op(L"POPW Y"); //90 85
				_stack_ptr -= 2;
				_curr_code_sec->add_op(L"LDW (" + Utils::str_tohex16(offset) + L" + 2, X), Y"); //EF BYTE_OFFSET DF WORD_OFFSET
			}
			else
			{
				_curr_code_sec->add_op(L"ADDW X, (" + dst + L")"); //72 BB WORD_OFFSET
				_curr_code_sec->add_op(L"POPW Y"); //90 85
				_stack_ptr -= 2;
				_curr_code_sec->add_op(L"LDW (X), Y"); // FF
				_curr_code_sec->add_op(L"POPW Y"); //90 85
				_stack_ptr -= 2;
				_curr_code_sec->add_op(L"LDW(2, X), Y"); // EF BYTE_OFFSET
			}
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::stm8_un_op(const B1_CMP_CMD &cmd)
{
	auto err = stm8_load(cmd.args[0], cmd.args[1][0].type, LVT::LVT_REG);
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	if(cmd.cmd == L"-")
	{
		if(cmd.args[1][0].type == B1Types::B1T_BYTE)
		{
			_curr_code_sec->add_op(L"NEG A"); //40
		}
		else
		if(cmd.args[1][0].type == B1Types::B1T_INT || cmd.args[1][0].type == B1Types::B1T_WORD)
		{
			_curr_code_sec->add_op(L"NEGW X"); //50
		}
		else
		if(cmd.args[1][0].type == B1Types::B1T_LONG)
		{
			_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_AUX_NEG32"); //AD SIGNED_BYTE_OFFSET (CALLR)
			_req_symbols.insert(L"__LIB_AUX_NEG32");
		}
		else
		{
			return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
		}
	}
	else
	// bitwise NOT
	if(cmd.cmd == L"!")
	{
		if(cmd.args[1][0].type == B1Types::B1T_BYTE)
		{
			_curr_code_sec->add_op(L"CPL A"); //43
		}
		else
		if(cmd.args[1][0].type == B1Types::B1T_INT || cmd.args[1][0].type == B1Types::B1T_WORD)
		{
			_curr_code_sec->add_op(L"CPLW X"); //53
		}
		else
		if(cmd.args[1][0].type == B1Types::B1T_LONG)
		{
			_curr_code_sec->add_op(L"CPLW Y"); //90 53
			_curr_code_sec->add_op(L"CPLW X"); //53
		}
		else
		{
			return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
		}
	}
	else
	if(cmd.cmd != L"=")
	{
		return C1STM8_T_ERROR::C1STM8_RES_EUNKINST;
	}

	err = stm8_store(cmd.args[1]);
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

// additive operations
C1STM8_T_ERROR C1STM8Compiler::stm8_add_op(const B1_CMP_CMD &cmd)
{
	B1Types com_type = B1Types::B1T_UNKNOWN;
	std::wstring inst, val;
	LVT lvt;
	bool comp = false;
	bool imm_val = false;
	bool mem_ref = false;
	bool stk_ref = false;

	B1_CMP_ARG arg1 = cmd.args[0];
	B1_CMP_ARG arg2 = cmd.args[1];

	auto err = B1CUtils::get_com_type(arg1[0].type, arg2[0].type, com_type, comp);
	if(err != B1_RES_OK)
	{
		return static_cast<C1STM8_T_ERROR>(err);
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
			return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
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
		return C1STM8_T_ERROR::C1STM8_RES_EUNKINST;
	}

	if(com_type == B1Types::B1T_INT || com_type == B1Types::B1T_WORD)
	{
		inst += L"W";
	}

	lvt = comp ? (LVT::LVT_REG | LVT::LVT_IMMVAL | LVT::LVT_MEMREF | LVT::LVT_STKREF) : (LVT::LVT_REG | LVT::LVT_IMMVAL);
	auto err1 = stm8_load(arg2, com_type, lvt, &lvt, &val);
	if(err1 != C1STM8_T_ERROR::C1STM8_RES_OK)
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
			_curr_code_sec->add_op(L"PUSH A"); //88
			_stack_ptr++;
		}
		else
		{
			_curr_code_sec->add_op(L"PUSHW X"); //89
			_stack_ptr += 2;

			if(com_type == B1Types::B1T_LONG)
			{
				_curr_code_sec->add_op(L"PUSHW Y"); // 90 89
				_stack_ptr += 2;
			}
		}
	}

	err1 = stm8_load(arg1, com_type, LVT::LVT_REG);
	if(err1 != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err1;
	}

	if(com_type == B1Types::B1T_STRING)
	{
		_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_APD"); //AD SIGNED_BYTE_OFFSET (CALLR)
		_req_symbols.insert(L"__LIB_STR_APD");
		_curr_code_sec->add_op(L"ADDW SP, 2"); //5B BYTE_VALUE
		_stack_ptr -= 2;
	}
	else
	if(com_type == B1Types::B1T_BYTE)
	{
		if(imm_val)
		{
			_curr_code_sec->add_op(inst + L" A, " + val); //AB/A0 BYTE_VALUE
		}
		else
		if(mem_ref)
		{
			_curr_code_sec->add_op(inst + L" A, (" + val + L")"); //BB/B0 BYTE_OFFSET CB/C0 WORD_OFFSET
		}
		else
		if(stk_ref)
		{
			_curr_code_sec->add_op(inst + L" A, (" + val + L", SP)"); //1B/10 BYTE_OFFSET
		}
		else
		{
			_curr_code_sec->add_op(inst + L" A, (0x1, SP)"); //1B/10 BYTE_OFFSET
			_curr_code_sec->add_op(L"ADDW SP, 1"); //5B BYTE_VALUE
			_stack_ptr--;
		}
	}
	else
	if(com_type == B1Types::B1T_INT || com_type == B1Types::B1T_WORD)
	{
		if(imm_val)
		{
			_curr_code_sec->add_op(inst + L" X, " + val); //1C/1D WORD_VALUE
		}
		else
		if(mem_ref)
		{
			_curr_code_sec->add_op(inst + L" X, (" + val + L")"); //72 BB/72 B0 WORD_OFFSET
		}
		else
		if(stk_ref)
		{
			_curr_code_sec->add_op(inst + L" X, (" + val + L", SP)"); //72 FB/72 F0 BYTE_OFFSET
		}
		else
		{
			_curr_code_sec->add_op(inst + L" X, (0x1, SP)"); //72 FB/72 F0 BYTE_OFFSET
			_curr_code_sec->add_op(L"ADDW SP, 2"); //5B BYTE_VALUE
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
				_curr_code_sec->add_op(L"ADDW X, " + val + L".l"); //1C WORD_VALUE
				const auto label = emit_label(true);
				_curr_code_sec->add_op(L"JRNC " + label); //24 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				_curr_code_sec->add_op(L"INCW Y"); //90 5C
				_curr_code_sec->add_lbl(label);
				_all_symbols.insert(label);
				_curr_code_sec->add_op(L"ADDW Y, " + val + L".h"); //72 A9 WORD_VALUE
			}
			else
			{
				_curr_code_sec->add_op(L"SUBW X, " + val + L".l"); //1D WORD_VALUE
				_curr_code_sec->add_op(L"RRWA Y"); //90 01
				_curr_code_sec->add_op(L"SBC A, " + val + L".hl"); //A2 BYTE_VALUE
				_curr_code_sec->add_op(L"RRWA Y"); //90 01
				_curr_code_sec->add_op(L"SBC A, " + val + L".hh"); //A2 BYTE_VALUE
				_curr_code_sec->add_op(L"RRWA Y"); //90 01
			}
		}
		else
		if(mem_ref)
		{
			if(cmd.cmd == L"+")
			{
				_curr_code_sec->add_op(L"ADDW X, (" + val + L" + 2)"); //72 BB WORD_VALUE
				const auto label = emit_label(true);
				_curr_code_sec->add_op(L"JRNC " + label); //24 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				_curr_code_sec->add_op(L"INCW Y"); //90 5C
				_curr_code_sec->add_lbl(label);
				_all_symbols.insert(label);
				_curr_code_sec->add_op(L"ADDW Y, (" + val + L")"); //72 B9 WORD_VALUE
			}
			else
			{
				_curr_code_sec->add_op(L"SUBW X, (" + val + L" + 2)"); //72 B0 WORD_VALUE
				_curr_code_sec->add_op(L"RRWA Y"); //90 01
				_curr_code_sec->add_op(L"SBC A, (" + val + L" + 1)"); //B2 BYTE_OFFSET C2 WORD_OFFSET
				_curr_code_sec->add_op(L"RRWA Y"); //90 01
				_curr_code_sec->add_op(L"SBC A, (" + val + L")"); //B2 BYTE_OFFSET C2 WORD_OFFSET
				_curr_code_sec->add_op(L"RRWA Y"); //90 01
			}
		}
		else
		if(stk_ref)
		{
			if(cmd.cmd == L"+")
			{
				_curr_code_sec->add_op(L"ADDW X, (" + val + L" + 2, SP)"); //72 FB BYTE_OFFSET
				const auto label = emit_label(true);
				_curr_code_sec->add_op(L"JRNC " + label); //24 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				_curr_code_sec->add_op(L"INCW Y"); //90 5C
				_curr_code_sec->add_lbl(label);
				_all_symbols.insert(label);
				_curr_code_sec->add_op(L"ADDW Y, (" + val + L", SP)"); //72 F9 WORD_VALUE
			}
			else
			{
				_curr_code_sec->add_op(L"SUBW X, (" + val + L" + 2, SP)"); //72 F0 WORD_VALUE
				_curr_code_sec->add_op(L"RRWA Y"); //90 01
				_curr_code_sec->add_op(L"SBC A, (" + val + L" + 1, SP)"); //12 BYTE_OFFSET
				_curr_code_sec->add_op(L"RRWA Y"); //90 01
				_curr_code_sec->add_op(L"SBC A, (" + val + L", SP)"); //12 BYTE_OFFSET
				_curr_code_sec->add_op(L"RRWA Y"); //90 01
			}
		}
		else
		{
			if(cmd.cmd == L"+")
			{
				_curr_code_sec->add_op(L"ADDW X, (0x3, SP)"); //72 FB BYTE_OFFSET
				const auto label = emit_label(true);
				_curr_code_sec->add_op(L"JRNC " + label); //24 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				_curr_code_sec->add_op(L"INCW Y"); //90 5C
				_curr_code_sec->add_lbl(label);
				_all_symbols.insert(label);
				_curr_code_sec->add_op(L"ADDW Y, (0x1, SP)"); //72 F9 WORD_VALUE
			}
			else
			{
				_curr_code_sec->add_op(L"SUBW X, (0x3, SP)"); //72 F0 WORD_VALUE
				_curr_code_sec->add_op(L"RRWA Y"); //90 01
				_curr_code_sec->add_op(L"SBC A, (0x2, SP)"); //12 BYTE_OFFSET
				_curr_code_sec->add_op(L"RRWA Y"); //90 01
				_curr_code_sec->add_op(L"SBC A, (0x1, SP)"); //12 BYTE_OFFSET
				_curr_code_sec->add_op(L"RRWA Y"); //90 01
			}

			_curr_code_sec->add_op(L"ADDW SP, 4"); //5B BYTE_VALUE
			_stack_ptr -= 4;
		}
	}

	err1 = stm8_arrange_types(com_type, cmd.args[2][0].type);
	if(err1 != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err1;
	}

	err1 = stm8_store(cmd.args[2]);
	if(err1 != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err1;
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

// multiplicative operations (*, /, ^, MOD)
C1STM8_T_ERROR C1STM8Compiler::stm8_mul_op(const B1_CMP_CMD &cmd)
{
	B1Types com_type;
	bool comp = false;

	B1_CMP_ARG arg1 = cmd.args[0];
	B1_CMP_ARG arg2 = cmd.args[1];

	if(arg1[0].type == B1Types::B1T_STRING || arg2[0].type == B1Types::B1T_STRING)
	{
		return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
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
			return static_cast<C1STM8_T_ERROR>(err);
		}
	}

	if(com_type == B1Types::B1T_BYTE)
	{
		// two BYTE arguments and *, /, or MOD operator
		auto err = stm8_load(arg1, com_type, LVT::LVT_REG);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			return err;
		}

		if(cmd.cmd == L"/" || cmd.cmd == L"%")
		{
			_curr_code_sec->add_op(L"CLRW X"); //5F
		}

		_curr_code_sec->add_op(L"LD XL, A"); //97

		err = stm8_load(arg2, com_type, LVT::LVT_REG);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			return err;
		}

		if(cmd.cmd == L"*")
		{
			_curr_code_sec->add_op(L"MUL X, A"); //42
		}
		else
		{
			// div or mod
			_curr_code_sec->add_op(L"DIV X, A"); //62
		}

		if(cmd.cmd == L"*" || cmd.cmd == L"/")
		{
			_curr_code_sec->add_op(L"LD A, XL"); //9F
		}
	}
	else
	if(com_type == B1Types::B1T_WORD && (cmd.cmd == L"/" || cmd.cmd == L"%"))
	{
		// two WORD arguments or BYTE and WORD
		auto err = stm8_load(arg2, com_type, LVT::LVT_REG);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			return err;
		}

		_curr_code_sec->add_op(L"PUSHW X"); //89
		_stack_ptr += 2;

		err = stm8_load(arg1, com_type, LVT::LVT_REG);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			return err;
		}

		_curr_code_sec->add_op(L"POPW Y"); //90 85
		_stack_ptr -= 2;

		_curr_code_sec->add_op(L"DIVW X, Y"); //62

		if(cmd.cmd == L"%")
		{
			_curr_code_sec->add_op(L"LDW X, Y"); //93
		}
	}
	else
	{
		// here com_type cannot be BYTE
		// power operator: exponent is always INT, not depending on base type
		auto err = stm8_load(arg2, (com_type == B1Types::B1T_LONG && cmd.cmd == L"^") ? B1Types::B1T_INT : com_type, LVT::LVT_REG);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			return err;
		}
		_curr_code_sec->add_op(L"PUSHW X"); //89
		_stack_ptr += 2;
		if(com_type == B1Types::B1T_LONG && cmd.cmd != L"^")
		{
			_curr_code_sec->add_op(L"PUSHW Y"); // 90 89
			_stack_ptr += 2;
		}

		err = stm8_load(arg1, com_type, LVT::LVT_REG);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			return err;
		}

		auto fn_name =	(cmd.cmd == L"*") ? L"__LIB_COM_MUL" :
						(cmd.cmd == L"/") ? L"__LIB_COM_DIV" :
						(cmd.cmd == L"%") ? L"__LIB_COM_REM" :
						(cmd.cmd == L"^") ? L"__LIB_COM_POW" : std::wstring();

		if(fn_name.empty())
		{
			return C1STM8_T_ERROR::C1STM8_RES_EUNKINST;
		}

		fn_name += (com_type == B1Types::B1T_LONG) ? L"32" : L"16";

		_curr_code_sec->add_op(_call_stmt + L" " + fn_name); //AD SIGNED_BYTE_OFFSET (CALLR)
		_req_symbols.insert(fn_name);

		if(com_type == B1Types::B1T_LONG && cmd.cmd != L"^")
		{
			_curr_code_sec->add_op(L"ADDW SP, 4"); //5B BYTE_VALUE
			_stack_ptr -= 4;
		}
		else
		{
			_curr_code_sec->add_op(L"ADDW SP, 2"); //5B BYTE_VALUE
			_stack_ptr -= 2;
		}
	}

	auto err = stm8_arrange_types(com_type, cmd.args[2][0].type);
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	err = stm8_store(cmd.args[2]);
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

// bitwise AND, OR and XOR operations
C1STM8_T_ERROR C1STM8Compiler::stm8_bit_op(const B1_CMP_CMD &cmd)
{
	B1Types com_type = B1Types::B1T_UNKNOWN;
	std::wstring inst, val;
	LVT lvt;
	bool comp = false;
	bool imm_val = false;
	bool mem_ref = false;
	bool stk = false;

	B1_CMP_ARG arg1 = cmd.args[0];
	B1_CMP_ARG arg2 = cmd.args[1];

	if(arg1[0].type == B1Types::B1T_STRING || arg2[0].type == B1Types::B1T_STRING)
	{
		return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
	}

	auto err = B1CUtils::get_com_type(arg1[0].type, arg2[0].type, com_type, comp);
	if(err != B1_RES_OK)
	{
		return static_cast<C1STM8_T_ERROR>(err);
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
		return C1STM8_T_ERROR::C1STM8_RES_EUNKINST;
	}

	lvt = comp ? (LVT::LVT_REG | LVT::LVT_IMMVAL | LVT::LVT_MEMREF | LVT::LVT_STKREF) : (LVT::LVT_REG | LVT::LVT_IMMVAL);
	auto err1 = stm8_load(arg2, com_type, lvt, &lvt, &val);
	if(err1 != C1STM8_T_ERROR::C1STM8_RES_OK)
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
			_curr_code_sec->add_op(L"PUSH A"); //88
			_stack_ptr++;
		}
		else
		{
			_curr_code_sec->add_op(L"PUSHW X"); //89
			_stack_ptr += 2;
			if(com_type == B1Types::B1T_LONG)
			{
				_curr_code_sec->add_op(L"PUSHW Y"); //90 89
				_stack_ptr += 2;
			}
		}
	}

	err1 = stm8_load(arg1, com_type, LVT::LVT_REG);
	if(err1 != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err1;
	}

	if(com_type == B1Types::B1T_BYTE)
	{
		if(imm_val)
		{
			_curr_code_sec->add_op(inst + L" A, " + val); //A4/AA/A8 BYTE_VALUE
		}
		else
		if(mem_ref)
		{
			_curr_code_sec->add_op(inst + L" A, (" + val + L")"); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
		}
		else
		if(stk)
		{
			_curr_code_sec->add_op( inst + L" A, (" + val + L", SP)"); //14/1A/18 BYTE_OFFSET
		}
		else
		{
			_curr_code_sec->add_op(inst + L" A, (1, SP)"); //1B/10/14/1A/18 BYTE_OFFSET
			_curr_code_sec->add_op(L"ADDW SP, 1"); //5B BYTE_VALUE
			_stack_ptr--;
		}
	}
	else
	{
		if(imm_val)
		{
			_curr_code_sec->add_op(L"RLWA X"); //02
			_curr_code_sec->add_op(inst + L" A, " + val + L".lh"); //A4/AA/A8 BYTE_VALUE
			_curr_code_sec->add_op(L"RLWA X"); //02
			_curr_code_sec->add_op(inst + L" A, " + val + L".ll"); //A4/AA/A8 BYTE_VALUE
			_curr_code_sec->add_op(L"RLWA X"); //02
			if(com_type == B1Types::B1T_LONG)
			{
				_curr_code_sec->add_op(L"RLWA Y"); //90 02
				_curr_code_sec->add_op(inst + L" A, " + val + L".hh"); //A4/AA/A8 BYTE_VALUE
				_curr_code_sec->add_op(L"RLWA Y"); //90 02
				_curr_code_sec->add_op(inst + L" A, " + val + L".hl"); //A4/AA/A8 BYTE_VALUE
				_curr_code_sec->add_op(L"RLWA Y"); //90 02
			}
		}
		else
		if(mem_ref)
		{
			if(com_type == B1Types::B1T_INT || com_type == B1Types::B1T_WORD)
			{
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(inst + L" A, (" + val + L")"); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(inst + L" A, (" + val + L" + 1)"); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
				_curr_code_sec->add_op(L"RLWA X"); //02
			}
			else
			{
				// LONG type
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(inst + L" A, (" + val + L" + 2)"); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(inst + L" A, (" + val + L" + 3)"); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(L"RLWA Y"); //90 02
				_curr_code_sec->add_op(inst + L" A, (" + val + L")"); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
				_curr_code_sec->add_op(L"RLWA Y"); //90 02
				_curr_code_sec->add_op(inst + L" A, (" + val + L" + 1)"); //B4/BA/B8 BYTE_OFFSET C4/CA/C8 WORD_OFFSET
				_curr_code_sec->add_op(L"RLWA Y"); //90 02
			}
		}
		else
		if(stk)
		{
			if(com_type == B1Types::B1T_INT || com_type == B1Types::B1T_WORD)
			{
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(inst + L" A, (" + val + L", SP)"); //14/1A/18 BYTE_OFFSET
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(inst + L" A, (" + val + L" + 1, SP)"); //14/1A/18 BYTE_OFFSET
				_curr_code_sec->add_op(L"RLWA X"); //02
			}
			else
			{
				// LONG type
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(inst + L" A, (" + val + L" + 2, SP)"); //14/1A/18 BYTE_OFFSET
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(inst + L" A, (" + val + L" + 3, SP)"); //14/1A/18 BYTE_OFFSET
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(L"RLWA Y"); //90 02
				_curr_code_sec->add_op(inst + L" A, (" + val + L", SP)"); //14/1A/18 BYTE_OFFSET
				_curr_code_sec->add_op(L"RLWA Y"); //90 02
				_curr_code_sec->add_op(inst + L" A, (" + val + L" + 1, SP)"); //14/1A/18 BYTE_OFFSET
				_curr_code_sec->add_op(L"RLWA Y"); //90 02
			}
		}
		else
		{
			if(com_type == B1Types::B1T_INT || com_type == B1Types::B1T_WORD)
			{
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(inst + L" A, (1, SP)"); //14/1A/18 BYTE_OFFSET
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(inst + L" A, (2, SP)"); //14/1A/18 BYTE_OFFSET
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(L"ADDW SP, 2"); //5B BYTE_VALUE
				_stack_ptr -= 2;
			}
			else
			{
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(inst + L" A, (3, SP)"); //14/1A/18 BYTE_OFFSET
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(inst + L" A, (4, SP)"); //14/1A/18 BYTE_OFFSET
				_curr_code_sec->add_op(L"RLWA X"); //02
				_curr_code_sec->add_op(L"RLWA Y"); //90 02
				_curr_code_sec->add_op(inst + L" A, (1, SP)"); //14/1A/18 BYTE_OFFSET
				_curr_code_sec->add_op(L"RLWA Y"); //90 02
				_curr_code_sec->add_op(inst + L" A, (2, SP)"); //14/1A/18 BYTE_OFFSET
				_curr_code_sec->add_op(L"RLWA Y"); //90 02
				_curr_code_sec->add_op(L"ADDW SP, 4"); //5B BYTE_VALUE
				_stack_ptr -= 4;
			}
		}
	}

	err1 = stm8_arrange_types(com_type, cmd.args[2][0].type);
	if(err1 != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err1;
	}

	err1 = stm8_store(cmd.args[2]);
	if(err1 != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err1;
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::stm8_add_shift_op(const std::wstring &shift_cmd, const B1Types type)
{
	if(type == B1Types::B1T_BYTE)
	{
		if(shift_cmd == L"<<")
		{
			_curr_code_sec->add_op(L"SLL A"); //48
		}
		else
		{
			_curr_code_sec->add_op(L"SRL A"); //44
		}
	}
	else
	if(type == B1Types::B1T_INT)
	{
		if(shift_cmd == L"<<")
		{
			_curr_code_sec->add_op(L"SLAW X"); //58
		}
		else
		{
			_curr_code_sec->add_op(L"SRAW X"); //57
		}
	}
	else
	if(type == B1Types::B1T_WORD)
	{
		if(shift_cmd == L"<<")
		{
			_curr_code_sec->add_op(L"SLLW X"); //58
		}
		else
		{
			_curr_code_sec->add_op(L"SRLW X"); //54
		}
	}
	else
	{
		// LONG type
		if(shift_cmd == L"<<")
		{
			_curr_code_sec->add_op(L"SLLW X"); //58
			_curr_code_sec->add_op(L"RLCW Y"); //90 59
		}
		else
		{
			_curr_code_sec->add_op(L"SRAW Y"); //90 57
			_curr_code_sec->add_op(L"RRCW X"); //56
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

// shift operations
C1STM8_T_ERROR C1STM8Compiler::stm8_shift_op(const B1_CMP_CMD &cmd)
{
	B1_CMP_ARG arg1 = cmd.args[0];
	B1_CMP_ARG arg2 = cmd.args[1];

	if(arg1[0].type == B1Types::B1T_STRING || arg2[0].type == B1Types::B1T_STRING)
	{
		return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
	}

	auto err = stm8_load(arg1, arg1[0].type, LVT::LVT_REG);
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
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
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}

				if(arg1[0].type == B1Types::B1T_BYTE)
				{
					_curr_code_sec->add_op(L"LDW X, " + res_val + L".ll"); //AE WORD_VALUE
				}
				else
				{
					_curr_code_sec->add_op(L"LD A, " + res_val); //A6 BYTE_VALUE
				}

				const auto loop_label = emit_label(true);
				_curr_code_sec->add_lbl(loop_label);
				_all_symbols.insert(loop_label);

				stm8_add_shift_op(cmd.cmd, arg1[0].type);

				if(arg1[0].type == B1Types::B1T_BYTE)
				{
					_curr_code_sec->add_op(L"DECW X"); //5A
				}
				else
				{
					_curr_code_sec->add_op(L"DEC A"); //4A
				}
				_curr_code_sec->add_op(L"JRNE " + loop_label); //26 SIGNED_BYTE_OFFSET
				_req_symbols.insert(loop_label);
			}
		}
		else
		{
			const auto loop_label = emit_label(true);
			const auto loop_end_label = emit_label(true);

			if(arg1[0].type == B1Types::B1T_BYTE)
			{
				_curr_code_sec->add_op(L"PUSH A"); //88
				_stack_ptr++;

				err = stm8_load(arg2, B1Types::B1T_INT, LVT::LVT_REG);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}

				_curr_code_sec->add_op(L"POP A"); //84
				_stack_ptr--;

				_curr_code_sec->add_op(L"TNZW X"); //5D
				_curr_code_sec->add_op(L"JREQ " + loop_end_label); //27 SIGNED_BYTE_OFFSET
				_req_symbols.insert(loop_end_label);

			}
			else
			{
				_curr_code_sec->add_op(L"PUSHW X"); //89
				_stack_ptr += 2;
				if(arg1[0].type == B1Types::B1T_LONG)
				{
					_curr_code_sec->add_op(L"PUSHW Y"); //90 89
					_stack_ptr += 2;
				}

				err = stm8_load(arg2, B1Types::B1T_BYTE, LVT::LVT_REG);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}

				if(arg1[0].type == B1Types::B1T_LONG)
				{
					_curr_code_sec->add_op(L"POPW Y"); //90 85
					_stack_ptr -= 2;
				}
				_curr_code_sec->add_op(L"POPW X"); //85
				_stack_ptr -= 2;

				_curr_code_sec->add_op(L"TNZ A"); //4D
				_curr_code_sec->add_op(L"JREQ " + loop_end_label); //27 SIGNED_BYTE_OFFSET
				_req_symbols.insert(loop_end_label);
			}

			_curr_code_sec->add_lbl(loop_label);
			_all_symbols.insert(loop_label);

			stm8_add_shift_op(cmd.cmd, arg1[0].type);

			if(arg1[0].type == B1Types::B1T_BYTE)
			{
				_curr_code_sec->add_op(L"DECW X"); //5A
			}
			else
			{
				_curr_code_sec->add_op(L"DEC A"); //4A
			}

			_curr_code_sec->add_op(L"JRNE " + loop_label); //26 SIGNED_BYTE_OFFSET
			_req_symbols.insert(loop_label);
			_curr_code_sec->add_lbl(loop_end_label);
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
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	err = stm8_store(cmd.args[2]);
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

// numeric comparison operations
C1STM8_T_ERROR C1STM8Compiler::stm8_num_cmp_op(const B1_CMP_CMD &cmd)
{
	B1Types com_type = B1Types::B1T_UNKNOWN;
	std::wstring val;
	LVT lvt;
	bool comp = false;
	bool imm_val = false;
	bool mem_ref = false;
	bool stk_ref = false;

	auto err = B1CUtils::get_com_type(cmd.args[0][0].type, cmd.args[1][0].type, com_type, comp);
	if(err != B1_RES_OK)
	{
		return static_cast<C1STM8_T_ERROR>(err);
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
		return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
	}

	lvt = comp ? (LVT::LVT_REG | LVT::LVT_IMMVAL | LVT::LVT_MEMREF | LVT::LVT_STKREF) : (LVT::LVT_REG | LVT::LVT_IMMVAL);
	auto err1 = stm8_load(arg2, com_type, lvt, &lvt, &val);
	if(err1 != C1STM8_T_ERROR::C1STM8_RES_OK)
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
			_curr_code_sec->add_op(L"PUSH A"); //88
			_stack_ptr++;
		}
		else
		{
			_curr_code_sec->add_op(L"PUSHW X"); //89
			_stack_ptr += 2;
			if(com_type == B1Types::B1T_LONG)
			{
				_curr_code_sec->add_op(L"PUSHW Y"); //90 89
				_stack_ptr += 2;
			}
		}
	}

	err1 = stm8_load(arg1, com_type, LVT::LVT_REG);
	if(err1 != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err1;
	}

	if(com_type == B1Types::B1T_BYTE)
	{
		if(imm_val)
		{
			_curr_code_sec->add_op(L"CP A, " + val); //A1 BYTE_VALUE
		}
		else
		if(mem_ref)
		{
			_curr_code_sec->add_op(L"CP A, (" + val + L")"); //B1 BYTE_OFFSET C1 WORD_OFFSET
		}
		else
		if(stk_ref)
		{
			_curr_code_sec->add_op(L"CP A, (" + val + L", SP)"); //11 BYTE_OFFSET
		}
		else
		{
			_curr_code_sec->add_op(L"CP A, (1, SP)"); //11 BYTE_OFFSET
			_curr_code_sec->add_op(L"POP A"); //84
			_stack_ptr--;
		}
	}
	else
	if(com_type == B1Types::B1T_INT || com_type == B1Types::B1T_WORD)
	{
		if(imm_val)
		{
			_curr_code_sec->add_op(L"CPW X, " + val); //A3 WORD_VALUE
		}
		else
		if(mem_ref)
		{
			_curr_code_sec->add_op(L"CPW X, (" + val + L")"); //B3 BYTE_OFFSET C3 WORD_OFFSET
		}
		else
		if(stk_ref)
		{
			_curr_code_sec->add_op(L"CPW X, (" + val + L", SP)"); //13 BYTE_OFFSET
		}
		else
		{
			_curr_code_sec->add_op(L"CPW X, (1, SP)"); //13 BYTE_OFFSET
			_curr_code_sec->add_op(L"POPW X"); //85
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
				_curr_code_sec->add_op(L"CPW X, " + val + L".l"); //A3 WORD_VALUE
				_curr_code_sec->add_op(L"JRNE " + label); //26 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				_curr_code_sec->add_op(L"CPW Y, " + val + L".h"); //90 A3 WORD_VALUE
			}
			else
			if(mem_ref)
			{
				_curr_code_sec->add_op(L"CPW X, (" + val + L" + 2)"); //B3 BYTE_OFFSET C3 WORD_OFFSET
				_curr_code_sec->add_op(L"JRNE " + label); //26 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				_curr_code_sec->add_op(L"CPW Y, (" + val + L")"); //90 B3 BYTE_OFFSET 90 C3 WORD_OFFSET
			}
			else
			if(stk_ref)
			{
				_curr_code_sec->add_op(L"CPW X, (" + val + L" + 2, SP)"); //13 BYTE_OFFSET
				_curr_code_sec->add_op(L"JRNE " + label); //26 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				_curr_code_sec->add_op(L"EXGW X, Y"); //51
				_curr_code_sec->add_op(L"CPW X, (" + val + L", SP)"); //13 BYTE_OFFSET
			}
			else
			{
				_curr_code_sec->add_op(L"CPW X, (" + val + L" + 3, SP)"); //13 BYTE_OFFSET
				_curr_code_sec->add_op(L"JRNE " + label); //26 SIGNED_BYTE_OFFSET
				_req_symbols.insert(label);
				_curr_code_sec->add_op(L"EXGW X, Y"); //51
				_curr_code_sec->add_op(L"CPW X, (" + val + L" + 1, SP)"); //13 BYTE_OFFSET

				clr_stk = true;
			}

			_curr_code_sec->add_lbl(label);
			_all_symbols.insert(label);

			if(clr_stk)
			{
				_curr_code_sec->add_op(L"ADDW SP, 4"); //5B BYTE_VALUE
				_stack_ptr -= 4;
			}
		}
		else
		{
			if(imm_val)
			{
				_curr_code_sec->add_op(L"CPW X, " + val + L".l"); //A3 WORD_VALUE
				_curr_code_sec->add_op(L"LD A, YL"); //90 9F
				_curr_code_sec->add_op(L"SBC A, " + val + L".hl"); //A2 BYTE_VALUE
				_curr_code_sec->add_op(L"LD A, YH"); //90 9E
				_curr_code_sec->add_op(L"SBC A, " + val + L".hh"); //A2 BYTE_VALUE
			}
			else
			if(mem_ref)
			{
				_curr_code_sec->add_op(L"CPW X, (" + val + L" + 2)"); //B3 BYTE_OFFSET C3 WORD_OFFSET
				_curr_code_sec->add_op(L"LD A, YL"); //90 9F
				_curr_code_sec->add_op(L"SBC A, (" + val + L" + 1)"); //B2 BYTE_OFFSET C2 WORD_OFFSET
				_curr_code_sec->add_op(L"LD A, YH"); //90 9E
				_curr_code_sec->add_op(L"SBC A, (" + val + L")"); //B2 BYTE_OFFSET C2 WORD_OFFSET
			}
			else
			if(stk_ref)
			{
				_curr_code_sec->add_op(L"CPW X, (" + val + L" + 2, SP)"); //13 BYTE_OFFSET
				_curr_code_sec->add_op(L"LD A, YL"); //90 9F
				_curr_code_sec->add_op(L"SBC A, (" + val + L" + 1, SP)"); //12 BYTE_OFFSET
				_curr_code_sec->add_op(L"LD A, YH"); //90 9E
				_curr_code_sec->add_op(L"SBC A, (" + val + L", SP)"); //12 BYTE_OFFSET
			}
			else
			{
				_curr_code_sec->add_op(L"CPW X, (" + val + L" + 3, SP)"); //13 BYTE_OFFSET
				_curr_code_sec->add_op(L"LD A, YL"); //90 9F
				_curr_code_sec->add_op(L"SBC A, (" + val + L" + 2, SP)"); //12 BYTE_OFFSET
				_curr_code_sec->add_op(L"LD A, YH"); //90 9E
				_curr_code_sec->add_op(L"SBC A, (" + val + L" + 1, SP)"); //12 BYTE_OFFSET
				_curr_code_sec->add_op(L"ADDW SP, 4"); //5B BYTE_VALUE
				_stack_ptr -= 4;
			}
		}
}

	_cmp_active = true;
	_cmp_type = com_type;

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

// string comparison operations
C1STM8_T_ERROR C1STM8Compiler::stm8_str_cmp_op(const B1_CMP_CMD &cmd)
{
	B1_CMP_ARG arg1 = cmd.args[0];
	B1_CMP_ARG arg2 = cmd.args[1];

	if(arg1[0].type != B1Types::B1T_STRING && arg2[0].type != B1Types::B1T_STRING)
	{
		return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
	}

	auto err = stm8_load(arg2, B1Types::B1T_STRING, LVT::LVT_REG);
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	_curr_code_sec->add_op(L"PUSHW X"); //89
	_stack_ptr += 2;

	err = stm8_load(arg1, B1Types::B1T_STRING, LVT::LVT_REG);
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_CMP"); //AD SIGNED_BYTE_OFFSET (CALLR)
	_req_symbols.insert(L"__LIB_STR_CMP");
	_curr_code_sec->add_op(L"ADDW SP, 2"); //5B BYTE_VALUE
	_stack_ptr -= 2;

	_curr_code_sec->add_op(L"TNZ A"); //4D

	_cmp_active = true;
	_cmp_op = cmd.cmd;
	_cmp_type = B1Types::B1T_STRING;

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

bool C1STM8Compiler::is_udef_or_var_used(const B1_CMP_ARG &arg, bool dst, std::set<std::wstring> &vars_to_free)
{
	bool first = true;

	for(const auto &a: arg)
	{
		if(_ufns.find(a.value) != _ufns.end())
		{
			return true;
		}

		if(!B1CUtils::is_imm_val(a.value))
		{
			if((first && arg.size() > 1) || (dst && arg.size() == 1))
			{
				vars_to_free.insert(a.value);
			}
		}

		first = false;
	}

	return false;
}

bool C1STM8Compiler::is_udef_or_var_used(const B1_CMP_CMD &cmd, std::set<std::wstring> &vars_to_free)
{
	if(B1CUtils::is_label(cmd))
	{
		return false;
	}

	if(B1CUtils::is_inline_asm(cmd))
	{
		return true;
	}

	if(cmd.cmd == L"GA")
	{
		for(auto a = cmd.args.begin() + 2; a != cmd.args.end(); a++)
		{
			if(is_udef_or_var_used(*a, false, vars_to_free))
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"RETVAL")
	{
		return is_udef_or_var_used(cmd.args[0], false, vars_to_free);
	}

	if(cmd.cmd == L"READ")
	{
		return is_udef_or_var_used(cmd.args[1], true, vars_to_free);
	}

	bool bin = false;
	bool log = false;
	bool un = false;
	int dst_arg;

	dst_arg = 2;
	bin = B1CUtils::is_bin_op(cmd);
	if(!bin)
	{
		dst_arg = -1;
		log = B1CUtils::is_log_op(cmd);
	}
	if(!(bin || log))
	{
		dst_arg = 1;
		un = B1CUtils::is_un_op(cmd);
	}

	if(bin || log || un)
	{
		for(int i = 0; i < cmd.args.size(); i++)
		{
			if(is_udef_or_var_used(cmd.args[i], i == dst_arg, vars_to_free))
			{
				return true;
			}
		}

		return false;
	}

	return false;
}

// on return cmd_it is set on the last processed cmd
C1STM8_T_ERROR C1STM8Compiler::write_ioctl(std::list<B1_CMP_CMD>::const_iterator &cmd_it)
{
	std::wstring dev_name;
	std::wstring cmd_name;
	int32_t id = -1;
	auto data_type = B1Types::B1T_UNKNOWN;
	bool pre_cmd = false; // command(-s) with predefined value(-s)
	int32_t mask = 0;
	int32_t values = 0;
	std::wstring label_value;
	bool accepts_data = false;
	auto call_type = Settings::IoCmd::IoCmdCallType::CT_CALL;
	auto code_place = Settings::IoCmd::IoCmdCodePlacement::CP_CURR_POS;
	std::wstring file_name;
	int32_t ioctl_num = 1;

	while(true)
	{
		auto *cmd = &*cmd_it;
		if(cmd->cmd != L"IOCTL")
		{
			if(id < 0)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
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
			return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
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
			_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd_it->src_line_id]));
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
			return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
		}

		if(!iocmd.predef_only)
		{
			id = iocmd.id;
			accepts_data = true;
			data_type = iocmd.data_type;
			call_type = iocmd.call_type;
			code_place = iocmd.code_place;
			file_name = iocmd.file_name;

			if(iocmd.data_type == B1Types::B1T_LABEL)
			{
				label_value = cmd->args[2][0].value;
			}
			else
			{
				auto err = stm8_load(cmd_it->args[2], iocmd.data_type, LVT::LVT_REG);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
			}

			break;
		}

		auto val = iocmd.values.find(cmd->args[2][0].value.substr(1, cmd->args[2][0].value.length() - 2));
		if(val == iocmd.values.end())
		{
			return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
		}

		if(id < 0)
		{
			// the first cmd
			id = iocmd.id;

			// predefined values cannot be strings at the moment
			if(iocmd.data_type == B1Types::B1T_STRING)
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
			}

			auto err = Utils::str2int32(val->second, values);
			if(err != B1_RES_OK)
			{
				return static_cast<C1STM8_T_ERROR>(err);
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
			auto err = Utils::str2int32(val->second, n);
			if(err != B1_RES_OK)
			{
				return static_cast<C1STM8_T_ERROR>(err);
			}

			mask |= iocmd.mask;
			values = (values & ~iocmd.mask) | n;
			ioctl_num++;
		}

		if(std::next(cmd_it) == cend())
		{
			break;
		}

		cmd_it++;
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
				return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
			}

			if(data_type == B1Types::B1T_BYTE)
			{
				_curr_code_sec->add_op(L"LD A, " + std::to_wstring(values)); //A6 BYTE_VALUE
				if(mask != 0)
				{
					_curr_code_sec->add_op(L"PUSH " + std::to_wstring(mask)); //4B BYTE_VALUE
					_stack_ptr++;
				}
			}
			else
			if(data_type == B1Types::B1T_INT)
			{
				_curr_code_sec->add_op(L"LDW X, " + std::to_wstring(values)); //AE WORD_VALUE
			}
			else
			if(data_type == B1Types::B1T_WORD)
			{
				_curr_code_sec->add_op(L"LDW X, " + std::to_wstring(values)); //AE WORD_VALUE
			}
			else
			if(data_type == B1Types::B1T_LONG)
			{
				_curr_code_sec->add_op(L"LDW X, " + std::to_wstring(values & 0xFFFF)); //AE WORD_VALUE
				_curr_code_sec->add_op(L"LDW Y, " + std::to_wstring((values >> 16) & 0xFFFF)); //90 AE WORD_VALUE
			}
		}

		_curr_code_sec->add_op(_call_stmt + L" " + file_name);  //AD SIGNED_BYTE_OFFSET (CALLR)
		_req_symbols.insert(file_name);

		if(pre_cmd && data_type == B1Types::B1T_BYTE && mask != 0)
		{
			_curr_code_sec->add_op(L"POP A"); //84
			_stack_ptr--;
		}

	}
	else
	{
		if(file_name.empty())
		{
			file_name = L"__LIB_" + dev_name + L"_" + std::to_wstring(id) + L"_INL";
		}

		// inline code
		std::map<std::wstring, std::wstring> params =
		{
			{ L"VALUE", (data_type == B1Types::B1T_LABEL) ? label_value : std::to_wstring(values) },
			{ L"MASK", std::to_wstring(mask) },
			{ L"DEV_NAME", dev_name },
			{ L"ID", std::to_wstring(id) },
			{ L"CALL_TYPE", L"INL"},
			{ L"IOCTL_NUM", std::to_wstring(ioctl_num) },
			{ L"CMD_NAME", cmd_name },
			{ L"FILE_NAME", file_name }
		};

		if(code_place == Settings::IoCmd::IoCmdCodePlacement::CP_CURR_POS)
		{
			auto saved_it = cmd_it++;
			auto err = load_inline(0, file_name, cmd_it, params);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}
			cmd_it = saved_it;
		}
		else
		{
			// Settings::IoCmd::IoCmdCodePlacement::CP_END: placement after END statement
			_end_placement.push_back(std::make_pair(cmd_it, params));
		}
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::write_code_sec()
{
	// code
	_stack_ptr = 0;
	_local_offset.clear();

	_curr_udef_args_size = 0;
	_curr_udef_arg_offsets.clear();
	_curr_udef_str_arg_offsets.clear();

	_cmp_active = false;
	_retval_active = false;

	_clear_locals.clear();

	_allocated_arrays.clear();

	bool int_handler = false;

	for(auto ci = cbegin(); ci != cend(); ci++)
	{
		const auto &cmd = *ci;

		_curr_src_file_id = cmd.src_file_id;
		_curr_line_cnt = cmd.line_cnt;

		if(B1CUtils::is_label(cmd))
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			_curr_code_sec->add_lbl(cmd.cmd);
			// labels are processed in load_next_command() function
			//_all_symbols.insert(cmd.cmd);

			if(B1CUtils::is_def_fn(cmd.cmd))
			{
				_curr_udef_arg_offsets.clear();
				_curr_udef_str_arg_offsets.clear();

				const auto ufn = _ufns.find(cmd.cmd);
				int32_t arg_off = 1;
				for(auto a = ufn->second.args.rbegin(); a != ufn->second.args.rend(); a++)
				{
					const auto &arg = *a;
					int32_t size = 0;
					
					if(!B1CUtils::get_asm_type(arg.type, nullptr, &size))
					{
						return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
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
					_curr_code_sec->add_op(L"PUSH A"); //88
					_stack_ptr++;
				}
				else
				if(_curr_udef_args_size == 2)
				{
					_curr_code_sec->add_op(L"PUSHW X"); //89
					_stack_ptr += 2;
				}
				else
				{
					// LONG type
					_curr_code_sec->add_op(L"PUSHW X"); //89
					_curr_code_sec->add_op(L"PUSHW Y"); //90 89
					_stack_ptr += 4;
				}
			}

			_cmp_active = false;
			_retval_active = false;

			_allocated_arrays.clear();

			continue;
		}

		if(B1CUtils::is_inline_asm(cmd))
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			for(const auto &a: cmd.args)
			{
				auto trimmed = Utils::str_trim(a[0].value);
				if(!trimmed.empty())
				{
					if(trimmed.front() == L':')
					{
						_curr_code_sec->add_lbl(trimmed.substr(1));
					}
					else
					if(trimmed.front() == L';')
					{
						_curr_code_sec->add_comment(trimmed.substr(1));
					}
					else
					if(trimmed.length() >= 2)
					{
						auto first2 = trimmed.substr(0, 2);
						if(first2 == L"DB" || first2 == L"DW")
						{
							_curr_code_sec->add_data(trimmed);
						}
						else
						{
							_curr_code_sec->add_op(trimmed);
						}
					}
					else
					{
						return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
					}
				}
			}

			_cmp_active = false;
			_retval_active = false;

			_allocated_arrays.clear();

			continue;
		}

		if(cmd.cmd == L"NS")
		{
			if(cmd.args[0][0].value.empty())
			{
				return static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			}

			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			_curr_name_space = cmd.args[0][0].value;
			_next_label = 32768;
			_next_local = 32768;

			continue;
		}

		if(cmd.cmd == L"GA")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			const auto &var = _vars.find(cmd.args[0][0].value)->second;

			if(cmd.args.size() == 2)
			{
				// simple variable
				auto err = stm8_st_gf(var, false);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
			}
			else
			{
				// allocate array memory
				auto err = stm8_st_ga(cmd, var);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}

				_allocated_arrays.insert(cmd.args[0][0].value);
			}

			_cmp_active = false;
			_retval_active = false;

			continue;
		}

		if(cmd.cmd == L"GF")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			const auto var = _vars.find(cmd.args[0][0].value);
			
			auto err = (var != _vars.end()) ?
				stm8_st_gf(var->second, false) :
				stm8_st_gf(_mem_areas.find(cmd.args[0][0].value)->second, true);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			_cmp_active = false;
			_retval_active = false;

			_allocated_arrays.erase(cmd.args[0][0].value);

			continue;
		}

		if(cmd.cmd == L"CALL")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			_curr_code_sec->add_op(_call_stmt + L" " + cmd.args[0][0].value); //AD SIGNED_BYTE_OFFSET (CALLR)
			_req_symbols.insert(cmd.args[0][0].value);

			_cmp_active = false;
			_retval_active = false;

			_allocated_arrays.clear();

			continue;
		}

		if(cmd.cmd == L"LA")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			// get local size
			int32_t size;
			if(!B1CUtils::get_asm_type(cmd.args[1][0].type, nullptr, &size))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
			}

			if(_cmp_active)
			{
				if(size == 1)
				{
					// B1T_BYTE
					_curr_code_sec->add_op(L"SUB SP, 1"); //52 BYTE_VALUE
				}
				else
				if(size == 2)
				{
					// use PUSH/POP for LA/LF after compare operations (in order not to overwrite flags register)
					// B1T_INT, B1T_WORD or B1T_STRING
					if(cmd.args[1][0].type == B1Types::B1T_STRING)
					{
						// string local variable must be emptied right after creation
						_curr_code_sec->add_op(L"PUSH 0"); //4B BYTE_VALUE
						_curr_code_sec->add_op(L"PUSH 0"); //4B BYTE_VALUE

						_clear_locals.insert(cmd.args[0][0].value);
					}
					else
					{
						_curr_code_sec->add_op(L"SUB SP, 2"); //52 BYTE_VALUE
					}
				}
				else
				{
					// B1T_LONG
					_curr_code_sec->add_op(L"SUB SP, 4"); //52 BYTE_VALUE
				}
			}
			else
			{
				if(cmd.args[1][0].type == B1Types::B1T_STRING)
				{
					// string local variable must be emptied right after creation
					_curr_code_sec->add_op(L"CLRW X"); //5F
					_curr_code_sec->add_op(L"PUSHW X"); //89

					_clear_locals.insert(cmd.args[0][0].value);
				}
				else
				{
					_curr_code_sec->add_op(L"SUB SP, " + Utils::str_tohex16(size)); //52 BYTE_VALUE
				}
			}

			_stack_ptr += size;
			_local_offset.push_back(std::pair<B1_TYPED_VALUE, int>(B1_TYPED_VALUE(cmd.args[0][0].value, cmd.args[1][0].type), _stack_ptr - 1));

			_retval_active = false;

			continue;
		}

		if(cmd.cmd == L"LF")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			const auto &loc = _local_offset.back();

			if(loc.first.value != cmd.args[0][0].value)
			{
				return C1STM8_T_ERROR::C1STM8_RES_ESTKFAIL;
			}

			// get local size
			int size;
			if(!B1CUtils::get_asm_type(loc.first.type, nullptr, &size))
			{
				return C1STM8_T_ERROR::C1STM8_RES_EINVTYPNAME;
			}

			bool not_used = (_clear_locals.find(cmd.args[0][0].value) != _clear_locals.end());

			if(_cmp_active)
			{
				// use PUSH/POP for LA/LF after compare operations (in order not to overwrite flags register)
				if(size == 1)
				{
					_curr_code_sec->add_op(L"ADDW SP, 1"); //5B BYTE_VALUE
				}
				else
				if(size == 2)
				{
					if(loc.first.type == B1Types::B1T_STRING)
					{
						_curr_code_sec->add_op(L"POPW X"); //85
						if(!not_used)
						{
							_curr_code_sec->add_op(L"PUSH CC"); //8A
							_stack_ptr += 1;
							_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
							_req_symbols.insert(L"__LIB_STR_RLS");
							_curr_code_sec->add_op(L"POP CC"); //86
							_stack_ptr -= 1;
						}
					}
					else
					{
						_curr_code_sec->add_op(L"ADDW SP, 2"); //5B BYTE_VALUE
					}
				}
				else
				{
					_curr_code_sec->add_op(L"ADDW SP, 4"); //5B BYTE_VALUE
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
							_curr_code_sec->add_op(L"PUSH A"); //88
							_stack_ptr += 1;
							_curr_code_sec->add_op(L"LDW X, (2, SP)"); //1E BYTE_OFFSET
							_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
							_req_symbols.insert(L"__LIB_STR_RLS");
							_curr_code_sec->add_op(L"POP A"); //84
							_stack_ptr -= 1;
						}
						else
						if(_retval_type == B1Types::B1T_INT || _retval_type == B1Types::B1T_WORD || _retval_type == B1Types::B1T_STRING)
						{
							_curr_code_sec->add_op(L"PUSHW X"); //89
							_stack_ptr += 2;
							_curr_code_sec->add_op(L"LDW X, (3, SP)"); //1E BYTE_OFFSET
							_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
							_req_symbols.insert(L"__LIB_STR_RLS");
							_curr_code_sec->add_op(L"POPW X"); //85
							_stack_ptr -= 2;
						}
						else
						{
							// LONG type
							_curr_code_sec->add_op(L"PUSHW X"); //89
							_curr_code_sec->add_op(L"PUSHW Y"); //90 89
							_stack_ptr += 4;
							_curr_code_sec->add_op(L"LDW X, (5, SP)"); //1E BYTE_OFFSET
							_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
							_req_symbols.insert(L"__LIB_STR_RLS");
							_curr_code_sec->add_op(L"POPW Y"); //90 85
							_curr_code_sec->add_op(L"POPW X"); //85
							_stack_ptr -= 4;
						}
					}

					_curr_code_sec->add_op(L"ADDW SP, " + Utils::str_tohex16(size)); //5B BYTE_VALUE
				}
				else
				{
					_curr_code_sec->add_op(L"ADDW SP, " + Utils::str_tohex16(size)); //5B BYTE_VALUE
				}
			}
			else
			{
				if(loc.first.type == B1Types::B1T_STRING)
				{
					_curr_code_sec->add_op(L"POPW X"); //85

					if(!not_used)
					{
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_RLS");
					}
				}
				else
				{
					_curr_code_sec->add_op(L"ADDW SP, " + Utils::str_tohex16(size)); //5B BYTE_VALUE
				}
			}

			_clear_locals.erase(cmd.args[0][0].value);

			_stack_ptr -= size;
			_local_offset.pop_back();

			continue;
		}

		if(cmd.cmd == L"MA")
		{
			continue;
		}

		if(cmd.cmd == L"DAT")
		{
			continue;
		}

		if(cmd.cmd == L"DEF")
		{
			continue;
		}

		if(cmd.cmd == L"IN")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			auto in_dev = _global_settings.GetIoDeviceName(cmd.args[0][0].value);

			if(in_dev.empty())
			{
				if(cmd.args[0][0].value.empty())
				{
					return C1STM8_T_ERROR::C1STM8_RES_ENODEFIODEV;
				}
				else
				{
					return C1STM8_T_ERROR::C1STM8_RES_EUNKIODEV;
				}
			}

			_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_" + in_dev + L"_IN");  //AD SIGNED_BYTE_OFFSET (CALLR)
			_req_symbols.insert(L"__LIB_" + in_dev + L"_IN");
			if(cmd.args[1][0].type == B1Types::B1T_BYTE)
			{
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_CBYTE"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_STR_CBYTE");
			}
			else
			if(cmd.args[1][0].type == B1Types::B1T_INT)
			{
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_CINT"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_STR_CINT");
			}
			else
			if(cmd.args[1][0].type == B1Types::B1T_WORD)
			{
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_CWRD"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_STR_CWRD");
			}
			else
			if(cmd.args[1][0].type == B1Types::B1T_LONG)
			{
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_CLNG"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_STR_CLNG");
			}

			// store value
			auto err = stm8_store(cmd.args[1]);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			_cmp_active = false;
			_retval_active = false;

			continue;
		}

		if(cmd.cmd == L"IOCTL")
		{
			auto err = write_ioctl(ci);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			_cmp_active = false;
			_retval_active = false;

			continue;
		}

		if(cmd.cmd == L"OUT")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			auto out_dev = _global_settings.GetIoDeviceName(cmd.args[0][0].value);

			if(out_dev.empty())
			{
				if(cmd.args[0][0].value.empty())
				{
					return C1STM8_T_ERROR::C1STM8_RES_ENODEFIODEV;
				}
				else
				{
					return C1STM8_T_ERROR::C1STM8_RES_EUNKIODEV;
				}
			}

			if(cmd.args[1][0].value == L"NL")
			{
				// print new line
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_" + out_dev + L"_NL"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_" + out_dev + L"_NL");
			}
			else
			if(cmd.args[1][0].value == L"TAB")
			{
				// PRINT TAB(n) function
				// TAB(0) is a special case: move to the next print zone
				// load argument of TAB or SPC function
				auto err = stm8_load(cmd.args[1][1], B1Types::B1T_BYTE, LVT::LVT_REG);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_" + out_dev + L"_TAB"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_" + out_dev + L"_TAB");
			}
			else
			if(cmd.args[1][0].value == L"SPC")
			{
				// PRINT SPC(n) function
				// load argument of TAB or SPC function
				auto err = stm8_load(cmd.args[1][1], B1Types::B1T_BYTE, LVT::LVT_REG);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_" + out_dev + L"_SPC"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_" + out_dev + L"_SPC");
			}
			else
			{
				if(cmd.args[1][0].type == B1Types::B1T_STRING)
				{
					auto err = stm8_load(cmd.args[1], B1Types::B1T_STRING, LVT::LVT_REG);
					if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
					{
						return err;
					}
				}
				else
				if(cmd.args[1][0].type == B1Types::B1T_WORD || cmd.args[1][0].type == B1Types::B1T_BYTE)
				{
					auto err = stm8_load(cmd.args[1], B1Types::B1T_WORD, LVT::LVT_REG);
					if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
					{
						return err;
					}

					_curr_code_sec->add_op(L"PUSH 2"); //4B BYTE_VALUE
					_stack_ptr += 1;
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR16"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_STR16");
					_curr_code_sec->add_op(L"POP A"); //84
					_stack_ptr -= 1;
				}
				else
				if(cmd.args[1][0].type == B1Types::B1T_INT)
				{
					auto err = stm8_load(cmd.args[1], B1Types::B1T_INT, LVT::LVT_REG);
					if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
					{
						return err;
					}

					_curr_code_sec->add_op(L"PUSH 3"); //4B BYTE_VALUE
					_stack_ptr += 1;
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR16"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_STR16");
					_curr_code_sec->add_op(L"POP A"); //84
					_stack_ptr -= 1;
				}
				else
				if(cmd.args[1][0].type == B1Types::B1T_LONG)
				{
					auto err = stm8_load(cmd.args[1], B1Types::B1T_LONG, LVT::LVT_REG);
					if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
					{
						return err;
					}

					_curr_code_sec->add_op(L"PUSH 2"); //4B BYTE_VALUE
					_stack_ptr += 1;
					_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_STR32"); //AD SIGNED_BYTE_OFFSET (CALLR)
					_req_symbols.insert(L"__LIB_STR_STR32");
					_curr_code_sec->add_op(L"POP A"); //84
					_stack_ptr -= 1;
				}

				_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_" + out_dev + L"_OUT"); //AD SIGNED_BYTE_OFFSET (CALLR)
				_req_symbols.insert(L"__LIB_" + out_dev + L"_OUT");
			}

			_cmp_active = false;
			_retval_active = false;

			continue;
		}

		if(cmd.cmd == L"RST")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			std::wstring name_space = cmd.args[0][0].value;

			if(_data_stmts.find(name_space) == _data_stmts.end())
			{
				return C1STM8_T_ERROR::C1STM8_RES_ENODATA;
			}

			name_space = name_space.empty() ? std::wstring() : (name_space + L"::");

			if(cmd.args.size() == 1)
			{
				_curr_code_sec->add_op(L"LDW X, " + name_space + L"__DAT_START"); //AE WORD_VALUE
				_req_symbols.insert(name_space + L"__DAT_START");
			}
			else
			{
				auto rst_label = _dat_rst_labels.find(cmd.args[1][0].value);
				if(rst_label == _dat_rst_labels.end())
				{
					return C1STM8_T_ERROR::C1STM8_RES_EUNRESSYMBOL;
				}
				_curr_code_sec->add_op(L"LDW X, " + rst_label->second); //AE WORD_VALUE
			}

			_curr_code_sec->add_op(L"LDW (" + name_space + L"__DAT_PTR), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
			_req_symbols.insert(name_space + L"__DAT_PTR");

			_cmp_active = false;
			_retval_active = false;

			continue;
		}

		if(cmd.cmd == L"READ")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			std::wstring name_space = cmd.args[0][0].value;

			if(_data_stmts.find(name_space) == _data_stmts.end())
			{
				return C1STM8_T_ERROR::C1STM8_RES_ENODATA;
			}

			name_space = name_space.empty() ? std::wstring() : (name_space + L"::");

			// load value
			if(cmd.args[1][0].type == B1Types::B1T_BYTE)
			{
				_curr_code_sec->add_op(L"LDW X, (" + name_space + L"__DAT_PTR)"); //BE SHORT_ADDRESS, CE LONG_ADDRESS
				_curr_code_sec->add_op(L"INCW X"); //5C
				_curr_code_sec->add_op(L"LD A, (X)");
				_curr_code_sec->add_op(L"INCW X"); //5C
				_curr_code_sec->add_op(L"LDW (" + name_space + L"__DAT_PTR), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
			}
			else
			if(cmd.args[1][0].type == B1Types::B1T_INT || cmd.args[1][0].type == B1Types::B1T_WORD || cmd.args[1][0].type == B1Types::B1T_STRING)
			{
				_curr_code_sec->add_op(L"LDW X, (" + name_space + L"__DAT_PTR)"); //BE SHORT_ADDRESS, CE LONG_ADDRESS
				_curr_code_sec->add_op(L"PUSHW X"); // 89
				_stack_ptr += 2;
				_curr_code_sec->add_op(L"INCW X"); //5C
				_curr_code_sec->add_op(L"INCW X"); //5C
				_curr_code_sec->add_op(L"LDW (" + name_space + L"__DAT_PTR), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
				_curr_code_sec->add_op(L"POPW X"); //85
				_stack_ptr -= 2;
				_curr_code_sec->add_op(L"LDW X, (X)"); //FE
			}
			else
			{
				// LONG type
				_curr_code_sec->add_op(L"LDW X, (" + name_space + L"__DAT_PTR)"); //BE SHORT_ADDRESS, CE LONG_ADDRESS
				_curr_code_sec->add_op(L"LDW Y, X"); //90 93
				_curr_code_sec->add_op(L"ADDW X, 4"); //1C WORD_VALUE
				_curr_code_sec->add_op(L"LDW (" + name_space + L"__DAT_PTR), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
				_curr_code_sec->add_op(L"LDW X, Y"); //93
				_curr_code_sec->add_op(L"LDW Y, (Y)"); //90 FE
				_curr_code_sec->add_op(L"LDW X, (2, X)"); //EE BYTE_OFFSET
			}
			_req_symbols.insert(name_space + L"__DAT_PTR");

			// store value
			auto err = stm8_store(cmd.args[1]);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			_cmp_active = false;
			_retval_active = false;

			continue;
		}

		if(cmd.cmd == L"RETVAL")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			auto err = stm8_load(cmd.args[0], cmd.args[1][0].type, LVT::LVT_REG);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			_cmp_active = false;
			_retval_active = true;
			_retval_type = cmd.args[1][0].type;

			continue;
		}

		if(cmd.cmd == L"RET")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			// release strings passed as arguments
			// temporary solution for a single argument case: function prologue code stores it in stack
			if(_curr_udef_arg_offsets.size() == 1)
			{
				if(_curr_udef_str_arg_offsets.size() == 1)
				{
					int32_t offset;

					if(_retval_type == B1Types::B1T_BYTE)
					{
						_curr_code_sec->add_op(L"PUSH A"); //88
						_stack_ptr += 1;
						offset = _stack_ptr - _curr_udef_args_size + 1;
						_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)"); //1E BYTE_OFFSET
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_RLS");
						_curr_code_sec->add_op(L"POP A"); //84
						_stack_ptr -= 1;
					}
					else
					if(_retval_type == B1Types::B1T_INT || _retval_type == B1Types::B1T_WORD || _retval_type == B1Types::B1T_STRING)
					{
						_curr_code_sec->add_op(L"PUSHW X"); //89
						_stack_ptr += 2;
						offset = _stack_ptr - _curr_udef_args_size + 1;
						_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)"); //1E BYTE_OFFSET
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_RLS");
						_curr_code_sec->add_op(L"POPW X"); //85
						_stack_ptr -= 2;
					}
					else
					{
						// LONG type
						_curr_code_sec->add_op(L"PUSHW X"); //89
						_curr_code_sec->add_op(L"PUSHW Y"); //90 89
						_stack_ptr += 4;
						offset = _stack_ptr - _curr_udef_args_size + 1;
						_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)"); //1E BYTE_OFFSET
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_RLS");
						_curr_code_sec->add_op(L"POPW Y"); //90 85
						_curr_code_sec->add_op(L"POPW X"); //85
						_stack_ptr -= 4;
					}
				}
			}
			else
			{
				int32_t offset;

				for(const auto &sa: _curr_udef_str_arg_offsets)
				{
					if(_retval_type == B1Types::B1T_BYTE)
					{
						_curr_code_sec->add_op(L"PUSH A"); //88
						_stack_ptr += 1;
						offset = _stack_ptr + _global_settings.GetRetAddressSize() + sa;
						_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)"); //1E BYTE_OFFSET
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_RLS");
						_curr_code_sec->add_op(L"POP A"); //84
						_stack_ptr -= 1;
					}
					else
					if(_retval_type == B1Types::B1T_INT || _retval_type == B1Types::B1T_WORD || _retval_type == B1Types::B1T_STRING)
					{
						_curr_code_sec->add_op(L"PUSHW X"); //89
						_stack_ptr += 2;
						offset = _stack_ptr + _global_settings.GetRetAddressSize() + sa;
						_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)"); //1E BYTE_OFFSET
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_RLS");
						_curr_code_sec->add_op(L"POPW X"); //85
						_stack_ptr -= 2;
					}
					else
					{
						// LONG type
						_curr_code_sec->add_op(L"PUSHW X"); //89
						_curr_code_sec->add_op(L"PUSHW Y"); //90 89
						_stack_ptr += 4;
						offset = _stack_ptr + _global_settings.GetRetAddressSize() + sa;
						_curr_code_sec->add_op(L"LDW X, (" + Utils::str_tohex16(offset) + L", SP)"); //1E BYTE_OFFSET
						_curr_code_sec->add_op(_call_stmt + L" " + L"__LIB_STR_RLS"); //AD SIGNED_BYTE_OFFSET (CALLR)
						_req_symbols.insert(L"__LIB_STR_RLS");
						_curr_code_sec->add_op(L"POPW Y"); //90 85
						_curr_code_sec->add_op(L"POPW X"); //85
						_stack_ptr -= 4;
					}
				}
			}

			// temporary solution for a single argument case: function prologue code stores it in stack
			if(_curr_udef_arg_offsets.size() == 1)
			{
				_curr_code_sec->add_op(L"ADDW SP, " + std::to_wstring(_curr_udef_args_size)); //5B BYTE_VALUE
				_stack_ptr -= _curr_udef_args_size;
			}

			if(_stack_ptr != 0)
			{
				// probably some local variables are not released
				if(_global_settings.GetFixRetStackPtr())
				{
					_curr_code_sec->add_op(L"ADDW SP, " + std::to_wstring(_stack_ptr)); //5B BYTE_VALUE
				}
				else
				{
					_warnings.push_back(std::make_tuple(GetCurrLineNum(), GetCurrFileName(), C1STM8_T_WARNING::C1STM8_WRN_WRETSTKOVF));
				}
			}

			if(int_handler)
			{
				_curr_code_sec->add_op(L"IRET"); //80
			}
			else
			{
				_curr_code_sec->add_op(_ret_stmt); //RET 81, RETF 87
			}

			_cmp_active = false;
			_retval_active = false;

			_allocated_arrays.clear();

			_curr_udef_args_size = 0;
			_curr_udef_arg_offsets.clear();
			_curr_udef_str_arg_offsets.clear();

			continue;
		}

		if(cmd.cmd == L"SET")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			if(cmd.args[0][0].value == L"ERR")
			{
				if(!B1CUtils::is_num_val(cmd.args[1][0].value))
				{
					return static_cast<C1STM8_T_ERROR>(B1_RES_ETYPMISM);
				}

				int32_t n;
				auto err = Utils::str2int32(cmd.args[1][0].value, n);
				if(err != B1_RES_OK)
				{
					return static_cast<C1STM8_T_ERROR>(err);
				}

				_curr_code_sec->add_op(L"MOV (__LIB_ERR_LAST_ERR), " + std::to_wstring(n)); //35 BYTE_VALUE LONG_ADDRESS
				_init_files.push_back(L"__LIB_ERR_LAST_ERR");
			}

			_cmp_active = false;
			_retval_active = false;

			continue;
		}

		if(cmd.cmd == L"END")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			int_handler = false;

			_cmp_active = false;
			_retval_active = false;

			_allocated_arrays.clear();

			_curr_udef_args_size = 0;
			_curr_udef_arg_offsets.clear();
			_curr_udef_str_arg_offsets.clear();

			for(const auto &epc: _end_placement)
			{
				auto err = load_inline(0, epc.second.at(L"FILE_NAME"), std::next(ci), epc.second);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
			}
			_end_placement.clear();

			continue;
		}

		if(cmd.cmd == L"ERR")
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			_init_files.push_back(L"__LIB_ERR_LAST_ERR");
			if(cmd.args[0][0].value.empty())
			{
				_curr_code_sec->add_op(L"TNZ (__LIB_ERR_LAST_ERR)"); // 3D SHORT_ADDRESS 72 5D LONG_ADDRESS
				_curr_code_sec->add_op(L"JRNE " + cmd.args[1][0].value); //26 SIGNED_BYTE_OFFSET
			}
			else
			{
				_curr_code_sec->add_op(L"LD A, (__LIB_ERR_LAST_ERR)"); //B6 SHORT_ADDRESS C6 LONG_ADDRESS
				_curr_code_sec->add_op(L"CP A, " + cmd.args[0][0].value); //A1 BYTE_VALUE
				_curr_code_sec->add_op(L"JREQ " + cmd.args[1][0].value); //27 SIGNED_BYTE_OFFSET
			}
			_req_symbols.insert(cmd.args[1][0].value);

			_cmp_active = false;

			continue;
		}

		if(cmd.cmd == L"IMP" || cmd.cmd == L"INI")
		{
			continue;
		}

		if(cmd.cmd == L"INT")
		{
			int_handler = true;

			std::string irq_name = Utils::wstr2str(cmd.args[0][0].value);
			int int_ind = _global_settings.GetInterruptIndex(irq_name);
			
			if(int_ind < 0)
			{
				// unknown interrupt name
				return C1STM8_T_ERROR::C1STM8_RES_EUNKINT;
			}

 			auto int_hnd = _irq_handlers.find(int_ind);
			if(int_hnd != _irq_handlers.end() && !int_hnd->second.empty())
			{
				// multiple handlers are defined for the same interrupt
				return C1STM8_T_ERROR::C1STM8_RES_EMULTINTHND;
			}

			const auto int_lbl_name = L"__" + cmd.args[0][0].value;

			_curr_code_sec->add_lbl(int_lbl_name);
			_all_symbols.insert(int_lbl_name);
			
			_irq_handlers[int_ind] = int_lbl_name;
			_req_symbols.insert(int_lbl_name);

			continue;
		}

		if(cmd.cmd == L"JMP")
		{
			_curr_code_sec->add_op(L"JRA " + cmd.args[0][0].value); //20 SIGNED_BYTE_OFFSET
			_req_symbols.insert(cmd.args[0][0].value);

			_cmp_active = false;

			continue;
		}

		if(cmd.cmd == L"JT" || cmd.cmd == L"JF")
		{
			if(!_cmp_active)
			{
				return C1STM8_T_ERROR::C1STM8_RES_ENOCMPOP;
			}

			if(_cmp_op == L"==")
			{
				if(cmd.cmd == L"JT")
				{
					_curr_code_sec->add_op(L"JREQ " + cmd.args[0][0].value); //27 SIGNED_BYTE_OFFSET
				}
				else
				{
					_curr_code_sec->add_op(L"JRNE " + cmd.args[0][0].value); //26 SIGNED_BYTE_OFFSET
				}
			}
			else
			if(_cmp_op == L"<>")
			{
				if(cmd.cmd == L"JT")
				{
					_curr_code_sec->add_op(L"JRNE " + cmd.args[0][0].value); //26 SIGNED_BYTE_OFFSET
				}
				else
				{
					_curr_code_sec->add_op(L"JREQ " + cmd.args[0][0].value); //27 SIGNED_BYTE_OFFSET
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
						_curr_code_sec->add_op(L"JRSGT " + cmd.args[0][0].value); //2C SIGNED_BYTE_OFFSET
					}
					else
					{
						_curr_code_sec->add_op(L"JRSLE " + cmd.args[0][0].value); //2D SIGNED_BYTE_OFFSET
					}
				}
				else
				{
					// unsigned comparison
					if(cmd.cmd == L"JT")
					{
						_curr_code_sec->add_op(L"JRUGT " + cmd.args[0][0].value); //22 SIGNED_BYTE_OFFSET
					}
					else
					{
						_curr_code_sec->add_op(L"JRULE " + cmd.args[0][0].value); //23 SIGNED_BYTE_OFFSET
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
						_curr_code_sec->add_op(L"JRSGE " + cmd.args[0][0].value); //2E SIGNED_BYTE_OFFSET
					}
					else
					{
						_curr_code_sec->add_op(L"JRSLT " + cmd.args[0][0].value); //2F SIGNED_BYTE_OFFSET
					}
				}
				else
				{
					// unsigned comparison
					if(cmd.cmd == L"JT")
					{
						_curr_code_sec->add_op(L"JRUGE " + cmd.args[0][0].value); //24 SIGNED_BYTE_OFFSET
					}
					else
					{
						_curr_code_sec->add_op(L"JRULT " + cmd.args[0][0].value); //25 SIGNED_BYTE_OFFSET
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
						_curr_code_sec->add_op(L"JRSLT " + cmd.args[0][0].value); //2F SIGNED_BYTE_OFFSET
					}
					else
					{
						_curr_code_sec->add_op(L"JRSGE " + cmd.args[0][0].value); //2E SIGNED_BYTE_OFFSET
					}
				}
				else
				{
					// unsigned comparison
					if(cmd.cmd == L"JT")
					{
						_curr_code_sec->add_op(L"JRULT " + cmd.args[0][0].value); //25 SIGNED_BYTE_OFFSET
					}
					else
					{
						_curr_code_sec->add_op(L"JRUGE " + cmd.args[0][0].value); //24 SIGNED_BYTE_OFFSET
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
						_curr_code_sec->add_op(L"JRSLE " + cmd.args[0][0].value); //2D SIGNED_BYTE_OFFSET
					}
					else
					{
						_curr_code_sec->add_op(L"JRSGT " + cmd.args[0][0].value); //2C SIGNED_BYTE_OFFSET
					}
				}
				else
				{
					// unsigned comparison
					if(cmd.cmd == L"JT")
					{
						_curr_code_sec->add_op(L"JRULE " + cmd.args[0][0].value); //23 SIGNED_BYTE_OFFSET
					}
					else
					{
						_curr_code_sec->add_op(L"JRUGT " + cmd.args[0][0].value); //22 SIGNED_BYTE_OFFSET
					}
				}
			}
			else
			{
				return C1STM8_T_ERROR::C1STM8_RES_EUNKINST;
			}

			_req_symbols.insert(cmd.args[0][0].value);

			_retval_active = false;

			continue;
		}

		if(B1CUtils::is_un_op(cmd))
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			// unary operation
			auto err = stm8_un_op(cmd);
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				return err;
			}

			_cmp_active = false;
			_retval_active = false;

			continue;
		}

		if(B1CUtils::is_bin_op(cmd))
		{
			if(_out_src_lines)
			{
				_curr_code_sec->add_comment(Utils::str_trim(_src_lines[cmd.src_line_id]));
			}

			// additive operation
			if(cmd.cmd == L"+" || cmd.cmd == L"-")
			{
				auto err = stm8_add_op(cmd);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
			}
			else
			// multiplicative operation
			if(cmd.cmd == L"*" || cmd.cmd == L"/" || cmd.cmd == L"%" || cmd.cmd == L"^")
			{
				auto err = stm8_mul_op(cmd);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
			}
			else
			// bitwise operation
			if(cmd.cmd == L"&" || cmd.cmd == L"|" || cmd.cmd == L"~")
			{
				auto err = stm8_bit_op(cmd);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
			}
			else
			// shift operation
			if(cmd.cmd == L"<<" || cmd.cmd == L">>")
			{
				auto err = stm8_shift_op(cmd);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
			}

			_cmp_active = false;
			_retval_active = false;

			continue;
		}

		if(B1CUtils::is_log_op(cmd))
		{
			if(cmd.args[0][0].type == B1Types::B1T_STRING || cmd.args[1][0].type == B1Types::B1T_STRING)
			{
				// string comparison
				auto err = stm8_str_cmp_op(cmd);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
			}
			else
			{
				// numeric comparison
				auto err = stm8_num_cmp_op(cmd);
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					return err;
				}
			}

			_retval_active = false;

			continue;
		}

		return C1STM8_T_ERROR::C1STM8_RES_EUNKINST;
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}


C1STM8Compiler::C1STM8Compiler(bool out_src_lines, bool opt_nocheck)
: B1_CMP_CMDS(L"", 32768, 32768)
, _data_size(0)
, _const_size(0)
, _stack_ptr(0)
, _curr_udef_args_size(0)
, _out_src_lines(out_src_lines)
, _opt_nocheck(opt_nocheck)
, _cmp_active(false)
, _cmp_type(B1Types::B1T_UNKNOWN)
, _retval_active(false)
, _retval_type(B1Types::B1T_UNKNOWN)
, _inline_asm(false)
, _page0(true)
, _next_temp_namespace_id(32768)
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

C1STM8_T_ERROR C1STM8Compiler::Load(const std::vector<std::string> &file_names)
{
	C1STM8_T_ERROR err = C1STM8_T_ERROR::C1STM8_RES_EIFEMPTY;

	clear();

	_curr_name_space = gen_next_tmp_namespace();

	_src_lines.clear();

	_inline_asm = false;

	_all_symbols.clear();
	_req_symbols.clear();
	
	_init_files.clear();

	// used as source line id (to output source text)
	_curr_src_line_id = -1;

	_curr_src_file_id = -1;
	_curr_line_cnt = 0;

	for(const auto &fn: file_names)
	{
		if(_src_file_name_ids.find(fn) == _src_file_name_ids.cend())
		{
			_src_file_names.push_back(fn);
			_src_file_name_ids[fn] = _src_file_names.size() - 1;
		}

		// used for line number output (in error messages)
		_curr_line_cnt = 0;
		_curr_src_file_id = _src_file_name_ids[fn];

		std::FILE *ofp = std::fopen(fn.c_str(), "r");
		if(ofp == nullptr)
		{
			return C1STM8_T_ERROR::C1STM8_RES_EFOPEN;
		}

		std::wstring line;

		while(true)
		{
			err = static_cast<C1STM8_T_ERROR>(Utils::read_line(ofp, line));
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				break;
			}

			_curr_src_line_id++;

			_src_lines[_curr_src_line_id] = line;

			_curr_line_cnt++;

			err = load_next_command(line, cend());
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				break;
			}
		}

		std::fclose(ofp);

		if(err == static_cast<C1STM8_T_ERROR>(B1_RES_EEOF) && _curr_line_cnt == 0)
		{
			_curr_line_cnt = 0;
			err = C1STM8_T_ERROR::C1STM8_RES_EIFEMPTY;
			break;
		}

		if(err == static_cast<C1STM8_T_ERROR>(B1_RES_EEOF))
		{
			err = C1STM8_T_ERROR::C1STM8_RES_OK;
		}

		if(_inline_asm && err == C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			err = static_cast<C1STM8_T_ERROR>(B1_RES_ESYNTAX);
			break;
		}

		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			break;
		}
	}

	return err;
}

C1STM8_T_ERROR C1STM8Compiler::Compile()
{
	_curr_src_file_id = -1;
	_curr_line_cnt = 0;

	auto err = read_ufns();
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	err = read_and_check_locals();
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	err = read_and_check_vars();
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	err = process_imm_str_values();
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::WriteCode(bool code_init)
{
	_curr_code_sec = nullptr;

	auto err = write_data_sec();
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	err = write_const_sec();
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	_curr_code_sec = code_init ? &_code_init_sec : &_code_sec;

	err = write_code_sec();
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		return err;
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::WriteCodeInitBegin()
{
	// interrupt vector table
	_code_init_sec.add_op(L"INT __START");
	_req_symbols.insert(L"__START");

	int prev = 0;

	for(const auto &i: _irq_handlers)
	{
		for(int n = prev + 1; n < i.first; n++)
		{
			_code_init_sec.add_op(L"INT __UNHANDLED");
			_req_symbols.insert(L"__UNHANDLED");
		}

		_code_init_sec.add_op(L"INT " + i.second);
		_req_symbols.insert(i.second);

		prev = i.first;
	}

	if(_req_symbols.find(L"__UNHANDLED") != _req_symbols.end())
	{
		// unhandled interrupt handler (empty loop)
		_code_init_sec.add_lbl(L"__UNHANDLED");
		_all_symbols.insert(L"__UNHANDLED");
		_code_init_sec.add_op(L"JRA __UNHANDLED"); //20 SIGNED_BYTE_OFFSET
		_req_symbols.insert(L"__UNHANDLED");
	}

	// init code begin
	_code_init_sec.add_lbl(L"__START");
	_all_symbols.insert(L"__START");

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::WriteCodeInitDAT()
{
	// DAT statements initialization code
	for(auto const &ns: _data_stmts_init)
	{
		std::wstring name_space = ns.empty() ? std::wstring() : (ns + L"::");

		_code_init_sec.add_op(L"LDW X, " + name_space + L"__DAT_START"); //AE WORD_VALUE
		_req_symbols.insert(name_space + L"__DAT_START");
		_code_init_sec.add_op(L"LDW (" + name_space + L"__DAT_PTR), X"); //BF SHORT_ADDRESS CF LONG_ADDRESS
		_req_symbols.insert(name_space + L"__DAT_PTR");
	}
	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::WriteCodeInitEnd()
{
	if(_const_size != 0)
	{
		_code_init_sec.add_op(L"JRA __CODE_START"); //20 SIGNED_BYTE_OFFSET
	}

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::Save(const std::string &file_name)
{
	std::FILE *ofs = std::fopen(file_name.c_str(), "w");
	if(ofs == nullptr)
	{
		return C1STM8_T_ERROR::C1STM8_RES_EFOPEN;
	}

	if(!_page0_sec.empty())
	{
		std::fwprintf(ofs, L".DATA PAGE0\n");

		for(const auto &op: _page0_sec)
		{
			if(!op._comment.empty())
			{
				std::fwprintf(ofs, L"; %ls\n", op._comment.c_str());
			}

			switch(op._type)
			{
				case AOT::AOT_LABEL:
					std::fwprintf(ofs, L":%ls\n", op._data.c_str());
					break;
				default:
					std::fwprintf(ofs, L"%ls\n", op._data.c_str());
					break;
			}
		}

		std::fwprintf(ofs, L"\n");
	}

	if(!_data_sec.empty())
	{
		std::fwprintf(ofs, L".DATA\n");

		for(const auto &op: _data_sec)
		{
			if(!op._comment.empty())
			{
				std::fwprintf(ofs, L"; %ls\n", op._comment.c_str());
			}

			switch(op._type)
			{
				case AOT::AOT_LABEL:
					std::fwprintf(ofs, L":%ls\n", op._data.c_str());
					break;
				default:
					std::fwprintf(ofs, L"%ls\n", op._data.c_str());
					break;
			}
		}

		std::fwprintf(ofs, L"\n");
	}

	int32_t ss = _global_settings.GetStackSize();
	int32_t hs = _global_settings.GetHeapSize();

	// use all available RAM memory for heap
	if(hs == 0)
	{
		hs = _global_settings.GetRAMSize() - _data_size - ss;
	}

	// emit warning or error if heap size is <= 0
	if(hs > 0)
	{
		std::fwprintf(ofs, L".HEAP\n");
		std::fwprintf(ofs, L"DB (0x%X)\n\n", (int)hs);
	}
	else
	{
		_warnings.push_back(std::make_tuple(-1, "", C1STM8_T_WARNING::C1STM8_WRN_WWRNGHEAPSIZE));
	}

	// emit warning if stack size is zero
	if(ss > 0)
	{
		std::fwprintf(ofs, L".STACK\n");
		std::fwprintf(ofs, L"DB (0x%X)\n\n", (int)ss);
	}
	else
	{
		_warnings.push_back(std::make_tuple(-1, "", C1STM8_T_WARNING::C1STM8_WRN_WWRNGSTKSIZE));
	}

	if(!_code_init_sec.empty())
	{
		std::fwprintf(ofs, L".CODE INIT\n");

		for(const auto &op: _code_init_sec)
		{
			if(!op._comment.empty())
			{
				std::fwprintf(ofs, L"; %ls\n", op._comment.c_str());
			}

			switch(op._type)
			{
				case AOT::AOT_LABEL:
					std::fwprintf(ofs, L":%ls\n", op._data.c_str());
					break;
				default:
					std::fwprintf(ofs, L"%ls\n", op._data.c_str());
					break;
			}
		}

		std::fwprintf(ofs, L"\n");
	}

	if(!_const_sec.empty())
	{
		std::fwprintf(ofs, L".CONST\n");

		for(const auto &op: _const_sec)
		{
			if(!op._comment.empty())
			{
				std::fwprintf(ofs, L"; %ls\n", op._comment.c_str());
			}

			switch(op._type)
			{
				case AOT::AOT_LABEL:
					std::fwprintf(ofs, L":%ls\n", op._data.c_str());
					break;
				default:
					std::fwprintf(ofs, L"%ls\n", op._data.c_str());
					break;
			}
		}

		std::fwprintf(ofs, L"\n");
	}

	if(!_code_sec.empty())
	{
		std::fwprintf(ofs, L".CODE\n");

		for(const auto &op: _code_sec)
		{
			if(!op._comment.empty())
			{
				std::fwprintf(ofs, L"; %ls\n", op._comment.c_str());
			}

			switch(op._type)
			{
				case AOT::AOT_LABEL:
					std::fwprintf(ofs, L":%ls\n", op._data.c_str());
					break;
				default:
					std::fwprintf(ofs, L"%ls\n", op._data.c_str());
					break;
			}
		}

		std::fwprintf(ofs, L"\n");
	}

	std::fclose(ofs);

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::GetUndefinedSymbols(std::set<std::wstring> &symbols)
{
	std::set_difference(_req_symbols.begin(), _req_symbols.end(), _all_symbols.begin(), _all_symbols.end(), std::inserter(symbols, symbols.end()));

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::GetResolvedSymbols(std::set<std::wstring> &symbols)
{
	std::copy(_all_symbols.begin(), _all_symbols.end(), std::inserter(symbols, symbols.end()));

	return C1STM8_T_ERROR::C1STM8_RES_OK;
}

C1STM8_T_ERROR C1STM8Compiler::GetInitFiles(std::vector<std::wstring> &init_files)
{
	std::copy(_init_files.begin(), _init_files.end(), std::back_inserter(init_files));

	return C1STM8_T_ERROR::C1STM8_RES_OK;
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

static void c1stm8_print_warnings(const std::vector<std::tuple<int32_t, std::string, C1STM8_T_WARNING>> &wrns)
{
	for(auto &w: wrns)
	{
		c1stm8_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
	}
}


int main(int argc, char** argv)
{
	int i;
	int retcode = 0;
	bool print_err_desc = false;
	bool print_version = false;
	bool out_src_lines = false;
	bool no_asm = false;
	std::string ofn;
	bool args_error = false;
	std::string args_error_txt;
	std::string lib_dir;
	std::string MCU_name;
	int32_t stack_size = -1;
	int32_t heap_size = -1;
	const std::string target_name = "STM8";
	bool opt_nocheck = false;
	std::string args;


	// set numeric properties from C locale for sprintf to use dot as decimal delimiter
	std::setlocale(LC_NUMERIC, "C");


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
				MCU_name = Utils::str_toupper(argv[i]);
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
				_global_settings.SetRetAddressSize(STM8_RET_ADDR_SIZE_MM_SMALL);
			}
			else
			{
				_global_settings.SetMemModelLarge();
				_global_settings.SetRetAddressSize(STM8_RET_ADDR_SIZE_MM_LARGE);
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
				if(Utils::str_toupper(argv[i]) != "STM8")
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


	// read settings
	_global_settings.SetTargetName(target_name);
	_global_settings.SetMCUName(MCU_name);
	_global_settings.SetLibDir(lib_dir);

	// list of source files
	std::vector<std::string> src_files;
	for(int j = i; j < argc; j++)
	{
		src_files.push_back(argv[j]);
	}


	// read settings file if specified
	if(!MCU_name.empty())
	{
		bool cfg_file_read = false;

		auto file_name = _global_settings.GetLibFileName(MCU_name, ".io");
		if(!file_name.empty())
		{
			auto err = static_cast<C1STM8_T_ERROR>(_global_settings.ReadIoSettings(file_name));
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				c1stm8_print_error(err, -1, file_name, print_err_desc);
				return 2;
			}
			cfg_file_read = true;
		}

		file_name = _global_settings.GetLibFileName(MCU_name, ".cfg");
		if(!file_name.empty())
		{
			auto err = static_cast<C1STM8_T_ERROR>(_global_settings.Read(file_name));
			if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
			{
				c1stm8_print_error(err, -1, file_name, print_err_desc);
				return 3;
			}
			cfg_file_read = true;
		}

		if(!cfg_file_read)
		{
			//warning: unknown MCU name
			c1stm8_print_warnings(std::vector<std::tuple<int32_t, std::string, C1STM8_T_WARNING>>({ std::make_tuple((int32_t)-1, MCU_name, C1STM8_T_WARNING::C1STM8_WRN_WUNKNMCU) }));
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

	C1STM8Compiler c1stm8(out_src_lines, opt_nocheck);

	std::set<std::wstring> undef;
	std::set<std::wstring> resolved;
	
	std::vector<std::wstring> init;
	
	init.push_back(L"__INI_STK");
	init.push_back(L"__INI_SYS");
	init.push_back(L"__INI_DATA");

	bool code_init_first = true;

	bool code_init = false;

	while(true)
	{
		auto err = c1stm8.Load(src_files);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			c1stm8_print_warnings(c1stm8.GetWarnings());
			c1stm8_print_error(err, c1stm8.GetCurrLineNum(), c1stm8.GetCurrFileName(), print_err_desc);
			retcode = 4;
			break;
		}

		err = c1stm8.Compile();
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			c1stm8_print_warnings(c1stm8.GetWarnings());
			c1stm8_print_error(err, c1stm8.GetCurrLineNum(), c1stm8.GetCurrFileName(), print_err_desc);
			retcode = 5;
			break;
		}

		err = c1stm8.WriteCode(code_init);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			c1stm8_print_warnings(c1stm8.GetWarnings());
			c1stm8_print_error(err, c1stm8.GetCurrLineNum(), c1stm8.GetCurrFileName(), print_err_desc);
			retcode = 6;
			break;
		}

		err = c1stm8.GetUndefinedSymbols(undef);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			c1stm8_print_warnings(c1stm8.GetWarnings());
			c1stm8_print_error(err, -1, c1stm8.GetCurrFileName(), print_err_desc);
			retcode = 7;
			break;
		}

		err = c1stm8.GetResolvedSymbols(resolved);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			c1stm8_print_warnings(c1stm8.GetWarnings());
			c1stm8_print_error(err, -1, c1stm8.GetCurrFileName(), print_err_desc);
			retcode = 8;
			break;
		}

		err = c1stm8.GetInitFiles(init);
		if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
		{
			c1stm8_print_warnings(c1stm8.GetWarnings());
			c1stm8_print_error(err, -1, c1stm8.GetCurrFileName(), print_err_desc);
			retcode = 9;
			break;
		}

		for(const auto &r: resolved)
		{
			undef.erase(r);
		}

		src_files.clear();

		if(undef.empty())
		{
			if(code_init_first)
			{
				// write interrupt vector table
				err = c1stm8.WriteCodeInitBegin();
				if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
				{
					c1stm8_print_warnings(c1stm8.GetWarnings());
					c1stm8_print_error(err, -1, "", print_err_desc);
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
			c1stm8_print_warnings(c1stm8.GetWarnings());
			c1stm8_print_error(C1STM8_T_ERROR::C1STM8_RES_EUNRESSYMBOL, -1, err_file_name, print_err_desc);
			retcode = 11;
			break;
		}

		if(undef.empty())
		{
			resolved.insert(std::wstring(err_file_name.begin(), err_file_name.end()));
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
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		c1stm8_print_warnings(c1stm8.GetWarnings());
		c1stm8_print_error(err, -1, "", print_err_desc);
		retcode = 12;
		return retcode;
	}

	err = c1stm8.WriteCodeInitEnd();
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		c1stm8_print_warnings(c1stm8.GetWarnings());
		c1stm8_print_error(err, -1, "", print_err_desc);
		retcode = 13;
		return retcode;
	}

	err = c1stm8.Save(ofn);
	if(err != C1STM8_T_ERROR::C1STM8_RES_OK)
	{
		c1stm8_print_warnings(c1stm8.GetWarnings());
		c1stm8_print_error(err, -1, ofn, print_err_desc);
		retcode = 14;
		return retcode;
	}

	c1stm8_print_warnings(c1stm8.GetWarnings());

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
			retcode = 15;
		}
	}

	return retcode;
}
