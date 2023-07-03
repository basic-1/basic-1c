/*
 BASIC1 compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 b1cmp.cpp: BASIC1 compiler helper classes
*/


extern "C"
{
#include "b1.h"
#include "b1err.h"
#include "b1types.h"
}

#include <limits.h>
#include <cwctype>
#include <iterator>

#include "moresym.h"
#include "b1cmp.h"
#include "Utils.h"


// "!" stands for bitwise NOT
const std::wstring B1CUtils::_un_ops[3] = { L"=", L"-", L"!" };
// "~" stands for bitwise XOR
const std::wstring B1CUtils::_bin_ops[11] = { L"+", L"-", L"*", L"/", L"^", L"<<", L">>", L"%", L"&", L"|", L"~" };
const std::wstring B1CUtils::_log_ops[6] = { L"==", L"<>", L">", L"<", L">=", L"<=" };


std::wstring B1CUtils::b1str_to_cstr(const B1_T_CHAR *bstr, bool is_null_terminated /*= false*/)
{
	int i, len;
	B1_T_CHAR c;
	std::wstring s;

	len = is_null_terminated ? INT_MAX : (int)*bstr++;

	for(i = 0; i < len; i++)
	{
		c = *bstr++;

		if(c == B1_T_C_STRTERM)
		{
			break;
		}

		s += (wchar_t)c;
	}

	return s;
}

B1_T_CHAR *B1CUtils::cstr_to_b1str(const std::wstring &cstr, B1_T_CHAR *strbuf)
{
	B1_T_INDEX i, len;

	len = cstr.length();
	*strbuf = (B1_T_CHAR)len;

	for(i = 0; i < len; i++)
	{
		*(strbuf + i + 1) = (B1_T_CHAR)cstr[i];
	}

	return strbuf;
}

std::wstring B1CUtils::get_progline_substring(B1_T_INDEX begin, B1_T_INDEX end, bool double_bkslashes /*= false*/)
{
	std::wstring outstr;
	wchar_t c;

	while(begin < end)
	{
		c = (wchar_t)*(b1_progline + begin++);
		outstr += c;
		if(double_bkslashes && c == L'\\')
		{
			outstr += L'\\';
		}
	}

	return outstr;
}

B1_T_ERROR B1CUtils::get_string_data(const std::wstring &str, std::wstring &data, bool quoted_string /*= true*/)
{
	auto b = str.begin();
	auto e = str.end();
	bool ddq = false;
	bool open_quote = false;
	bool bksl = false;

	data.clear();

	if(b != e && *b == L'\"')
	{
		b++;
		if(b == e)
		{
			return B1_RES_ESYNTAX;
		}
		e--;
		if(*e != L'\"')
		{
			return B1_RES_ESYNTAX;
		}

		// doubled double-quotes
		ddq = true;
	}

	if(quoted_string && !ddq)
	{
		return B1_RES_ESYNTAX;
	}

	for(; b != e; b++)
	{
		auto c = *b;

		if(ddq)
		{
			if(c == L'\"')
			{
				if(open_quote)
				{
					open_quote = false;
					continue;
				}
				else
				{
					open_quote = true;
				}
			}
			else
			{
				if(open_quote)
				{
					return B1_RES_ESYNTAX;
				}
				else
				if(bksl)
				{
					bksl = false;

					if(c == L'0')
					{
						c = L'\0';
					}
					else
					if(c == L't')
					{
						c = L'\t';
					}
					else
					if(c == L'n')
					{
						c = L'\n';
					}
					else
					if(c == L'r')
					{
						c = L'\r';
					}
					else
					if(c == L'\\')
					{
						c = L'\\';
					}
					else
					{
						return B1_RES_ESYNTAX;
					}
				}
				else
				if(c == L'\\')
				{
					if(!bksl)
					{
						bksl = true;
						continue;
					}
				}
			}
		}

		data += c;
	}

	if(ddq && (open_quote || bksl))
	{
		return B1_RES_ESYNTAX;
	}

	return B1_RES_OK;
}

B1_T_ERROR B1CUtils::get_type_name(uint8_t type, std::wstring &tname)
{
	for(auto i = 0; i < B1_TYPE_COUNT; i++)
	{
		if(b1_t_types[i] == type)
		{
			tname = b1str_to_cstr(b1_t_type_names[i]);
			return B1_RES_OK;
		}
	}

	return B1_RES_ETYPMISM;
}

bool B1CUtils::is_num_val(const std::wstring &val)
{
	return (!val.empty()) && (B1_T_ISDIGIT((B1_T_CHAR)val.front()) || B1_T_ISMINUS((B1_T_CHAR)val.front()) || B1_T_ISPLUS((B1_T_CHAR)val.front()));
}

bool B1CUtils::is_str_val(const std::wstring &val)
{
	return (!val.empty()) && B1_T_ISDBLQUOTE((B1_T_CHAR)val.front());
}

bool B1CUtils::is_imm_val(const std::wstring &val)
{
	return is_num_val(val) || is_str_val(val);
}

B1_T_ERROR B1CUtils::get_num_min_type(const std::wstring &val, std::wstring &type, std::wstring &mod_val)
{
	long long ival;
	std::size_t pos = 0;

	try
	{
		ival = std::stoll(val, &pos, 0);
	}
	catch(std::exception &)
	{
		return B1_RES_EINVNUM;
	}

	if(pos == 0)
	{
		return B1_RES_EINVNUM;
	}

	type = L"INT";
	mod_val = val;

	// the only numeric data type specifier character at the moment
	if(val.back() == L'%')
	{
		mod_val.pop_back();
		return B1_RES_OK;
	}

	if(ival >= 0 && ival <= 255)
	{
		type = L"BYTE";
	}
	else
	if(ival >= -32768 && ival <= 32767)
	{
		type = L"INT";
	}
	else
	if(ival >= 0 && ival <= 65535)
	{
		type = L"WORD";
	}

	return B1_RES_OK;
}

// comp_num_types - compatible numeric types (the same size, no need to convert)
B1_T_ERROR B1CUtils::get_com_type(const std::wstring &type0, const std::wstring &type1, std::wstring &com_type, bool &comp_num_types)
{
	comp_num_types = false;

	if(type0.empty() || type1.empty())
	{
		return B1_RES_ETYPMISM;
	}

	if(type0 == L"STRING" || type1 == L"STRING")
	{
		com_type = L"STRING";
	}
	else
	if(type0 == L"INT" || type1 == L"INT")
	{
		comp_num_types = (type0 != L"BYTE" && type1 != L"BYTE");
		com_type = L"INT";
	}
	else
	if(type0 == L"WORD" || type1 == L"WORD")
	{
		comp_num_types = (type0 != L"BYTE" && type1 != L"BYTE");
		com_type = L"WORD";
	}
	else
	{
		comp_num_types = true;
		com_type = L"BYTE";
	}

	return B1_RES_OK;
}

