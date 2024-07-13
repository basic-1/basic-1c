/*
 Intermediate code compiler
 Copyright (c) 2021-2024 Nikolay Pletnev
 MIT license

 c1.cpp: Intermediate code compiler
*/


#include <cwctype>
#include <cstring>
#include <iterator>
#include <algorithm>
#include <filesystem>

#include "moresym.h"

#include "c1.h"


extern Settings &_global_settings;


C1_T_ERROR C1Compiler::find_first_of(const std::wstring &str, const std::wstring &delimiters, size_t &off) const
{
	auto b = str.begin() + off;
	auto e = str.end();

	// skip leading blanks
	while(b != e && std::iswspace(*b)) b++;

	if(b == e)
	{
		off = std::wstring::npos;
		return C1_T_ERROR::C1_RES_OK;
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
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
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
			return C1_T_ERROR::C1_RES_OK;
		}

		if(delimiters.find(*b) == std::wstring::npos)
		{
			return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
		}

		off = b - str.begin();
	}
	else
	{
		off = str.find_first_of(delimiters, off);
	}

	return C1_T_ERROR::C1_RES_OK;
}

std::wstring C1Compiler::get_next_value(const std::wstring &str, const std::wstring &delimiters, size_t &next_off) const
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

bool C1Compiler::check_label_name(const std::wstring &name) const
{
	return (name.find_first_not_of(L"_:0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::wstring::npos);
}

bool C1Compiler::check_stdfn_name(const std::wstring &name) const
{
	return (name.find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz$") == std::wstring::npos);
}

bool C1Compiler::check_cmd_name(const std::wstring &name) const
{
	if(B1CUtils::is_bin_op(name) || B1CUtils::is_un_op(name) || B1CUtils::is_log_op(name))
	{
		return true;
	}
	return (name.find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::wstring::npos);
}

bool C1Compiler::check_type_name(const std::wstring &name) const
{
	return (Utils::get_type_by_name(name) != B1Types::B1T_UNKNOWN);
}

bool C1Compiler::check_namespace_name(const std::wstring &name) const
{
	return (name.find_first_not_of(L"_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::wstring::npos);
}

bool C1Compiler::check_address(const std::wstring &address) const
{
	int32_t n;
	return (Utils::str2int32(address, n) == B1_RES_OK);
}

bool C1Compiler::check_num_val(const std::wstring &numval) const
{
	int32_t n;
	return (Utils::str2int32(numval, n) == B1_RES_OK);
}

bool C1Compiler::check_str_val(const std::wstring &strval) const
{
	std::wstring s;
	return (B1CUtils::get_string_data(strval, s) == B1_RES_OK);
}

C1_T_ERROR C1Compiler::get_cmd_name(const std::wstring &str, std::wstring &name, size_t &next_off) const
{
	name = Utils::str_trim(get_next_value(str, L",", next_off));
	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::get_simple_arg(const std::wstring &str, B1_TYPED_VALUE &arg, size_t &next_off) const
{
	auto sval = Utils::str_trim(get_next_value(str, L",)", next_off));
	arg.value = sval;
	arg.type = B1Types::B1T_UNKNOWN;
	return sval.empty() ? static_cast<C1_T_ERROR>(B1_RES_ESYNTAX) : C1_T_ERROR::C1_RES_OK;
}

std::wstring C1Compiler::gen_next_tmp_namespace()
{
	return L"NS" + std::to_wstring(_next_temp_namespace_id++);
}

// replaces default namespace mark (::) with namespace name
std::wstring C1Compiler::add_namespace(const std::wstring &name) const
{
	if(name.size() > 2 && name[0] == L':' && name[1] == L':')
	{
		return _curr_name_space.empty() ? name.substr(2) : (_curr_name_space + name);
	}

	return name;
}

C1_T_ERROR C1Compiler::get_arg(const std::wstring &str, B1_CMP_ARG &arg, size_t &next_off) const
{
	bool check_optional = false;

	arg.clear();
	
	auto name = Utils::str_trim(get_next_value(str, L"<", next_off));
	if(next_off == std::wstring::npos)
	{
		return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
	}
	if(!check_label_name(name) && !check_num_val(name) && !check_str_val(name) && !check_stdfn_name(name))
	{
		return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
	}
	name = add_namespace(name);

	auto type = Utils::str_trim(get_next_value(str, L">", next_off));
	if(next_off == std::wstring::npos)
	{
		return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
	}
	if(!check_type_name(type))
	{
		return C1_T_ERROR::C1_RES_EINVTYPNAME;
	}

	arg.push_back(B1_TYPED_VALUE(name, Utils::get_type_by_name(type)));

	name = Utils::str_trim(get_next_value(str, L"(,", next_off));
	if(!name.empty())
	{
		return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
	}

	if(next_off != std::wstring::npos && str[next_off - 1] == L'(')
	{
		while(true)
		{
			name = Utils::str_trim(get_next_value(str, L"<,)", next_off));
			if(next_off == std::wstring::npos)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			if(name.empty())
			{
				auto dc = str[next_off - 1];

				if(dc == L'<')
				{
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}

				// probably omitted function argument
				arg.push_back(B1_TYPED_VALUE(L""));
				check_optional = true;

				if(dc == L')')
				{
					name = Utils::str_trim(get_next_value(str, L",", next_off));
					if(!name.empty())
					{
						return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
					}

					break;
				}

				continue;
			}

			if(!check_label_name(name) && !check_num_val(name) && !check_str_val(name) && !check_stdfn_name(name))
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}
			name = add_namespace(name);

			type = Utils::str_trim(get_next_value(str, L">", next_off));
			if(next_off == std::wstring::npos)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}
			if(!check_type_name(type))
			{
				return C1_T_ERROR::C1_RES_EINVTYPNAME;
			}

			arg.push_back(B1_TYPED_VALUE(name, Utils::get_type_by_name(type)));

			name = Utils::str_trim(get_next_value(str, L",)", next_off));
			if(!name.empty() || next_off == std::wstring::npos)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			if(str[next_off - 1] == L')')
			{
				name = Utils::str_trim(get_next_value(str, L",", next_off));
				if(!name.empty())
				{
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
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
			return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
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
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}
			}
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::replace_inline(std::wstring &line, const std::map<std::wstring, std::wstring> &inl_params)
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
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
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
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}
				auto err = Utils::str2int32(str, start);
				if(err != B1_RES_OK)
				{
					return static_cast<C1_T_ERROR>(err);
				}
				
				str = Utils::str_trim(get_next_value(line, L"}", offset1));
				err = Utils::str2int32(str, charnum);
				if(err != B1_RES_OK)
				{
					return static_cast<C1_T_ERROR>(err);
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
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			line.replace(offset, val_len, (start > ip.second.length()) ? L"" : ip.second.substr(start, (charnum < 0) ? std::wstring::npos : (size_t)charnum));
			offset = line.find(val_start);
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::load_inline(size_t offset, const std::wstring &line, const_iterator pos, const std::map<std::wstring, std::wstring> &inl_params)
{
	B1_TYPED_VALUE tv;

	// read file name
	auto err = get_simple_arg(line, tv, offset);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}
	if(offset != std::wstring::npos)
	{
		return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
	}

	const auto file_name = _global_settings.GetLibFileName(Utils::wstr2str(tv.value), ".b1c");
	if(file_name.empty())
	{
		return C1_T_ERROR::C1_RES_EFOPEN;
	}

	if(_inline_code.find(file_name) != _inline_code.end())
	{
		return C1_T_ERROR::C1_RES_ERECURINL;
	}

	_inline_code.insert(file_name);

	std::FILE *fp = std::fopen(file_name.c_str(), "r");
	if(fp == nullptr)
	{
		return C1_T_ERROR::C1_RES_EFOPEN;
	}

	auto saved_ns = _curr_name_space;
	_curr_name_space = gen_next_tmp_namespace();

	std::wstring inl_line;

	while(true)
	{
		err = static_cast<C1_T_ERROR>(Utils::read_line(fp, inl_line));
		if(err == static_cast<C1_T_ERROR>(B1_RES_EEOF))
		{
			err = C1_T_ERROR::C1_RES_OK;
			if(inl_line.empty())
			{
				break;
			}
		}

		if(err != C1_T_ERROR::C1_RES_OK)
		{
			break;
		}

		err = replace_inline(inl_line, inl_params);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			break;
		}

		err = load_next_command(inl_line, pos);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			break;
		}
	}

	_curr_name_space = saved_ns;

	std::fclose(fp);

	if(_inline_asm && err == C1_T_ERROR::C1_RES_OK)
	{
		return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
	}

	_inline_code.erase(file_name);

	return err;
}

C1_T_ERROR C1Compiler::load_next_command(const std::wstring &line, const_iterator pos)
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
				return C1_T_ERROR::C1_RES_EINVLBNAME;
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

			return C1_T_ERROR::C1_RES_OK;
		}

		// comment
		if(*b == L';')
		{
			return C1_T_ERROR::C1_RES_OK;
		}

		// command
		auto err = get_cmd_name(line, cmd, offset);
		if(err != C1_T_ERROR::C1_RES_OK)
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
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}

				return C1_T_ERROR::C1_RES_OK;
			}

			return process_asm_cmd(line);
		}

		if(!check_cmd_name(cmd))
		{
			return C1_T_ERROR::C1_RES_EINVCMDNAME;
		}

		if(cmd == L"ASM")
		{
			_asm_stmt_it = emit_inline_asm(pos);

			_inline_asm = true;

			if(offset != std::wstring::npos)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			return C1_T_ERROR::C1_RES_OK;
		}

		if(cmd == L"DEF")
		{
			// read fn name
			err = get_simple_arg(line, tv, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			if(!check_label_name(tv.value))
			{
				return C1_T_ERROR::C1_RES_EINVLBNAME;
			}
			tv.value = add_namespace(tv.value);
			args.push_back(tv.value);

			// read fn return type
			err = get_simple_arg(line, tv, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			if(!check_type_name(tv.value))
			{
				return C1_T_ERROR::C1_RES_EINVTYPNAME;
			}
			args.push_back(B1_CMP_ARG(tv.value, Utils::get_type_by_name(tv.value)));

			// read fn arguments types
			while(offset != std::wstring::npos)
			{
				err = get_simple_arg(line, tv, offset);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
				if(!check_type_name(tv.value))
				{
					return C1_T_ERROR::C1_RES_EINVTYPNAME;
				}
				args.push_back(B1_CMP_ARG(tv.value, Utils::get_type_by_name(tv.value)));
			}
		}
		else
		if(cmd == L"GA" || cmd == L"MA")
		{
			// read var. name
			err = get_simple_arg(line, tv, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			if(!check_label_name(tv.value))
			{
				return C1_T_ERROR::C1_RES_EINVLBNAME;
			}
			tv.value = add_namespace(tv.value);
			args.push_back(tv.value);

			// read var. type
			auto sval = Utils::str_trim(get_next_value(line, L",(", offset));
			if(sval.empty())
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}
			if(!check_type_name(sval))
			{
				return C1_T_ERROR::C1_RES_EINVTYPNAME;
			}
			args.push_back(B1_CMP_ARG(sval, Utils::get_type_by_name(sval)));

			bool is_static = false;

			// read optional type modifiers (V - stands for volatile, S - static, C - const)
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
					// here tv.value already contains variable name (for GA stmt)
					is_static = true;
					sval.erase(lpos, 1);
				}
				lpos = sval.find(L'C');
				if(lpos != std::wstring::npos)
				{
					// here tv.value already contains variable name (for GA stmt), CONST variables are always static
					is_static = true;
					type_mod += L'C';
					sval.erase(lpos, 1);
				}

				if(!sval.empty())
				{
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}

				if(!type_mod.empty())
				{
					args.back().push_back(B1_TYPED_VALUE(type_mod));
				}

				sval = Utils::str_trim(get_next_value(line, L",", offset));
				if(!sval.empty())
				{
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}
			}

			// read var. address
			if(cmd == L"MA")
			{
				err = get_simple_arg(line, tv, offset);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
				if(!Utils::check_const_name(tv.value) && !check_address(tv.value))
				{
					return static_cast<C1_T_ERROR>(B1_RES_EINVNUM);
				}
			}
			else
			if(is_static)
			{
				// turn static or const GA stmt into MA with variable name as address
				cmd = L"MA";
			}

			if(cmd == L"MA")
			{
				args.push_back(tv.value);
			}

			// get var. size
			int argnum = 0;
			while(offset != std::wstring::npos)
			{
				err = get_arg(line, arg, offset);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
				args.push_back(arg);
				argnum++;
			}
			if(argnum % 2 != 0)
			{
				return static_cast<C1_T_ERROR>(B1_RES_EWRARGCNT);
			}
		}
		else
		if(cmd == L"LA")
		{
			// read var. name
			err = get_simple_arg(line, tv, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			if(!check_label_name(tv.value))
			{
				return C1_T_ERROR::C1_RES_EINVLBNAME;
			}
			tv.value = add_namespace(tv.value);
			args.push_back(tv.value);

			// read var. type
			err = get_simple_arg(line, tv, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			if(!check_type_name(tv.value))
			{
				return C1_T_ERROR::C1_RES_EINVTYPNAME;
			}
			args.push_back(B1_CMP_ARG(tv.value, Utils::get_type_by_name(tv.value)));
		}
		else
		if(cmd == L"NS")
		{
			err = get_simple_arg(line, tv, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			if(!check_namespace_name(tv.value))
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}
			args.push_back(tv.value);

			// set namespace
			_curr_name_space = tv.value;
		}
		else
		if(cmd == L"OUT" || cmd == L"IN" || cmd == L"GET" || cmd == L"PUT" || cmd == L"TRR")
		{
			if(offset == std::wstring::npos)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			auto sval = Utils::str_trim(get_next_value(line, L",", offset));
			args.push_back(sval);
			if(offset == std::wstring::npos)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			err = get_arg(line, arg, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			if((cmd == L"GET" || cmd == L"TRR") && arg[0].type == B1Types::B1T_STRING)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
			}

			if(offset != std::wstring::npos)
			{
				if(arg[0].type != B1Types::B1T_BYTE)
				{
					return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
				}
				if(arg.size() != 2)
				{
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}

				args.push_back(arg);

				err = get_arg(line, arg, offset);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}

			args.push_back(arg);
		}
		else
		if(cmd == L"IOCTL")
		{
			if(offset == std::wstring::npos)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			// read device name
			err = get_arg(line, arg, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			if(!B1CUtils::is_str_val(arg[0].value))
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}
			args.push_back(arg);
			auto dev_name = _global_settings.GetIoDeviceName(arg[0].value.substr(1, arg[0].value.length() - 2));

			if(offset == std::wstring::npos)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			// read command
			err = get_arg(line, arg, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			if(!B1CUtils::is_str_val(arg[0].value))
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}
			args.push_back(arg);
			auto cmd_name = arg[0].value.substr(1, arg[0].value.length() - 2);

			// check data
			Settings::IoCmd iocmd;
			if(!_global_settings.GetIoCmd(dev_name, cmd_name, iocmd))
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}
			if(iocmd.accepts_data)
			{
				bool def_val = false;

				if(offset == std::wstring::npos)
				{
					if(iocmd.predef_only && !iocmd.def_val.empty())
					{
						def_val = true;
					}
					else
					{
						return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
					}
				}

				if(def_val)
				{
					arg.clear();
					arg.push_back(L"\"" + iocmd.def_val + L"\"");
				}
				else
				{
					// read data
					err = get_arg(line, arg, offset);
					if(err != C1_T_ERROR::C1_RES_OK)
					{
						return err;
					}
				}

				if(iocmd.predef_only)
				{
					if(!B1CUtils::is_str_val(arg[0].value))
					{
						return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
					}

					auto cmd_data = arg[0].value.substr(1, arg[0].value.length() - 2);
					if(iocmd.values.find(cmd_data) == iocmd.values.end())
					{
						return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
					}
				}
				else
				{
					if(iocmd.data_type == B1Types::B1T_LABEL)
					{
						const auto label = (arg[0].value.length() >= 3 && arg[0].value[0] == L'\"') ? arg[0].value.substr(1, arg[0].value.length() - 2) : arg[0].value;
						
						if(!check_label_name(label))
						{
							return C1_T_ERROR::C1_RES_EINVLBNAME;
						}
						_req_symbols.insert(label);
						arg[0].value = label;
					}
					else
					if(iocmd.data_type == B1Types::B1T_TEXT)
					{
						const auto text = (arg[0].value.length() >= 3 && arg[0].value[0] == L'\"') ? arg[0].value.substr(1, arg[0].value.length() - 2) : arg[0].value;
						arg[0].value = text;
					}
					else
					if(!B1CUtils::are_types_compatible(arg[0].type, iocmd.data_type))
					{
						return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
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
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}

				err = get_simple_arg(line, tv, offset);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
				if(!check_namespace_name(tv.value))
				{
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}
				args.push_back(tv.value);
			}

			if(offset != std::wstring::npos)
			{
				if(cmd == L"RST")
				{
					err = get_simple_arg(line, tv, offset);
					if(err != C1_T_ERROR::C1_RES_OK)
					{
						return err;
					}
					if(!check_label_name(tv.value))
					{
						return C1_T_ERROR::C1_RES_EINVLBNAME;
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
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			args.push_back(arg);

			err = get_simple_arg(line, tv, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			if(!check_type_name(tv.value))
			{
				return C1_T_ERROR::C1_RES_EINVTYPNAME;
			}
			args.push_back(B1_CMP_ARG(tv.value, Utils::get_type_by_name(tv.value)));
		}
		else
		if(cmd == L"SET")
		{
			if(offset == std::wstring::npos)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			err = get_simple_arg(line, tv, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			args.push_back(tv.value);

			if(tv.value == L"ERR")
			{
				err = get_arg(line, arg, offset);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
				args.push_back(arg);
			}
			else
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}
		}
		else
		if(cmd == L"JMP" || cmd == L"JF" || cmd == L"JT" || cmd == L"CALL" || cmd == L"GF" || cmd == L"LF" || cmd == L"IMP" || cmd == L"INI" || cmd == L"INT")
		{
			// read label name
			err = get_simple_arg(line, tv, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			if(!check_label_name(tv.value))
			{
				return C1_T_ERROR::C1_RES_EINVLBNAME;
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
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			auto sval = Utils::str_trim(get_next_value(line, L",", offset));
			args.push_back(sval);
			if(offset == std::wstring::npos)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			// read label name
			err = get_simple_arg(line, tv, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			if(!check_label_name(tv.value))
			{
				return C1_T_ERROR::C1_RES_EINVLBNAME;
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
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			err = get_simple_arg(line, tv, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			args.push_back(tv.value);

			while(offset != std::wstring::npos)
			{
				err = get_arg(line, arg, offset);
				if(err != C1_T_ERROR::C1_RES_OK)
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
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			err = get_simple_arg(line, tv, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			args.push_back(tv.value);

			if(offset == std::wstring::npos)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}
			err = get_arg(line, arg, offset);
			if(err != C1_T_ERROR::C1_RES_OK)
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
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
				args.push_back(arg);
			}

			if(!((B1CUtils::is_bin_op(cmd) && args.size() == 3) || args.size() == 2))
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}
		}
		else
		{
			return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
		}

		if(offset != std::wstring::npos)
		{
			return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
		}

		emit_command(cmd, pos, args);
	}

	return C1_T_ERROR::C1_RES_OK;
}

// function without arguments
const B1_CMP_FN *C1Compiler::get_fn(const B1_TYPED_VALUE &val) const
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

const B1_CMP_FN *C1Compiler::get_fn(const B1_CMP_ARG &arg) const
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

void C1Compiler::update_vars_stats(const std::wstring &name, VST storage_type, B1Types data_type)
{
	auto vs = _vars_stats.find(name);
	if(vs == _vars_stats.end())
	{
		_vars_stats.emplace(std::make_pair(name, std::make_tuple(storage_type, data_type, 1)));
	}
	else
	{
		auto vst = std::get<0>(vs->second);
		if(	(storage_type != VST::VST_UNKNOWN && vst == VST::VST_UNKNOWN) ||
			(!(storage_type == VST::VST_STAT_ARRAY || storage_type == VST::VST_CONST_ARRAY) && vst == VST::VST_ARRAY))
		{
			std::get<0>(vs->second) = storage_type;
		}
		if(data_type != B1Types::B1T_UNKNOWN && std::get<1>(vs->second) == B1Types::B1T_UNKNOWN)
		{
			std::get<1>(vs->second) = data_type;
		}
		std::get<2>(vs->second)++;
	}
}

// checks if the arg is variable or function call, arg can be scalar or subscripted variable or function call with
// omitted arguments. the function inserts default values for omitted arguments and put found variables into _vars map
// e.g.:	INSTR<INT>(, "133"<STRING>, "3"<STRING>) -> INSTR<INT>(1<INT>, "133"<STRING>, "3"<STRING>)
//			ARR<WORD>(I<INT>) -> check ARR and I types and put them into _vars if necessary
C1_T_ERROR C1Compiler::check_arg(B1_CMP_ARG &arg)
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
						
						_vars[a->value] = B1_CMP_VAR(a->value, a->type, 0, false, false, _curr_src_file_id, _curr_line_cnt);
					}
				}
				else
				{
					if(v->second.type == B1Types::B1T_UNKNOWN)
					{
						v->second.type = a->type;
					}
					else
					{
						B1Types com_type;
						bool comp_types = false;
						if(B1CUtils::get_com_type(v->second.type, a->type, com_type, comp_types) != B1_RES_OK || !comp_types)
						{
							return C1_T_ERROR::C1_RES_EVARTYPMIS;
						}
					}

					if(v->second.dim_num != 0)
					{
						return C1_T_ERROR::C1_RES_EVARDIMMIS;
					}
				}

				update_vars_stats(a->value, VST::VST_SIMPLE, a->type);
			}
			else
			{
				B1Types com_type;
				bool comp_types = false;
				if(B1CUtils::get_com_type(ma->second.type, a->type, com_type, comp_types) != B1_RES_OK || !comp_types)
				{
					return C1_T_ERROR::C1_RES_EVARTYPMIS;
				}

				if(ma->second.dim_num != 0)
				{
					return C1_T_ERROR::C1_RES_EVARDIMMIS;
				}
			}
		}
	}

	if(_locals.find(arg[0].value) != _locals.end() || B1CUtils::is_fn_arg(arg[0].value) || B1CUtils::is_imm_val(arg[0].value))
	{
		return C1_T_ERROR::C1_RES_OK;
	}

	auto fn = get_fn(arg);

	if(fn != nullptr)
	{
		// check function arg. count and their types
		if(arg.size() - 1 != fn->args.size())
		{
			return static_cast<C1_T_ERROR>(B1_RES_EWRARGCNT);
		}

		for(int a = 0; a < fn->args.size(); a++)
		{
			if(arg[a + 1].value.empty())
			{
				if(!fn->args[a].optional)
				{
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}

				arg[a + 1].value = fn->args[a].defval;
				arg[a + 1].type = fn->args[a].type;
			}
			else
			// STRING value cannot be passed to a function as non-STRING argument
			if(fn->args[a].type != B1Types::B1T_STRING && arg[a + 1].type == B1Types::B1T_STRING)
			{
				return static_cast<C1_T_ERROR>(B1_RES_EWARGTYPE);
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
						return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
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

					_vars[arg[0].value] = B1_CMP_VAR(arg[0].value, arg[0].type, arg.size() - 1, false, false, _curr_src_file_id, _curr_line_cnt);
					update_vars_stats(arg[0].value, (arg.size() == 1) ? VST::VST_SIMPLE : VST::VST_ARRAY, arg[0].type);
				}
			}
			else
			{
				if(v->second.type == B1Types::B1T_UNKNOWN)
				{
					v->second.type = arg[0].type;
					v->second.dim_num = arg.size() - 1;
				}
				else
				{
					B1Types com_type;
					bool comp_types = false;
					if(B1CUtils::get_com_type(v->second.type, arg[0].type, com_type, comp_types) != B1_RES_OK || !comp_types)
					{
						return C1_T_ERROR::C1_RES_EVARTYPMIS;
					}
				}

				if(v->second.dim_num != arg.size() - 1)
				{
					return C1_T_ERROR::C1_RES_EVARDIMMIS;
				}

				update_vars_stats(arg[0].value, (arg.size() == 1) ? VST::VST_SIMPLE : VST::VST_ARRAY, arg[0].type);
			}
		}
		else
		{
			B1Types com_type;
			bool comp_types = false;
			if(B1CUtils::get_com_type(ma->second.type, arg[0].type, com_type, comp_types) != B1_RES_OK || !comp_types)
			{
				return C1_T_ERROR::C1_RES_EVARTYPMIS;
			}

			if(ma->second.dim_num != arg.size() - 1)
			{
				return C1_T_ERROR::C1_RES_EVARDIMMIS;
			}

			if(arg.size() != 1 && (ma->second.is_const || (ma->second.use_symbol && ma->first == ma->second.symbol)))
			{
				// static or const array
				update_vars_stats(arg[0].value, ma->second.is_const ? VST::VST_CONST_ARRAY : VST::VST_STAT_ARRAY, arg[0].type);
			}
		}

		// check subscript types (should be numeric)
		for(auto a = arg.begin() + 1; a != arg.end(); a++)
		{
			if(a->type == B1Types::B1T_STRING)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
			}
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::read_ufns()
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
				return static_cast<C1_T_ERROR>(B1_RES_EIDINUSE);
			}

			if(_ufns.find(fname) != _ufns.end())
			{
				return C1_T_ERROR::C1_RES_EUFNREDEF;
			}

			B1_CMP_FN fn(fname, cmd.args[1][0].type, std::vector<B1Types>(), fname, false);
			for(auto at = cmd.args.begin() + 2; at != cmd.args.end(); at++)
			{
				fn.args.push_back(B1_CMP_FN_ARG(at->at(0).type, false, L""));
			}

			_ufns.emplace(std::make_pair(fname, fn));
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::read_and_check_locals()
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
				return C1_T_ERROR::C1_RES_ELCLREDEF;
			}

			_locals[cmd.args[0][0].value] = B1_CMP_VAR(cmd.args[0][0].value, cmd.args[1][0].type, 0, false, false, _curr_src_file_id, _curr_line_cnt);
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

// check variables types and sizes, set values of optional function arguments, build variable list
C1_T_ERROR C1Compiler::read_and_check_vars()
{
	//       name          GAs number
	std::map<std::wstring, int32_t> exp_alloc;
	std::map<std::wstring, std::tuple<int32_t, int32_t, bool>> arr_ranges;

	_vars.clear();
	_vars_order.clear();
	_vars_order_set.clear();
	_mem_areas.clear();
	_data_stmts.clear();
	_vars_stats.clear();

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
				return static_cast<C1_T_ERROR>(B1_RES_EIDINUSE);
			}

			const auto vtype = cmd.args[1][0].type;
			auto v = vars.find(vname);
			int32_t dims_off = is_ma ? 3 : 2;
			int32_t dims = cmd.args.size() - dims_off;
			bool is_volatile = (cmd.args[1].size() > 1) && (cmd.args[1][1].value.find(L'V') != std::wstring::npos);
			bool is_const = (cmd.args[1].size() > 1) && (cmd.args[1][1].value.find(L'C') != std::wstring::npos);

			if(is_ma)
			{
				// allow for mem. references to be temporarily added to variables (if the reference is used prior to MA statement)
				if(v != vars.end())
				{
					return C1_T_ERROR::C1_RES_EVARREDEF;
				}
			}
			else
			{
				if(_mem_areas.find(vname) != _mem_areas.end())
				{
					return C1_T_ERROR::C1_RES_EVARREDEF;
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
				vars[vname] = B1_CMP_VAR(vname, vtype, dims / 2, is_volatile, is_const, _curr_src_file_id, _curr_line_cnt);
				v = vars.find(vname);

				if(is_ma)
				{
					int32_t addr = 0, size = 0;
					// vname == cmd.args[2][0].value for static variables
					bool is_static = (vname == cmd.args[2][0].value);
					
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
							return static_cast<C1_T_ERROR>(err);
						}
					}

					// write address and size for MA variables
					v->second.address = addr;

					if(!B1CUtils::get_asm_type(vtype, nullptr, &size))
					{
						C1_T_ERROR::C1_RES_EINVTYPNAME;
					}
					// single element size (even for subscripted variables)
					v->second.size = size;
					v->second.fixed_size = true;

					if(is_static && _vars_order_set.find(vname) == _vars_order_set.end())
					{
						_vars_order_set.insert(vname);
						_vars_order.push_back(vname);
					}

					if((is_static || is_const) && dims != 0)
					{
						update_vars_stats(vname, is_const ? VST::VST_CONST_ARRAY : VST::VST_STAT_ARRAY, vtype);
					}
				}
				else
				{
					if(_vars_order_set.find(vname) == _vars_order_set.end())
					{
						_vars_order_set.insert(vname);
						_vars_order.push_back(vname);
					}

					update_vars_stats(vname, (dims != 0) ? VST::VST_ARRAY : VST::VST_SIMPLE, vtype);
				}
			}
			else
			{
				if(v->second.type != B1Types::B1T_UNKNOWN && v->second.type != vtype)
				{
					return C1_T_ERROR::C1_RES_EVARTYPMIS;
				}
				v->second.type = vtype;

				if(v->second.dim_num >= 0 && v->second.dim_num != dims / 2)
				{
					return C1_T_ERROR::C1_RES_EVARDIMMIS;
				}
				v->second.dim_num = dims / 2;

				if(v->second.type != B1Types::B1T_UNKNOWN && ((v->second.is_volatile != is_volatile) || (v->second.is_const != is_const)))
				{
					return C1_T_ERROR::C1_RES_EVARTYPMIS;
				}
				v->second.is_volatile = is_volatile;
				v->second.is_const = is_const;

				if(is_ma)
				{
					if(((v->second.use_symbol && v->second.symbol == vname) || is_const) && dims != 0)
					{
						update_vars_stats(vname, is_const ? VST::VST_CONST_ARRAY : VST::VST_STAT_ARRAY, vtype);
					}
				}
				else
				{
					update_vars_stats(vname, (dims != 0) ? VST::VST_ARRAY : VST::VST_SIMPLE, vtype);
				}
			}

			for(auto a = cmd.args.begin() + dims_off; a != cmd.args.end(); a++)
			{
				auto err = check_arg(*a);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}

				if(is_ma || check_sizes)
				{
					if(a->size() > 1)
					{
						if(is_ma)
						{
							return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
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
							return static_cast<C1_T_ERROR>(err);
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
				return static_cast<C1_T_ERROR>(B1_RES_EIDINUSE);
			}

			auto v = _vars.find(vname);
			if(v == _vars.end())
			{
				if(_vars_order_set.find(vname) == _vars_order_set.end())
				{
					_vars_order_set.insert(vname);
					_vars_order.push_back(vname);
				}
				_vars[vname] = B1_CMP_VAR(vname, B1Types::B1T_UNKNOWN, 0, false, false, _curr_src_file_id, _curr_line_cnt);
				update_vars_stats(vname, VST::VST_UNKNOWN, B1Types::B1T_UNKNOWN);
			}

			continue;
		}

		if(cmd.cmd == L"OUT")
		{
			auto err = check_arg(cmd.args[1]);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"IN")
		{
			auto err = check_arg(cmd.args[1]);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"GET" || cmd.cmd == L"PUT" || cmd.cmd == L"TRR")
		{
			auto err = check_arg(cmd.args[1]);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			if(cmd.args.size() != 2)
			{
				err = check_arg(cmd.args[2]);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}

			continue;
		}

		if(cmd.cmd == L"RETVAL")
		{
			auto err = check_arg(cmd.args[0]);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"READ")
		{
			auto err = check_arg(cmd.args[1]);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"SET")
		{
			auto err = check_arg(cmd.args[1]);
			if(err != C1_T_ERROR::C1_RES_OK)
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
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}

				if(!(iocmd.data_type == B1Types::B1T_LABEL || iocmd.data_type == B1Types::B1T_TEXT))
				{
					auto err = check_arg(cmd.args[2]);
					if (err != C1_T_ERROR::C1_RES_OK)
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
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
		}
	}

	// remove mem. references from variables list and const variables data from DAT statements init list
	for(auto &ma: _mem_areas)
	{
		_data_stmts_init.erase(ma.first);

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

		auto ar = arr_ranges.find(ma.first);
		if(ar != arr_ranges.cend())
		{
			if(ma.second.type != B1Types::B1T_BYTE || ma.second.dim_num != 1)
			{
				_curr_src_file_id = std::get<0>(ar->second);
				_curr_line_cnt = std::get<1>(ar->second);

				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			if(ma.second.is_const && !std::get<2>(ar->second))
			{
				_curr_src_file_id = std::get<0>(ar->second);
				_curr_line_cnt = std::get<1>(ar->second);

				return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			}

			arr_ranges.erase(ar);
		}
	}

	for(auto &var: _vars)
	{
		auto ea = exp_alloc.find(var.first);

		var.second.fixed_size = (ea == exp_alloc.cend());

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

		auto ar = arr_ranges.find(var.first);
		if(ar != arr_ranges.cend())
		{
			if(var.second.type != B1Types::B1T_BYTE || var.second.dim_num != 1)
			{
				_curr_src_file_id = std::get<0>(ar->second);
				_curr_line_cnt = std::get<1>(ar->second);

				return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
			}

			if(var.second.is_const && !std::get<2>(ar->second))
			{
				_curr_src_file_id = std::get<0>(ar->second);
				_curr_line_cnt = std::get<1>(ar->second);

				return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
			}

			arr_ranges.erase(ar);
		}
	}

	if(!arr_ranges.empty())
	{
		auto ar = arr_ranges.cbegin();
		_curr_src_file_id = std::get<0>(ar->second);
		_curr_line_cnt = std::get<1>(ar->second);

		return static_cast<C1_T_ERROR>(B1_RES_ETYPMISM);
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::process_imm_str_value(const B1_CMP_ARG &arg)
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

	return C1_T_ERROR::C1_RES_OK;
}

// build label list for all imm. string values (__STR_XXX labels)
C1_T_ERROR C1Compiler::process_imm_str_values()
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
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}
			continue;
		}

		if(cmd.cmd == L"OUT")
		{
			auto err = process_imm_str_value(cmd.args[1]);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			continue;
		}

		if(cmd.cmd == L"IN")
		{
			auto err = process_imm_str_value(cmd.args[1]);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"GET" || cmd.cmd == L"PUT" || cmd.cmd == L"TRR")
		{
			auto err = process_imm_str_value(cmd.args[1]);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			if(cmd.args.size() != 2)
			{
				err = process_imm_str_value(cmd.args[2]);
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}

			continue;
		}

		if(cmd.cmd == L"RETVAL")
		{
			auto err = process_imm_str_value(cmd.args[0]);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			continue;
		}

		if(cmd.cmd == L"READ")
		{
			auto err = process_imm_str_value(cmd.args[1]);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}
			continue;
		}

		if(cmd.cmd == L"SET")
		{
			auto err = process_imm_str_value(cmd.args[1]);
			if(err != C1_T_ERROR::C1_RES_OK)
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
					return static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
				}

				if(!iocmd.predef_only && !(iocmd.data_type == B1Types::B1T_LABEL || iocmd.data_type == B1Types::B1T_TEXT))
				{
					auto err = process_imm_str_value(cmd.args[2]);
					if(err != C1_T_ERROR::C1_RES_OK)
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
				if(err != C1_T_ERROR::C1_RES_OK)
				{
					return err;
				}
			}
			continue;
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