// checks if a value of src type can be assigned to a variable of dst type
bool B1CUtils::are_types_compatible(const std::wstring &src_type, const std::wstring &dst_type)
{
	return (dst_type == L"STRING") || (src_type != L"STRING");
}

bool B1CUtils::is_label(const B1_CMP_CMD &cmd)
{
	return (cmd.type == B1_CMD_TYPE::B1_CMD_TYPE_LABEL);
}

bool B1CUtils::is_cmd(const B1_CMP_CMD &cmd)
{
	return (cmd.type == B1_CMD_TYPE::B1_CMD_TYPE_COMMAND);
}

bool B1CUtils::is_inline_asm(const B1_CMP_CMD &cmd)
{
	return (cmd.type == B1_CMD_TYPE::B1_CMD_TYPE_INLINE_ASM);
}

bool B1CUtils::is_def_fn(const std::wstring &name)
{
	return name.find(L"__DEF_") == 0 || name.find(L"::__DEF_") != std::wstring::npos;
}

bool B1CUtils::is_def_fn(const B1_CMP_CMD &cmd)
{
	return (cmd.type == B1_CMD_TYPE::B1_CMD_TYPE_LABEL && is_def_fn(cmd.cmd));
}

bool B1CUtils::is_fn_arg(const std::wstring &name)
{
	return name.find(L"__ARG_") == 0;
}

bool B1CUtils::is_local(const std::wstring &name)
{
	return name.find(L"__LCL_") == 0 || name.find(L"::__LCL_") != std::wstring::npos;
}

int B1CUtils::get_fn_arg_index(const std::wstring &name)
{
	return (name.find(L"__ARG_") == 0) ? std::stoi(name.substr(std::wstring(L"__ARG_").length())) : -1;
}

bool B1CUtils::is_src(const B1_CMP_CMD &cmd, const std::wstring &val)
{
	if(B1CUtils::is_label(cmd))
	{
		return false;
	}

	if(B1CUtils::is_inline_asm(cmd))
	{
		return true;
	}

	if(cmd.cmd == L"GA" || cmd.cmd == L"MA")
	{

		for(auto a = cmd.args.begin() + (cmd.cmd == L"GA" ? 2 : 3); a != cmd.args.end(); a++)
		{
			if((*a)[0].value == val)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"RETVAL")
	{
		if(cmd.args[0][0].value == val)
		{
			return true;
		}

		return false;
	}

	if(cmd.cmd == L"OUT")
	{
		if(cmd.args[1][0].value == val)
		{
			return true;
		}

		return false;
	}

	if(cmd.cmd == L"SET")
	{
		if(cmd.args[1][0].value == val)
		{
			return true;
		}

		return false;
	}

	if(cmd.cmd == L"IOCTL")
	{
		if(cmd.args.size() > 2 && cmd.args[2][0].value == val)
		{
			return true;
		}

		return false;
	}

	if(cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_un_ops)
		{
			if(cmd.cmd == op)
			{
				return cmd.args[0][0].value == val;
			}
		}

		for(auto &op: B1CUtils::_log_ops)
		{
			if(cmd.cmd == op)
			{
				return (cmd.args[0][0].value == val || cmd.args[1][0].value == val);
			}
		}
	}

	if(cmd.args.size() == 3)
	{
		for(auto &op: B1CUtils::_bin_ops)
		{
			if(cmd.cmd == op)
			{
				return (cmd.args[0][0].value == val || cmd.args[1][0].value == val);
			}
		}
	}

	return false;
}

bool B1CUtils::is_dst(const B1_CMP_CMD &cmd, const std::wstring &val)
{
	if(B1CUtils::is_label(cmd))
	{
		return false;
	}

	if(B1CUtils::is_inline_asm(cmd))
	{
		return true;
	}

	if(cmd.cmd == L"READ")
	{
		if(cmd.args[1][0].value == val)
		{
			return true;
		}

		return false;
	}

	if(cmd.cmd == L"IN")
	{
		if(cmd.args[1][0].value == val)
		{
			return true;
		}

		return false;
	}

	if(cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_un_ops)
		{
			if(cmd.cmd == op && cmd.args[1][0].value == val)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.args.size() == 3)
	{
		for(auto &op: B1CUtils::_bin_ops)
		{
			if(cmd.cmd == op && cmd.args[2][0].value == val)
			{
				return true;
			}
		}

		return false;
	}

	return false;
}

// check if the variable is array subscript or function call argument
bool B1CUtils::is_sub_or_arg(const B1_CMP_CMD &cmd, const std::wstring &val)
{
	if(B1CUtils::is_label(cmd))
	{
		return false;
	}

	if(B1CUtils::is_inline_asm(cmd))
	{
		return true;
	}

	if(cmd.cmd == L"GA" || cmd.cmd == L"MA")
	{
		for(auto a = cmd.args.begin() + (cmd.cmd == L"GA" ? 2 : 3); a != cmd.args.end(); a++)
		{
			for(auto s = a->begin() + 1; s != a->end(); s++)
			{
				if(s->value == val)
				{
					return true;
				}
			}
		}

		return false;
	}

	if(cmd.cmd == L"RETVAL")
	{
		for(auto s = cmd.args[0].begin() + 1; s != cmd.args[0].end(); s++)
		{
			if(s->value == val)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"IN")
	{
		for(auto s = cmd.args[1].begin() + 1; s != cmd.args[1].end(); s++)
		{
			if(s->value == val)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"OUT")
	{
		for(auto s = cmd.args[1].begin() + 1; s != cmd.args[1].end(); s++)
		{
			if(s->value == val)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"SET")
	{
		for(auto s = cmd.args[1].begin() + 1; s != cmd.args[1].end(); s++)
		{
			if(s->value == val)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"IOCTL")
	{
		if(cmd.args.size() > 2)
		{
			for(auto s = cmd.args[2].begin() + 1; s != cmd.args[2].end(); s++)
			{
				if(s->value == val)
				{
					return true;
				}
			}
		}

		return false;
	}

	if(cmd.cmd == L"READ")
	{
		for(auto s = cmd.args[1].begin() + 1; s != cmd.args[1].end(); s++)
		{
			if(s->value == val)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_un_ops)
		{
			if(cmd.cmd == op)
			{
				for(auto a = cmd.args.begin(); a != cmd.args.end(); a++)
				{
					for(auto s = a->begin() + 1; s != a->end(); s++)
					{
						if(s->value == val)
						{
							return true;
						}
					}
				}

				return false;
			}
		}

		for(auto &op: B1CUtils::_log_ops)
		{
			if(cmd.cmd == op)
			{
				for(auto a = cmd.args.begin(); a != cmd.args.end(); a++)
				{
					for(auto s = a->begin() + 1; s != a->end(); s++)
					{
						if(s->value == val)
						{
							return true;
						}
					}
				}

				return false;
			}
		}
	}

	if(cmd.args.size() == 3)
	{
		for(auto &op: B1CUtils::_bin_ops)
		{
			if(cmd.cmd == op)
			{
				for(auto a = cmd.args.begin(); a != cmd.args.end(); a++)
				{
					for(auto s = a->begin() + 1; s != a->end(); s++)
					{
						if(s->value == val)
						{
							return true;
						}
					}
				}

				return false;
			}
		}
	}

	return false;
}

bool B1CUtils::is_used(const B1_CMP_CMD &cmd, const std::wstring &val)
{
	if(B1CUtils::is_label(cmd))
	{
		return false;
	}

	if(B1CUtils::is_inline_asm(cmd))
	{
		return true;
	}

	if(cmd.cmd == L"GA" || cmd.cmd == L"MA")
	{
		for(auto a = cmd.args.begin() + (cmd.cmd == L"GA" ? 2 : 3); a != cmd.args.end(); a++)
		{
			for(const auto &aa: *a)
			{
				if(aa.value == val)
				{
					return true;
				}
			}
		}

		return false;
	}

	if(cmd.cmd == L"RETVAL")
	{
		for(const auto &aa: cmd.args[0])
		{
			if(aa.value == val)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"IN")
	{
		for(const auto &aa: cmd.args[1])
		{
			if(aa.value == val)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"OUT")
	{
		for(const auto &aa: cmd.args[1])
		{
			if(aa.value == val)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"SET")
	{
		for(const auto &aa: cmd.args[1])
		{
			if(aa.value == val)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"IOCTL")
	{
		if(cmd.args.size() > 2)
		{
			for(const auto &aa: cmd.args[2])
			{
				if(aa.value == val)
				{
					return true;
				}
			}
		}

		return false;
	}

	if(cmd.cmd == L"READ")
	{
		for(const auto &aa: cmd.args[1])
		{
			if(aa.value == val)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_un_ops)
		{
			if(cmd.cmd == op)
			{
				for(const auto &a: cmd.args)
				{
					for(const auto &aa: a)
					{
						if(aa.value == val)
						{
							return true;
						}
					}
				}

				return false;
			}
		}

		for(auto &op: B1CUtils::_log_ops)
		{
			if(cmd.cmd == op)
			{
				for(const auto &a: cmd.args)
				{
					for(const auto &aa: a)
					{
						if(aa.value == val)
						{
							return true;
						}
					}
				}

				return false;
			}
		}
	}

	if(cmd.args.size() == 3)
	{
		for(auto &op: B1CUtils::_bin_ops)
		{
			if(cmd.cmd == op)
			{
				for(const auto &a: cmd.args)
				{
					for(const auto &aa: a)
					{
						if(aa.value == val)
						{
							return true;
						}
					}
				}

				return false;
			}
		}
	}

	return false;
}

bool B1CUtils::replace_dst(B1_CMP_CMD &cmd, const std::wstring &val, const B1_CMP_ARG &arg, bool preserve_type /*= false*/)
{
	if(B1CUtils::is_label(cmd))
	{
		return false;
	}

	if(B1CUtils::is_inline_asm(cmd))
	{
		return false;
	}

	B1_CMP_ARG *to_replace = nullptr;

	if(cmd.cmd == L"READ")
	{
		if(cmd.args[1][0].value == val)
		{
			to_replace = &cmd.args[1];
		}
	}

	if(cmd.cmd == L"IN")
	{
		if(cmd.args[1][0].value == val)
		{
			to_replace = &cmd.args[1];
		}
	}

	if(to_replace == nullptr && cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_un_ops)
		{
			if(cmd.cmd == op && cmd.args[1][0].value == val)
			{
				to_replace = &cmd.args[1];
				break;
			}
		}
	}

	if(to_replace == nullptr && cmd.args.size() == 3)
	{
		for(auto &op: B1CUtils::_bin_ops)
		{
			if(cmd.cmd == op && cmd.args[2][0].value == val)
			{
				to_replace = &cmd.args[2];
				break;
			}
		}
	}

	if(to_replace == nullptr)
	{
		return false;
	}

	if(preserve_type)
	{
		auto type = (*to_replace)[0].type;
		*to_replace = arg;
		(*to_replace)[0].type = type;
	}
	else
	{
		*to_replace = arg;
	}

	return true;
}

bool B1CUtils::replace_src(B1_CMP_CMD &cmd, const std::wstring &val, const B1_CMP_ARG &arg)
{
	bool replaced = false;

	if(cmd.cmd == L"GA" || cmd.cmd == L"MA")
	{
		for(auto a = cmd.args.begin() + (cmd.cmd == L"GA" ? 2 : 3); a != cmd.args.end(); a++)
		{
			if((*a)[0].value == val)
			{
				*a = arg;
				replaced = true;
			}
		}

		return replaced;
	}

	if(cmd.cmd == L"RETVAL")
	{
		if(cmd.args[0][0].value == val)
		{
			cmd.args[0] = arg;
			return true;
		}

		return false;
	}

	if(cmd.cmd == L"OUT")
	{
		if(cmd.args[1][0].value == val)
		{
			cmd.args[1] = arg;
			return true;
		}

		return false;
	}

	if(cmd.cmd == L"SET")
	{
		if(cmd.args[1][0].value == val)
		{
			cmd.args[1] = arg;
			return true;
		}

		return false;
	}

	if(cmd.cmd == L"IOCTL")
	{
		if(cmd.args.size() > 2 && cmd.args[2][0].value == val)
		{
			cmd.args[2] = arg;
			return true;
		}

		return false;
	}

	if(cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_un_ops)
		{
			if(cmd.cmd == op)
			{
				if(cmd.args[0][0].value == val)
				{
					cmd.args[0] = arg;
					return true;
				}

				return false;
			}
		}

		for(auto &op: B1CUtils::_log_ops)
		{
			if(cmd.cmd == op)
			{
				for(auto a = cmd.args.begin(); a != cmd.args.end(); a++)
				{
					if((*a)[0].value == val)
					{
						*a = arg;
						replaced = true;
					}
				}

				return replaced;
			}
		}
	}

	if(cmd.args.size() == 3)
	{
		for(auto &op: B1CUtils::_bin_ops)
		{
			if(cmd.cmd == op)
			{
				for(auto a = cmd.args.begin(); a != cmd.args.end() - 1; a++)
				{
					if((*a)[0].value == val)
					{
						*a = arg;
						replaced = true;
					}
				}

				return replaced;
			}
		}
	}

	return replaced;
}

bool B1CUtils::replace_src(B1_CMP_CMD &cmd, const B1_CMP_ARG &src_arg, const B1_CMP_ARG &arg)
{
	bool replaced = false;

	if(cmd.cmd == L"GA" || cmd.cmd == L"MA")
	{
		for(auto a = cmd.args.begin() + (cmd.cmd == L"GA" ? 2 : 3); a != cmd.args.end(); a++)
		{
			if(*a == src_arg)
			{
				*a = arg;
				replaced = true;
			}
		}

		return replaced;
	}

	if(cmd.cmd == L"RETVAL")
	{
		if(cmd.args[0] == src_arg)
		{
			cmd.args[0] = arg;
			replaced = true;
		}

		return replaced;
	}

	if(cmd.cmd == L"OUT")
	{
		if(cmd.args[1] == src_arg)
		{
			cmd.args[1] = arg;
			replaced = true;
		}

		return replaced;
	}

	if(cmd.cmd == L"SET")
	{
		if(cmd.args[1] == src_arg)
		{
			cmd.args[1] = arg;
			replaced = true;
		}

		return replaced;
	}

	if(cmd.cmd == L"IOCTL")
	{
		if(cmd.args.size() > 2)
		{
			if(cmd.args[2] == src_arg)
			{
				cmd.args[2] = arg;
				replaced = true;
			}
		}

		return replaced;
	}

	if(cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_un_ops)
		{
			if(cmd.cmd == op)
			{
				if(cmd.args[0] == src_arg)
				{
					cmd.args[0] = arg;
					return true;
				}

				return false;
			}
		}

		for(auto &op: B1CUtils::_log_ops)
		{
			if(cmd.cmd == op)
			{
				for(auto a = cmd.args.begin(); a != cmd.args.end(); a++)
				{
					if(*a == src_arg)
					{
						*a = arg;
						replaced = true;
					}
				}

				return replaced;
			}
		}
	}

	if(cmd.args.size() == 3)
	{
		for(auto &op: B1CUtils::_bin_ops)
		{
			if(cmd.cmd == op)
			{
				for(auto a = cmd.args.begin(); a != cmd.args.end() - 1; a++)
				{
					if(*a == src_arg)
					{
						*a = arg;
						replaced = true;
					}
				}

				return replaced;
			}
		}
	}

	return replaced;
}

// replaces source variable in cmd command (including subscripts and function arguments)
bool B1CUtils::replace_src_with_subs(B1_CMP_CMD &cmd, const std::wstring &val, const B1_TYPED_VALUE &tv, bool preserve_type /*= false*/)
{
	if(B1CUtils::is_label(cmd))
	{
		return false;
	}

	if(B1CUtils::is_inline_asm(cmd))
	{
		return false;
	}

	bool processed = false;
	std::vector<B1_TYPED_VALUE *> to_replace;

	if(cmd.cmd == L"GA" || cmd.cmd == L"MA")
	{
		for(auto a = cmd.args.begin() + (cmd.cmd == L"GA" ? 2 : 3); a != cmd.args.end(); a++)
		{
			for(auto aa = a->begin(); aa != a->end(); aa++)
			{
				if(aa->value == val)
				{
					to_replace.push_back(&*aa);
				}
			}
		}

		processed = true;
	}

	if(!processed && cmd.cmd == L"RETVAL")
	{
		for(auto aa = cmd.args[0].begin(); aa != cmd.args[0].end(); aa++)
		{
			if(aa->value == val)
			{
				to_replace.push_back(&*aa);
			}
		}

		processed = true;
	}

	if(!processed && cmd.cmd == L"IN")
	{
		for(auto aa = cmd.args[1].begin() + 1; aa != cmd.args[1].end(); aa++)
		{
			if(aa->value == val)
			{
				to_replace.push_back(&*aa);
			}
		}

		processed = true;
	}

	if(!processed && cmd.cmd == L"OUT")
	{
		for(auto aa = cmd.args[1].begin(); aa != cmd.args[1].end(); aa++)
		{
			if(aa->value == val)
			{
				to_replace.push_back(&*aa);
			}
		}

		processed = true;
	}

	if(!processed && cmd.cmd == L"SET")
	{
		for(auto aa = cmd.args[1].begin(); aa != cmd.args[1].end(); aa++)
		{
			if(aa->value == val)
			{
				to_replace.push_back(&*aa);
			}
		}

		processed = true;
	}

	if(!processed && cmd.cmd == L"IOCTL")
	{
		if(cmd.args.size() > 2)
		{
			for(auto aa = cmd.args[2].begin(); aa != cmd.args[2].end(); aa++)
			{
				if(aa->value == val)
				{
					to_replace.push_back(&*aa);
				}
			}
		}

		processed = true;
	}

	if(!processed && cmd.cmd == L"READ")
	{
		for(auto aa = cmd.args[1].begin() + 1; aa != cmd.args[1].end(); aa++)
		{
			if(aa->value == val)
			{
				to_replace.push_back(&*aa);
			}
		}

		processed = true;
	}

	if(!processed && cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_un_ops)
		{
			if(cmd.cmd == op)
			{
				for(auto a = cmd.args.begin(); a != cmd.args.end(); a++)
				{
					for(auto aa = a->begin(); aa != a->end(); aa++)
					{
						if(a == cmd.args.begin() + 1 && aa == a->begin())
						{
							// not a src var
							continue;
						}

						if(aa->value == val)
						{
							to_replace.push_back(&*aa);
						}
					}
				}

				processed = true;
				break;
			}
		}
	}

	if(!processed && cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_log_ops)
		{
			if(cmd.cmd == op)
			{
				for(auto a = cmd.args.begin(); a != cmd.args.end(); a++)
				{
					for(auto aa = a->begin(); aa != a->end(); aa++)
					{
						if(aa->value == val)
						{
							to_replace.push_back(&*aa);
						}
					}
				}

				processed = true;
				break;
			}
		}
	}

	if(!processed && cmd.args.size() == 3)
	{
		for(auto &op: B1CUtils::_bin_ops)
		{
			if(cmd.cmd == op)
			{
				for(auto a = cmd.args.begin(); a != cmd.args.end(); a++)
				{
					for(auto aa = a->begin(); aa != a->end(); aa++)
					{
						if(a == cmd.args.begin() + 2 && aa == a->begin())
						{
							// not a src var
							continue;
						}

						if(aa->value == val)
						{
							to_replace.push_back(&*aa);
						}
					}
				}

				processed = true;
				break;
			}
		}
	}

	for(auto rep: to_replace)
	{
		if(preserve_type)
		{
			auto type = rep->type;
			*rep = tv;
			rep->type = type;
		}
		else
		{
			*rep = tv;
		}
	}

	return !to_replace.empty();
}

bool B1CUtils::replace_all(B1_CMP_CMD &cmd, const std::wstring &val, const B1_TYPED_VALUE &tv, bool preserve_type /*= false*/)
{
	B1_CMP_ARG arg(tv.value, tv.type);

	bool dst = replace_dst(cmd, val, arg, preserve_type);
	bool src = replace_src_with_subs(cmd, val, tv, preserve_type);
	return dst || src;
}

bool B1CUtils::arg_is_src(const B1_CMP_CMD &cmd, const B1_CMP_ARG &arg)
{
	if(arg.size() == 1)
	{
		return B1CUtils::is_src(cmd, arg[0].value) || B1CUtils::is_sub_or_arg(cmd, arg[0].value);
	}

	if(B1CUtils::is_label(cmd))
	{
		return false;
	}

	if(B1CUtils::is_inline_asm(cmd))
	{
		return true;
	}

	if(cmd.cmd == L"GA" || cmd.cmd == L"MA")
	{

		for(auto a = cmd.args.begin() + (cmd.cmd == L"GA" ? 2 : 3); a != cmd.args.end(); a++)
		{
			if(*a == arg)
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"RETVAL")
	{
		if(cmd.args[0] == arg)
		{
			return true;
		}

		return false;
	}

	if(cmd.cmd == L"OUT")
	{
		if(cmd.args[1] == arg)
		{
			return true;
		}

		return false;
	}

	if(cmd.cmd == L"SET")
	{
		if(cmd.args[1] == arg)
		{
			return true;
		}

		return false;
	}

	if(cmd.cmd == L"IOCTL")
	{
		if(cmd.args.size() > 2 && cmd.args[2] == arg)
		{
				return true;
		}

		return false;
	}

	if(cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_un_ops)
		{
			if(cmd.cmd == op && cmd.args[0] == arg)
			{
				return true;
			}
		}

		for(auto &op: B1CUtils::_log_ops)
		{
			if(cmd.cmd == op && (cmd.args[0] == arg || cmd.args[1] == arg))
			{
				return true;
			}
		}
	}

	if(cmd.args.size() == 3)
	{
		for(auto &op: B1CUtils::_bin_ops)
		{
			if(cmd.cmd == op && (cmd.args[0] == arg || cmd.args[1] == arg))
			{
				return true;
			}
		}
	}

	return false;
}

bool B1CUtils::arg_is_dst(const B1_CMP_CMD &cmd, const B1_CMP_ARG &arg)
{
	if(B1CUtils::is_label(cmd))
	{
		return false;
	}

	if(B1CUtils::is_inline_asm(cmd))
	{
		return true;
	}

	if(cmd.cmd == L"READ")
	{
		if(cmd.args[1] == arg)
		{
			return true;
		}

		return false;
	}

	if(cmd.cmd == L"IN")
	{
		if(cmd.args[1] == arg)
		{
			return true;
		}

		return false;
	}

	if(cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_un_ops)
		{
			if(cmd.cmd == op && cmd.args[1] == arg)
			{
				return true;
			}
		}
	}

	if(cmd.args.size() == 3)
	{
		for(auto &op: B1CUtils::_bin_ops)
		{
			if(cmd.cmd == op && cmd.args[2] == arg)
			{
				return true;
			}
		}
	}

	return false;
}

std::wstring B1CUtils::get_dst_var_name(const B1_CMP_CMD &cmd)
{
	if(B1CUtils::is_label(cmd))
	{
		return L"";
	}

	if(B1CUtils::is_inline_asm(cmd))
	{
		return L"";
	}

	if(cmd.cmd == L"READ")
	{
		return cmd.args[1][0].value;
	}

	if(cmd.cmd == L"IN")
	{
		return cmd.args[1][0].value;
	}

	if(is_un_op(cmd))
	{
		return cmd.args[1][0].value;
	}

	if(is_bin_op(cmd))
	{
		return cmd.args[2][0].value;
	}

	return L"";
}

// checks local variable types compatibility, returns true if a local of base_type can be used instead of a local of reuse_type
bool B1CUtils::local_compat_types(const std::wstring &base_type, const std::wstring &reuse_type)
{
	if((base_type == reuse_type) || (base_type == L"INT" && reuse_type == L"WORD") || (base_type == L"WORD" && reuse_type == L"INT") || (base_type != L"STRING" && reuse_type == L"BYTE"))
	{
		return true;
	}

	return false;
}

bool B1CUtils::is_log_op(const std::wstring &cmd)
{
	for(auto op: _log_ops)
	{
		if(cmd == op)
		{
			return true;
		}
	}

	return false;
}

bool B1CUtils::is_un_op(const std::wstring &cmd)
{
	for(auto op: _un_ops)
	{
		if(cmd == op)
		{
			return true;
		}
	}

	return false;
}

bool B1CUtils::is_bin_op(const std::wstring &cmd)
{
	for(auto op: _bin_ops)
	{
		if(cmd == op)
		{
			return true;
		}
	}

	return false;
}

bool B1CUtils::is_log_op(const B1_CMP_CMD &cmd)
{
	if(cmd.args.size() != 2)
	{
		return false;
	}

	return is_log_op(cmd.cmd);
}

bool B1CUtils::is_un_op(const B1_CMP_CMD &cmd)
{
	if(cmd.args.size() != 2)
	{
		return false;
	}

	return is_un_op(cmd.cmd);
}

bool B1CUtils::is_bin_op(const B1_CMP_CMD &cmd)
{
	if(cmd.args.size() != 3)
	{
		return false;
	}

	return is_bin_op(cmd.cmd);
}

bool B1CUtils::get_asm_type(const std::wstring &type, std::wstring *asmtype /*= nullptr*/, int32_t *size /*= nullptr*/, int32_t *rep /*= nullptr*/, int32_t dimnum /*= 0*/)
{
	int32_t s, r;
	std::wstring at;

	if(dimnum < 0)
	{
		return false;
	}
	else
	if(dimnum == 0)
	{
		if(type == L"STRING")
		{
			// string is represented as 2-byte pointer
			s = 2;
			at = L"DW";
			r = 1;
		}
		else
		if(type == L"INT")
		{
			s = 2;
			at = L"DW";
			r = 1;
		}
		else
		if(type == L"WORD")
		{
			s = 2;
			at = L"DW";
			r = 1;
		}
		else
		if(type == L"BYTE")
		{
			s = 1;
			at = L"DB";
			r = 1;
		}
		else
		{
			return false;
		}
	}
	else
	{
		// array header:
		// DW ; 2-byte array address
		// DW ; 1st dimension lbound
		// DW ; 1st dimension size
		// ...
		// DW ; Nth dimension lbound
		// DW ; Nth dimension size
		s = (1 + dimnum * 2) * 2;
		at = L"DW";
		r = 1 + dimnum * 2;
	}

	if(size != nullptr)
	{
		*size = s;
	}

	if(asmtype != nullptr)
	{
		*asmtype = at;
	}

	if(rep != nullptr)
	{
		*rep = r;
	}

	return true;
}

// checks if the numeric value can be the minimal or maximal integer value (-32768, 0, 255, 32767, 65535)
bool B1CUtils::is_num_min_max(const std::wstring &val)
{
	int32_t n;

	if(!is_num_val(val))
	{
		return true;
	}

	auto err = Utils::str2int32(val, n);
	if(err != B1_RES_OK)
	{
		return true;
	}

	return (n <= -32768 || n == 0 || n == 255 || n == 32767 || n >= 65535);
}


B1_TYPED_VALUE::B1_TYPED_VALUE()
{
}

B1_TYPED_VALUE::B1_TYPED_VALUE(const std::wstring &val, const std::wstring &tp /*= std::wstring()*/)
{
	value = val;
	type = tp;
}

bool B1_TYPED_VALUE::operator!=(const B1_TYPED_VALUE &tv) const
{
	return (type != tv.type || value != tv.value);
}

void B1_TYPED_VALUE::clear()
{
	type.clear();
	value.clear();
}


B1_CMP_ARG::B1_CMP_ARG()
{
}

B1_CMP_ARG::B1_CMP_ARG(const std::wstring &val, const std::wstring &tp /*= std::wstring()*/)
{
	push_back(B1_TYPED_VALUE(val, tp));
}

bool B1_CMP_ARG::operator==(const B1_CMP_ARG &arg) const
{
	if(size() != arg.size())
	{
		return false;
	}

	for(int i = 0; i < size(); i++)
	{
		if(at(i) != arg[i])
		{
			return false;
		}
	}

	return true;
}


B1_CMP_CMD::B1_CMP_CMD(int32_t line_num, int32_t line_cnt, int32_t src_file_id, int32_t src_line_id)
{
	clear();

	this->line_num = line_num;
	this->line_cnt = line_cnt;
	this->src_file_id = src_file_id;
	this->src_line_id = src_line_id;
}

void B1_CMP_CMD::clear()
{
	type = B1_CMD_TYPE::B1_CMD_TYPE_UNKNOWN;
	cmd.clear();
	args.clear();

	line_num = -1;
	line_cnt = -1;
	src_file_id = -1;
	src_line_id = -1;
}


B1_CMP_CMDS::B1_CMP_CMDS()
: _curr_name_space(std::wstring())
, _next_label(0)
, _next_local(0)
, _curr_line_num(-1)
, _curr_line_cnt(-1)
, _curr_src_file_id(-1)
, _curr_src_line_id(-1)
{
}

B1_CMP_CMDS::B1_CMP_CMDS(const std::wstring &name_space, int32_t next_label /*= 0*/, int32_t next_local /*= 0*/)
: _curr_name_space(name_space)
, _next_label(next_label)
, _next_local(next_local)
, _curr_line_num(-1)
, _curr_line_cnt(-1)
, _curr_src_file_id(-1)
, _curr_src_line_id(-1)
{
}

std::wstring B1_CMP_CMDS::get_name_space_prefix() const
{
	return _curr_name_space.empty() ? std::wstring() : (_curr_name_space + L"::");
}

// if global == true the function does not add namespace to the label name
void B1_CMP_CMDS::emit_label(const std::wstring &name, const_iterator pos, bool global /*= false*/)
{
	B1_CMP_CMD cmd(_curr_line_num, _curr_line_cnt, _curr_src_file_id, _curr_src_line_id);

	cmd.type = B1_CMD_TYPE::B1_CMD_TYPE_LABEL;

	if(global)
	{
		cmd.cmd = name;
	}
	else
	{
		cmd.cmd = (name.find(get_name_space_prefix()) == 0 ? std::wstring() : get_name_space_prefix()) + name;
	}

	insert(pos, cmd);
}

void B1_CMP_CMDS::emit_label(const std::wstring &name, bool global /*= false*/)
{
	emit_label(name, cend(), global);
}

std::wstring B1_CMP_CMDS::emit_label(const_iterator pos, bool gen_name_only /*= false*/)
{
	std::wstring name = get_name_space_prefix() + L"__ALB_" + std::to_wstring(_next_label++);

	if(!gen_name_only)
	{
		emit_label(name, pos);
	}

	return name;
}

std::wstring B1_CMP_CMDS::emit_label(bool gen_name_only /*= false*/)
{
	return emit_label(cend(), gen_name_only);
}

std::wstring B1_CMP_CMDS::emit_local(const std::wstring &type, const_iterator pos)
{
	std::wstring name = get_name_space_prefix() + L"__LCL_" + std::to_wstring(_next_local++);
	B1_CMP_CMD cmd(_curr_line_num, _curr_line_cnt, _curr_src_file_id, _curr_src_line_id);

	cmd.type = B1_CMD_TYPE::B1_CMD_TYPE_COMMAND;
	cmd.cmd = L"LA";
	cmd.args.push_back(B1_CMP_ARG(name, L""));
	if(!type.empty())
	{
		cmd.args.push_back(B1_CMP_ARG(type, L""));
	}

	insert(pos, cmd);

	return name;
}

std::wstring B1_CMP_CMDS::emit_local(const std::wstring &type)
{
	return emit_local(type, cend());
}

// checks if the string is autogenerated local name
bool B1_CMP_CMDS::is_gen_local(const std::wstring &s) const
{
	return s.find(get_name_space_prefix() + L"__LCL_") == 0;
}

std::wstring B1_CMP_CMDS::emit_command(const std::wstring &name, const_iterator pos, const std::vector<std::wstring> &args /*= std::vector<std::wstring>()*/)
{
	B1_CMP_CMD cmd(_curr_line_num, _curr_line_cnt, _curr_src_file_id, _curr_src_line_id);

	cmd.type = B1_CMD_TYPE::B1_CMD_TYPE_COMMAND;
	cmd.cmd = name;
	
	//cmd.args = args;
	cmd.args.clear();
	for(const auto &a: args)
	{
		cmd.args.push_back(B1_CMP_ARG(a));
	}

	insert(pos, cmd);

	return name;
}

std::wstring B1_CMP_CMDS::emit_command(const std::wstring &name, const std::vector<std::wstring> &args /*= std::vector<std::wstring>()*/)
{
	return emit_command(name, cend(), args);
}

std::wstring B1_CMP_CMDS::emit_command(const std::wstring &name, const_iterator pos, const std::wstring &arg)
{
	return emit_command(name, pos, std::vector<std::wstring>(1, arg));
}

std::wstring B1_CMP_CMDS::emit_command(const std::wstring &name, const std::wstring &arg)
{
	return emit_command(name, cend(), arg);
}

std::wstring B1_CMP_CMDS::emit_command(const std::wstring &name, const_iterator pos, const std::vector<B1_TYPED_VALUE> &args)
{
	B1_CMP_CMD cmd(_curr_line_num, _curr_line_cnt, _curr_src_file_id, _curr_src_line_id);

	cmd.type = B1_CMD_TYPE::B1_CMD_TYPE_COMMAND;
	cmd.cmd = name;
	
	//cmd.args = args;
	cmd.args.clear();
	for(const auto &a : args)
	{
		cmd.args.push_back(B1_CMP_ARG(a.value, a.type));
	}
	
	insert(pos, cmd);

	return name;
}

std::wstring B1_CMP_CMDS::emit_command(const std::wstring &name, const std::vector<B1_TYPED_VALUE> &args)
{
	return emit_command(name, cend(), args);
}

std::wstring B1_CMP_CMDS::emit_command(const std::wstring &name, const_iterator pos, const B1_TYPED_VALUE &arg)
{
	return emit_command(name, pos, std::vector<B1_TYPED_VALUE>(1, arg));
}

std::wstring B1_CMP_CMDS::emit_command(const std::wstring &name, const_iterator pos, const std::vector<B1_CMP_ARG> &args)
{
	B1_CMP_CMD cmd(_curr_line_num, _curr_line_cnt, _curr_src_file_id, _curr_src_line_id);

	cmd.type = B1_CMD_TYPE::B1_CMD_TYPE_COMMAND;
	cmd.cmd = name;
	std::copy(args.begin(), args.end(), std::back_inserter(cmd.args));
	insert(pos, cmd);
	return name;
}

std::wstring B1_CMP_CMDS::emit_command(const std::wstring &name, const std::vector<B1_CMP_ARG> &args)
{
	return emit_command(name, cend(), args);
}

B1_CMP_CMDS::iterator B1_CMP_CMDS::emit_inline_asm(const_iterator pos)
{
	B1_CMP_CMD cmd(_curr_line_num, _curr_line_cnt, _curr_src_file_id, _curr_src_line_id);

	cmd.type = B1_CMD_TYPE::B1_CMD_TYPE_INLINE_ASM;
	return insert(pos, cmd);
}

B1_CMP_CMDS::iterator B1_CMP_CMDS::emit_inline_asm(void)
{
	return emit_inline_asm(cend());
}


B1_CMP_FN_ARG::B1_CMP_FN_ARG(const std::wstring &tp, bool opt /*= false*/, const std::wstring &dv /*= std::wstring()*/)
{
	type = tp;
	optional = opt;
	defval = dv;
}


B1_CMP_FN::B1_CMP_FN(const std::wstring &nm, const std::wstring &rt, const std::initializer_list<B1_CMP_FN_ARG> &arglist, const std::wstring &in, bool stdfn /*= true*/)
{
	name = nm;
	rettype = rt;
	std::copy(arglist.begin(), arglist.end(), std::back_inserter(args));
	iname = in;
	isstdfn = stdfn;
}

B1_CMP_FN::B1_CMP_FN(const std::wstring &nm, const std::wstring &rt, const std::initializer_list<std::wstring> &arglist, const std::wstring &in, bool stdfn /*= true*/)
{
	name = nm;
	rettype = rt;
	std::copy(arglist.begin(), arglist.end(), std::back_inserter(args));
	iname = in;
	isstdfn = stdfn;
}

B1_CMP_FN::B1_CMP_FN(const std::wstring &nm, const std::wstring &rt, const std::vector<std::wstring> &arglist, const std::wstring &in, bool stdfn /*= true*/)
{
	name = nm;
	rettype = rt;
	std::copy(arglist.begin(), arglist.end(), std::back_inserter(args));
	iname = in;
	isstdfn = stdfn;
}

B1_CMP_FN::B1_CMP_FN(const std::wstring &nm, const std::wstring &rt, const std::vector<B1_CMP_FN_ARG> &arglist, const std::wstring &in, bool stdfn /*= true*/)
{
	name = nm;
	rettype = rt;
	std::copy(arglist.begin(), arglist.end(), std::back_inserter(args));
	iname = in;
	isstdfn = stdfn;
}


const B1_CMP_FN B1_CMP_FNS::_fns[] =
{
//            name,         ret. type   arg. types(def. values) fn. name in std. library
	B1_CMP_FN(L"LEN",		L"BYTE",	{ L"STRING" },			L"__LIB_STR_LEN"),
	B1_CMP_FN(L"ASC",		L"BYTE",	{ L"STRING" },			L"__LIB_STR_ASC"),
	B1_CMP_FN(L"CHR$",		L"STRING",	{ L"BYTE" },			L"__LIB_STR_CHR"),
	B1_CMP_FN(L"STR$",		L"STRING",	{ L"INT" },				L"__LIB_STR_STR_I"),
	B1_CMP_FN(L"STR$",		L"STRING",	{ L"WORD" },			L"__LIB_STR_STR_W"),
	B1_CMP_FN(L"VAL",		L"INT",		{ L"STRING" },			L"__LIB_STR_VAL"),
	B1_CMP_FN(L"MID$",		L"STRING",	{ B1_CMP_FN_ARG(L"STRING"), B1_CMP_FN_ARG(L"BYTE"), B1_CMP_FN_ARG(L"BYTE", true, std::to_wstring(B1C_T_CONST::B1C_MAX_STR_LEN)) }, L"__LIB_STR_MID"),
	B1_CMP_FN(L"INSTR",		L"BYTE",	{ B1_CMP_FN_ARG(L"BYTE", true, L"1"), B1_CMP_FN_ARG(L"STRING"), B1_CMP_FN_ARG(L"STRING") }, L"__LIB_STR_INS"),
	B1_CMP_FN(L"LTRIM$",	L"STRING",	{ L"STRING" },			L"__LIB_STR_LTRIM"),
	B1_CMP_FN(L"RTRIM$",	L"STRING",	{ L"STRING" },			L"__LIB_STR_RTRIM"),
	B1_CMP_FN(L"LEFT$",		L"STRING",	{ L"STRING", L"BYTE" },	L"__LIB_STR_LEFT"),
	B1_CMP_FN(L"RIGHT$",	L"STRING",	{ L"STRING", L"BYTE" },	L"__LIB_STR_RIGHT"),
	B1_CMP_FN(L"LSET$",		L"STRING",	{ L"STRING", L"BYTE" },	L"__LIB_STR_LSET"),
	B1_CMP_FN(L"RSET$",		L"STRING",	{ L"STRING", L"BYTE" },	L"__LIB_STR_RSET"),
	B1_CMP_FN(L"UCASE$",	L"STRING",	{ L"STRING" },			L"__LIB_STR_UCASE"),
	B1_CMP_FN(L"LCASE$",	L"STRING",	{ L"STRING" },			L"__LIB_STR_LCASE"),
	B1_CMP_FN(L"SET$",		L"STRING",	{ L"STRING", L"BYTE" },	L"__LIB_STR_SET"),

	B1_CMP_FN(L"ABS",		L"INT",		{ L"INT" },				L""),
	B1_CMP_FN(L"ABS",		L"WORD",	{ L"WORD" },			L""),
	B1_CMP_FN(L"ABS",		L"BYTE",	{ L"BYTE" },			L""),
	B1_CMP_FN(L"SGN",		L"INT",		{ L"INT" },				L""),
	B1_CMP_FN(L"SGN",		L"BYTE",	{ L"WORD" },			L""),
	B1_CMP_FN(L"SGN",		L"BYTE",	{ L"BYTE" },			L""),
	B1_CMP_FN(L"STR$",		L"STRING",	{ L"STRING" },			L""),

	B1_CMP_FN(L"TAB",		L"STRING",	{ L"BYTE" },			L""),
	B1_CMP_FN(L"SPC",		L"STRING",	{ L"BYTE" },			L""),
	B1_CMP_FN(L"NL",		L"STRING",	std::initializer_list<std::wstring>(),	L""),

	// the last empty record, used to get records number
	B1_CMP_FN(L"",			L"",		std::initializer_list<std::wstring>(),	L"")
};


// check standard function existence by name
bool B1_CMP_FNS::fn_exists(const std::wstring &name)
{
	for(int i = 0; !_fns[i].name.empty(); i++)
	{
		if(_fns[i].name == name)
		{
			return true;
		}
	}

	// check inline and special functions
	if(name == L"IIF" || name == L"IIF$" || name == L"ABS" || name == L"SGN" || name == L"TAB" || name == L"SPC")
	{
		return true;
	}

	return false;
}

const B1_CMP_FN *B1_CMP_FNS::get_fn(const std::wstring &name)
{
	for(int i = 0; !_fns[i].name.empty(); i++)
	{
		if(_fns[i].name == name)
		{
			return &_fns[i];
		}
	}

	return nullptr;
}

const B1_CMP_FN *B1_CMP_FNS::get_fn(const B1_TYPED_VALUE &val)
{
	for(int i = 0; !_fns[i].name.empty(); i++)
	{
		// function without arguments
		if(_fns[i].name == val.value && _fns[i].args.size() == 0)
		{
			return &_fns[i];
		}
	}

	return nullptr;
}

const B1_CMP_FN *B1_CMP_FNS::get_fn(const B1_CMP_ARG &arg)
{
	bool found = false;

	for(int i = 0; !_fns[i].name.empty(); i++)
	{
		// check function name and arguments count
		if(_fns[i].name == arg[0].value && _fns[i].args.size() == arg.size() - 1)
		{
			found = true;

			// check function arguments
			for(int a = 0; a < _fns[i].args.size(); a++)
			{
				if(arg[a + 1].value.empty() && _fns[i].args[a].optional)
				{
					continue;
				}
				if(!arg[a + 1].value.empty() && arg[a + 1].type == _fns[i].args[a].type)
				{
					continue;
				}

				found = false;
				break;
			}
		}

		if(found)
		{
			return &_fns[i];
		}
	}

	// try compatible types
	for(int i = 0; !_fns[i].name.empty(); i++)
	{
		// check function name and arguments count
		if(_fns[i].name == arg[0].value && _fns[i].args.size() == arg.size() - 1)
		{
			found = true;

			// check function arguments
			for(int a = 0; a < _fns[i].args.size(); a++)
			{
				if(arg[a + 1].value.empty() && _fns[i].args[a].optional)
				{
					continue;
				}
				if(!arg[a + 1].value.empty() && B1CUtils::are_types_compatible(arg[a + 1].type, _fns[i].args[a].type))
				{
					continue;
				}

				found = false;
				break;
			}
		}

		if(found)
		{
			return &_fns[i];
		}
	}

	return nullptr;
}

std::wstring B1_CMP_FNS::get_fn_int_name(const std::wstring &name)
{
	for(int i = 0; !_fns[i].name.empty(); i++)
	{
		if(_fns[i].name == name)
		{
			return name;
		}
	}

	return std::wstring();
}


B1_CMP_VAR::B1_CMP_VAR()
: dim_num(-1)
, size(0)
, address(0)
, use_symbol(false)
, fixed_size(false)
, is_volatile(false)
, src_file_id(-1)
, src_line_cnt(0)
{
}

B1_CMP_VAR::B1_CMP_VAR(const std::wstring &nm, const std::wstring &tp, int32_t dn, bool vlt, int32_t sfid, int32_t slc)
: B1_CMP_VAR()
{
	name = nm;
	type = tp;
	dim_num = dn;
	is_volatile = vlt;
	src_file_id = sfid;
	src_line_cnt = slc;
}