B1_ASM_OPS::const_iterator C1Compiler::create_asm_op(B1_ASM_OPS &sec, B1_ASM_OPS::const_iterator where, AOT type, const std::wstring &lbl, bool is_volatile, bool is_inline)
{
	return sec.emplace(where, new B1_ASM_OP(type, lbl, _comment, is_volatile, is_inline));
}

std::wstring C1Compiler::ROM_string_representation(int32_t str_len, const std::wstring &str) const
{
	return L"DB " + Utils::str_tohex32(str_len) + L", " + str;
}

B1_ASM_OPS::const_iterator C1Compiler::add_lbl(B1_ASM_OPS &sec, B1_ASM_OPS::const_iterator where, const std::wstring &lbl, bool is_volatile, bool is_inline /*= false*/)
{
	auto it = create_asm_op(sec, where, AOT::AOT_LABEL, lbl, is_volatile, is_inline);
	_comment.clear();
	return it;
}

B1_ASM_OPS::const_iterator C1Compiler::add_data(B1_ASM_OPS &sec, B1_ASM_OPS::const_iterator where, const std::wstring &data, bool is_volatile, bool is_inline /*= false*/)
{
	auto it = create_asm_op(sec, where, AOT::AOT_DATA, data, is_volatile, is_inline);
	_comment.clear();
	return it;
}

void C1Compiler::add_op(B1_ASM_OPS &sec, const std::wstring &op, bool is_volatile, bool is_inline /*= false*/)
{
	create_asm_op(sec, sec.cend(), AOT::AOT_OP, op, is_volatile, is_inline);
	_comment.clear();
}

C1_T_ERROR C1Compiler::add_data_def(const std::wstring &name, const std::wstring &asmtype, int32_t rep, bool is_volatile)
{
	add_lbl(_data_sec, _data_sec.cend(), name, is_volatile);
	add_data(_data_sec, _data_sec.cend(), asmtype + (rep == 1 ? std::wstring() : L" (" + std::to_wstring(rep) + L")"), is_volatile);
	_all_symbols.insert(name);
	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::write_data_sec()
{
	_comment.clear();

	for(const auto &vn: _vars_order)
	{
		bool is_static = false;
		int32_t size, rep;
		std::wstring type;

		auto v = _mem_areas.find(vn);
		if(v != _mem_areas.end())
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

		auto err = add_data_def(v->first, type, rep, v->second.is_volatile);
		if(err != C1_T_ERROR::C1_RES_OK)
		{
			return err;
		}

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
			std::wstring asmtype;

			// no __DAT_PTR variable for const variables data
			if(_mem_areas.find(label) != _mem_areas.cend())
			{
				continue;
			}

			label = label.empty() ? std::wstring() : (label + L"::");

			label = label + L"__DAT_PTR";
#ifdef B1_POINTER_SIZE_32_BIT
			B1_CMP_VAR var(label, B1Types::B1T_LONG, 0, false, false, -1, 0);
			B1CUtils::get_asm_type(B1Types::B1T_LONG, &asmtype, &var.size);
#else
			B1_CMP_VAR var(label, B1Types::B1T_WORD, 0, false, false, -1, 0);
			B1CUtils::get_asm_type(B1Types::B1T_WORD, &asmtype, &var.size);
#endif
			var.address = _data_size;
			_vars[label] = var;
			// no use of non-user variables in _vars_order
			//_vars_order.push_back(label);

			auto err = add_data_def(label, asmtype, 1, false);
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				return err;
			}

			_data_size += var.size;
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::write_const_sec()
{
	_comment.clear();
	_dat_rst_labels.clear();

	int32_t str_var_size;
	std::wstring str_var_asm_type;
	B1CUtils::get_asm_type(B1Types::B1T_STRING, &str_var_asm_type, &str_var_size);

	// DAT statements
	if(!_data_stmts.empty())
	{
		for(auto const &dt: _data_stmts)
		{
			bool dat_start = true;
			bool const_var_data = false;

			std::wstring name_space = dt.first;

			const auto &ma = _mem_areas.find(name_space);
			if(ma != _mem_areas.cend())
			{
				const_var_data = true;
			}
			else
			{
				name_space = name_space.empty() ? std::wstring() : (name_space + L"::");
			}

			int32_t values_num = 0;

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
					if(const_var_data)
					{
						add_lbl(_const_sec, _const_sec.cend(), name_space, false);
						_all_symbols.insert(name_space);
					}
					else
					{
						add_lbl(_const_sec, _const_sec.cend(), name_space + L"__DAT_START", false);
						_all_symbols.insert(name_space + L"__DAT_START");
					}
					dat_start = false;
				}

				if(!const_var_data)
				{
					std::wstring dat_label;
					iterator prev = i;
					while(prev != begin() && B1CUtils::is_label(*std::prev(prev)))
					{
						prev--;
						if(dat_label.empty())
						{
							dat_label = L"__DAT_" + std::to_wstring(_dat_rst_labels.size());
							add_lbl(_const_sec, _const_sec.cend(), dat_label, false);
							_all_symbols.insert(dat_label);
						}
						_dat_rst_labels[prev->cmd] = dat_label;
					}
				}

				bool skip_nmspc = true;

				if(_out_src_lines)
				{
					_comment = Utils::str_trim(_src_lines[i->src_line_id]);
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
						if(err != C1_T_ERROR::C1_RES_OK)
						{
							return err;
						}

						add_data(_const_sec, _const_sec.cend(), str_var_asm_type + L" " + std::get<0>(_str_labels[a[0].value]), false);
						_const_size += str_var_size;
					}
					else
					{
						std::wstring asmtype;
						int32_t size;

						// store bytes as words (for all types to be 2 bytes long, to simplify READ statement)
						if(!B1CUtils::get_asm_type((a[0].type == B1Types::B1T_BYTE && !const_var_data) ? B1Types::B1T_WORD : a[0].type, &asmtype, &size))
						{
							return C1_T_ERROR::C1_RES_EINVTYPNAME;
						}

						add_data(_const_sec, _const_sec.cend(), asmtype + L" " + a[0].value, false);
						_const_size += size;
					}

					values_num++;
				}
			}

			if(const_var_data)
			{
				int32_t arr_size = 1;

				for(int32_t i = 0; i < ma->second.dim_num; i++)
				{
					arr_size *= ma->second.dims[i * 2 + 1] - ma->second.dims[i * 2] + 1;
				}

				if(arr_size > values_num)
				{
					arr_size -= values_num;
					for(int32_t i = 0; i < arr_size; i++)
					{
						if(ma->second.type == B1Types::B1T_STRING)
						{
							auto err = process_imm_str_value(B1_CMP_ARG(L"\"\"", B1Types::B1T_STRING));
							if(err != C1_T_ERROR::C1_RES_OK)
							{
								return err;
							}

							add_data(_const_sec, _const_sec.cend(), str_var_asm_type + L" " + std::get<0>(_str_labels[L"\"\""]), false);
							_const_size += str_var_size;
						}
						else
						{
							std::wstring asmtype;
							int32_t size;

							if(!B1CUtils::get_asm_type(ma->second.type, &asmtype, &size))
							{
								return C1_T_ERROR::C1_RES_EINVTYPNAME;
							}

							add_data(_const_sec, _const_sec.cend(), asmtype + L" 0", false);
							_const_size += size;
						}
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
				return static_cast<C1_T_ERROR>(err);
			}

			if(sdata.length() > B1C_T_CONST::B1C_MAX_STR_LEN)
			{
				return static_cast<C1_T_ERROR>(B1_RES_ESTRLONG);
			}

			add_lbl(_const_sec, _const_sec.cend(), std::get<0>(sl.second), false);
			std::get<1>(sl.second) = true;
			_all_symbols.insert(std::get<0>(sl.second));

			add_data(_const_sec, _const_sec.cend(), ROM_string_representation(sdata.length(), sl.first), false);
			_const_size += sdata.length();
		}
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::save_section(const std::wstring &sec_name, const B1_ASM_OPS &sec, std::FILE *fp)
{
	if(!sec.empty())
	{
		std::fwprintf(fp, L"%ls\n", sec_name.c_str());

		for(const auto &op: sec)
		{
			if(!op->_comment.empty())
			{
				std::fwprintf(fp, L"; %ls\n", op->_comment.c_str());
			}

			switch(op->_type)
			{
				case AOT::AOT_LABEL:
					std::fwprintf(fp, L":%ls\n", op->_data.c_str());
					break;
				default:
					std::fwprintf(fp, L"%ls\n", op->_data.c_str());
					break;
			}
		}

		std::fwprintf(fp, L"\n");
	}

	return C1_T_ERROR::C1_RES_OK;
}


C1Compiler::C1Compiler(bool out_src_lines, bool opt_nocheck)
: B1_CMP_CMDS(L"", 32768, 32768)
, _data_size(0)
, _const_size(0)
, _out_src_lines(out_src_lines)
, _opt_nocheck(opt_nocheck)
, _inline_asm(false)
, _next_temp_namespace_id(32768)
, _curr_code_sec(nullptr)
{
	_call_stmt = L"CALL";
	_ret_stmt = L"RET";
}

C1Compiler::~C1Compiler()
{
}

C1_T_ERROR C1Compiler::Load(const std::vector<std::string> &file_names)
{
	C1_T_ERROR err = C1_T_ERROR::C1_RES_EIFEMPTY;

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

	_comment.clear();

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
			return C1_T_ERROR::C1_RES_EFOPEN;
		}

		std::wstring line;

		while(true)
		{
			err = static_cast<C1_T_ERROR>(Utils::read_line(ofp, line));
			if(err == static_cast<C1_T_ERROR>(B1_RES_EEOF) && line.empty() && _curr_line_cnt == 0)
			{
				_curr_line_cnt = 0;
				err = C1_T_ERROR::C1_RES_EIFEMPTY;
				break;
			}

			if(err == static_cast<C1_T_ERROR>(B1_RES_EEOF))
			{
				err = C1_T_ERROR::C1_RES_OK;
				if(line.empty())
				{
					break;
				}
			}

			if(err != C1_T_ERROR::C1_RES_OK)
			{
				break;
			}

			_curr_src_line_id++;

			_src_lines[_curr_src_line_id] = line;

			_curr_line_cnt++;

			err = load_next_command(line, cend());
			if(err != C1_T_ERROR::C1_RES_OK)
			{
				break;
			}
		}

		std::fclose(ofp);

		if(_inline_asm && err == C1_T_ERROR::C1_RES_OK)
		{
			err = static_cast<C1_T_ERROR>(B1_RES_ESYNTAX);
			break;
		}

		if(err != C1_T_ERROR::C1_RES_OK)
		{
			break;
		}
	}

	return err;
}

C1_T_ERROR C1Compiler::Compile()
{
	_curr_src_file_id = -1;
	_curr_line_cnt = 0;

	auto err = read_ufns();
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	err = read_and_check_locals();
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	err = read_and_check_vars();
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	err = process_imm_str_values();
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::WriteCode(bool code_init)
{
	_curr_code_sec = nullptr;

	auto err = write_data_sec();
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	err = write_const_sec();
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	_curr_code_sec = code_init ? &_code_init_sec : &_code_sec;

	err = write_code_sec(code_init);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::Save(const std::string &file_name, bool overwrite_existing /*= true*/)
{
	std::FILE *ofs = std::fopen(file_name.c_str(), overwrite_existing ? "w" : "a");
	if(ofs == nullptr)
	{
		return C1_T_ERROR::C1_RES_EFOPEN;
	}

	auto err = save_section(L".DATA", _data_sec, ofs);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

	int32_t ss = _global_settings.GetStackSize();
	int32_t hs = _global_settings.GetHeapSize();

	if(hs == 0)
	{
		// use all available RAM memory for heap
		std::fwprintf(ofs, L".HEAP\n\n");
	}
	else
	if(hs > 0)
	{
		std::fwprintf(ofs, L".HEAP\n");
		std::fwprintf(ofs, L"DB (0x%X)\n\n", (int)hs);
	}
	else
	{
		// emit warning or error if heap size is < 0
		_warnings.push_back(std::make_tuple(-1, "", C1_T_WARNING::C1_WRN_WWRNGHEAPSIZE));
	}

	// emit warning if stack size is zero
	if(ss > 0)
	{
		std::fwprintf(ofs, L".STACK\n");
		std::fwprintf(ofs, L"DB (0x%X)\n\n", (int)ss);
	}
	else
	{
		_warnings.push_back(std::make_tuple(-1, "", C1_T_WARNING::C1_WRN_WWRNGSTKSIZE));
	}

	err = save_section(L".CODE INIT", _code_init_sec, ofs);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

#ifndef B1_SECT_CONST_AFTER_CODE
	err = save_section(L".CONST", _const_sec, ofs);
	if(err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}
#endif

	err = save_section(L".CODE", _code_sec, ofs);
	if (err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}

#ifdef B1_SECT_CONST_AFTER_CODE
	err = save_section(L".CONST", _const_sec, ofs);
	if (err != C1_T_ERROR::C1_RES_OK)
	{
		return err;
	}
#endif

	std::fclose(ofs);

	return C1_T_ERROR::C1_RES_OK;
}

// init = false creates new record and sets its usage count to 0 (if the record does not exist)
void C1Compiler::update_opt_rule_usage_stat(int32_t rule_id, bool init /*= false*/) const
{
	auto rule = _opt_rules_usage_data.find(rule_id);
	if(rule == _opt_rules_usage_data.end())
	{
		_opt_rules_usage_data.emplace(std::make_pair((int)rule_id, std::make_tuple((int)(init ? 0 : 1))));
	}
	else
	if(!init)
	{
		rule->second = std::make_tuple(std::get<0>(rule->second) + 1);
	}
}

C1_T_ERROR C1Compiler::ReadOptLogFile(const std::string &file_name) const
{
	C1_T_ERROR err = C1_T_ERROR::C1_RES_OK;

	_opt_rules_usage_data.clear();

	if(!std::filesystem::exists(file_name))
	{
		std::FILE *fp = std::fopen(file_name.c_str(), "w");
		if(fp == nullptr)
		{
			return C1_T_ERROR::C1_RES_EFOPEN;
		}
		std::fclose(fp);

		return C1_T_ERROR::C1_RES_OK;
	}

	std::FILE *fp = std::fopen(file_name.c_str(), "r");
	if(fp == nullptr)
	{
		return C1_T_ERROR::C1_RES_EFOPEN;
	}

	std::wstring line;

	while(true)
	{
		err = static_cast<C1_T_ERROR>(Utils::read_line(fp, line));
		if(err == static_cast<C1_T_ERROR>(B1_RES_EEOF))
		{
			err = C1_T_ERROR::C1_RES_OK;
			if(line.empty())
			{
				break;
			}
		}

		if(err != C1_T_ERROR::C1_RES_OK)
		{
			break;
		}

		line = Utils::str_trim(line);
		if(line.empty())
		{
			continue;
		}

		std::vector<std::wstring> data;
		Utils::str_split(line, L",", data);
		if(data.size() != 2)
		{
			err = C1_T_ERROR::C1_RES_EWOPTLOGFMT;
			break;
		}

		int32_t rule_id = -1, usage_count = -1;
		if(Utils::str2int32(Utils::str_trim(data[0]), rule_id) != B1_RES_OK || Utils::str2int32(Utils::str_trim(data[1]), usage_count) != B1_RES_OK)
		{
			err = C1_T_ERROR::C1_RES_EWOPTLOGFMT;
			break;
		}

		_opt_rules_usage_data[(int)rule_id] = std::make_tuple((int)usage_count);
	}

	std::fclose(fp);

	return err;
}

C1_T_ERROR C1Compiler::WriteOptLogFile(const std::string &file_name) const
{
	std::FILE *fp = std::fopen(file_name.c_str(), "w");
	if(fp == nullptr)
	{
		return C1_T_ERROR::C1_RES_EFOPEN;
	}

	for(const auto &od: _opt_rules_usage_data)
	{
		std::fwprintf(fp, L"0x%X,%d\n", (unsigned int)od.first, (int)std::get<0>(od.second));
	}

	std::fclose(fp);

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::GetUndefinedSymbols(std::set<std::wstring> &symbols) const
{
	std::set_difference(_req_symbols.begin(), _req_symbols.end(), _all_symbols.begin(), _all_symbols.end(), std::inserter(symbols, symbols.end()));

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::GetResolvedSymbols(std::set<std::wstring> &symbols) const
{
	std::copy(_all_symbols.begin(), _all_symbols.end(), std::inserter(symbols, symbols.end()));

	return C1_T_ERROR::C1_RES_OK;
}

C1_T_ERROR C1Compiler::GetInitFiles(std::vector<std::wstring> &init_files) const
{
	std::copy(_init_files.begin(), _init_files.end(), std::back_inserter(init_files));

	return C1_T_ERROR::C1_RES_OK;
}
