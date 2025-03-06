/*
 BASIC1 compiler
 Copyright (c) 2021-2024 Nikolay Pletnev
 MIT license

 b1c.cpp: BASIC1 compiler
*/


#include <cstdio>
#include <clocale>
#include <cstring>
#include <set>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <list>
#include <stack>
#include <functional>

#include "../../common/source/version.h"
#include "../../common/source/gitrev.h"
#include "../../common/source/trgsel.h"

#include "b1c.h"


extern "C" B1_T_ERROR b1_ex_prg_set_prog_file(const char *prog_file);


static const char *version = B1_CMP_VERSION;


static const B1_T_CHAR _AT[] = { 2, 'A', 'T' };
static const B1_T_CHAR _GLOBAL[] = { 6, 'G', 'L', 'O', 'B', 'A', 'L' };
static const B1_T_CHAR _VOLATILE[] = { 8, 'V', 'O', 'L', 'A', 'T', 'I', 'L', 'E' };
static const B1_T_CHAR _STATIC[] = { 6, 'S', 'T', 'A', 'T', 'I', 'C' };
static const B1_T_CHAR _CONST[] = { 5, 'C', 'O', 'N', 'S', 'T' };
static const B1_T_CHAR _NOCHECK[] = { 7, 'N', 'O', 'C', 'H', 'E', 'C', 'K' };
static const B1_T_CHAR _INPUTDEVICE[] = { 11, 'I', 'N', 'P', 'U', 'T', 'D', 'E', 'V', 'I', 'C', 'E' };
static const B1_T_CHAR _OUTPUTDEVICE[] = { 12, 'O', 'U', 'T', 'P', 'U', 'T', 'D', 'E', 'V', 'I', 'C', 'E' };
static const B1_T_CHAR _USING[] = { 5, 'U', 'S', 'I', 'N', 'G' };

static const B1_T_CHAR *CONST_VAL_SEPARATORS[3] = { _COMMA, _CLBRACKET, NULL };
static const B1_T_CHAR *CONST_STOP_TOKEN[2] = { _CLBRACKET, NULL };
static const B1_T_CHAR *PUT_GET_STOP_TOKENS[3] = { _COMMA, _USING, NULL };
static const B1_T_CHAR *USING_SEPARATORS[3] = { _COMMA, _CLBRACKET, NULL };


Settings global_settings;
Settings &_global_settings = global_settings;


B1C_T_ERROR B1FileCompiler::put_var_name(const std::wstring &name, const B1Types type, int dims, bool is_global, bool is_volatile, bool is_mem_var, bool is_static, bool is_const)
{
	// checks for global variables
	if(!_compiler.global_var_check(is_global, is_mem_var, is_static, is_const, name))
	{
		return static_cast<B1C_T_ERROR>(B1_RES_EIDINUSE);
	}

	auto var = _var_names.find(name);

	if(var != _var_names.end())
	{
		// forbid multiple DIMs for memory, static and const variables
		if(is_mem_var || is_static || is_const)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_EIDINUSE);
		}

		// forbid declaring global and local variables with the same name
		if(is_global)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_EIDINUSE);
		}

		if(!is_global)
		{
			auto gen_name = var->second;
			auto var1 = _vars.find(gen_name);

			if(std::get<0>(var1->second) != type)
			{
				return B1C_T_ERROR::B1C_RES_EVARTYPMIS;
			}

			if(std::get<2>(var1->second) != is_volatile)
			{
				return B1C_T_ERROR::B1C_RES_EVARTYPMIS;
			}

			if(std::get<1>(var1->second) != dims)
			{
				return B1C_T_ERROR::B1C_RES_EVARDIMMIS;
			}
		}
	}
	else
	if(!is_global)
	{
		auto gen_name = get_name_space_prefix() + (is_mem_var ? L"__MEM_" : L"__VAR_") + name;

		_var_names[name] = gen_name;
		_vars[gen_name] = std::make_tuple(type, dims, is_volatile, is_mem_var, is_static, is_const);
	}

	if(is_global)
	{
		return _compiler.put_global_var_name(name, type, dims, is_volatile, is_mem_var, is_static, is_const);
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::put_const_var_init_values(const std::wstring &name, const std::vector<std::wstring> &const_init)
{
	auto var = _vars.find(name);

	if(var == _vars.end())
	{
		var = _compiler._global_vars.find(name);
		if(var == _compiler._global_vars.end())
		{
			return static_cast<B1C_T_ERROR>(B1_RES_EUNKIDENT);
		}

		_compiler._global_const_init[name] = std::make_pair(std::get<0>(var->second), const_init);
	}
	else
	{
		_const_init[name] = std::make_pair(std::get<0>(var->second), const_init);
	}
	return B1C_T_ERROR::B1C_RES_OK;
}

// expl stands for explicitly declared variable
std::wstring B1FileCompiler::get_var_name(const std::wstring &name, bool &expl) const
{
	std::wstring gen_name;

	expl = true;

	auto v = _var_names.find(name);
	if(v != _var_names.end())
	{
		gen_name = v->second;
	}

	if(gen_name.empty())
	{
		gen_name = _compiler.get_global_var_name(name);
	}

	// implicitly declared variable
	if(gen_name.empty())
	{
		expl = false;
		gen_name = get_name_space_prefix() + L"__VAR_" + name;
	}

	return gen_name;
}

bool B1FileCompiler::is_mem_var_name(const std::wstring &name) const
{
	auto var = _vars.find(name);

	if(var != _vars.end())
	{
		return std::get<3>(var->second);
	}

	return _compiler.is_global_mem_var_name(name);
}

bool B1FileCompiler::is_volatile_var(const std::wstring &name) const
{
	auto var = _vars.find(name);

	if(var != _vars.end())
	{
		return std::get<2>(var->second);
	}

	return _compiler.is_global_volatile_var(name);
}

bool B1FileCompiler::is_const_var(const std::wstring &name) const
{
	auto var = _vars.find(name);

	if(var != _vars.end())
	{
		return std::get<5>(var->second);
	}

	return _compiler.is_global_const_var(name);
}

int B1FileCompiler::get_var_dim(const std::wstring &name) const
{
	auto var = _vars.find(name);

	if(var != _vars.end())
	{
		return std::get<1>(var->second);
	}

	return _compiler.get_global_var_dim(name);
}

B1Types B1FileCompiler::get_var_type(const std::wstring &name) const
{
	auto var = _vars.find(name);

	if(var != _vars.end())
	{
		return std::get<0>(var->second);
	}

	return _compiler.get_global_var_type(name);
}

// check function existence by name (standard, local and global)
bool B1FileCompiler::fn_exists(const std::wstring &name)
{
	// standard functions
	if(B1_CMP_FNS::fn_exists(name))
	{
		return true;
	}

	// local user-defined functions
	if(_ufns.find(name) != _ufns.end())
	{
		return true;
	}

	// global user-defined functions
	return _compiler.global_fn_exists(name);
}

bool B1FileCompiler::add_ufn(bool global, const std::wstring &nm, const B1Types rt, const std::vector<B1Types> &arglist)
{
	if(fn_exists(nm))
	{
		return false;
	}

	if(global)
	{
		return _compiler.add_global_ufn(nm, rt, arglist, L"__DEF_" + nm);
	}

	_ufns.emplace(std::make_pair(nm, B1_CMP_FN(nm, rt, arglist, get_name_space_prefix() + L"__DEF_" + nm, false)));

	return true;
}

// returns pointer to B1_CMP_FN class of the function identified by name (standard, local, global)
const B1_CMP_FN *B1FileCompiler::get_fn(const std::wstring &name)
{
	auto fn = B1_CMP_FNS::get_fn(name);

	if(fn == nullptr)
	{
		const auto &ufn = _ufns.find(name);
		if(ufn != _ufns.end())
		{
			fn = &(ufn->second);
		}
	}

	if(fn == nullptr)
	{
		fn = _compiler.get_global_ufn(name);
	}
	
	return fn;
}

// returns pointer to B1_CMP_FN class of the function identified by name, arg. count and arg. types (standard, local, global)
const B1_CMP_FN *B1FileCompiler::get_fn(const B1_TYPED_VALUE &val)
{
	auto fn = B1_CMP_FNS::get_fn(val);

	if(fn == nullptr)
	{
		const auto &ufn = _ufns.find(val.value);
		if(ufn != _ufns.end() && ufn->second.args.size() == 0)
		{
			fn = &(ufn->second);
		}
	}

	if(fn == nullptr)
	{
		fn = _compiler.get_global_ufn(val);
	}

	return fn;
}

// returns pointer to B1_CMP_FN class of the function identified by name, arg. count and arg. types (standard, local, global)
const B1_CMP_FN *B1FileCompiler::get_fn(const B1_CMP_ARG &arg)
{
	auto fn = B1_CMP_FNS::get_fn(arg);

	if(fn == nullptr)
	{
		const auto &ufn = _ufns.find(arg[0].value);
		if(ufn != _ufns.end() && ufn->second.args.size() == arg.size() - 1)
		{
			fn = &(ufn->second);
		}
	}

	if(fn == nullptr)
	{
		fn = _compiler.get_global_ufn(arg);
	}

	return fn;
}

std::wstring B1FileCompiler::get_fn_int_name(const std::wstring &name)
{
	std::wstring iname = B1_CMP_FNS::get_fn_int_name(name);

	if(iname.empty())
	{
		const auto &ufn = _ufns.find(name);
		if(ufn != _ufns.end())
		{
			iname = ufn->second.iname;
		}

		if(iname.empty())
		{
			iname = _compiler.get_global_ufn_int_name(name);
		}
	}

	return iname;
}

void B1FileCompiler::change_ufn_names()
{
	std::vector<B1_CMP_FN> ufns;

	for(const auto &ufn: _ufns)
	{
		ufns.push_back(ufn.second);
		ufns.back().name = ufn.second.iname;
	}

	_ufns.clear();

	for(auto &ufn: ufns)
	{
		_ufns.emplace(std::make_pair(ufn.name, B1_CMP_FN(ufn.name, ufn.rettype, ufn.args, ufn.iname, false)));
	}
}

// processes RPN expressions like (10)--- -> (-10)
bool B1FileCompiler::correct_rpn(B1_CMP_EXP_TYPE &res_type, B1_CMP_ARG &res, bool get_ref)
{
	if(get_ref)
	{
		return false;
	}

	if(B1_RPNREC_GET_TYPE(b1_rpn[0].flags) == B1_RPNREC_TYPE_FNVAR)
	{
		B1_T_INDEX id_off = (*(b1_rpn)).data.id.offset;
		B1_T_INDEX id_len = (*(b1_rpn)).data.id.length;
		auto token = B1CUtils::get_progline_substring(id_off, id_off + id_len);

		if(Utils::check_const_name(token) && b1_rpn[1].flags == 0)
		{
			res = token;
			res_type = B1_CMP_EXP_TYPE::B1_CMP_ET_IMM_VAL;

			return true;
		}

		return false;
	}

	if(B1_RPNREC_GET_TYPE(b1_rpn[0].flags) == B1_RPNREC_TYPE_IMM_VALUE && !B1_RPNREC_TEST_IMM_VALUE_NULL_ARG(b1_rpn[0].flags))
	{
		B1_T_INDEX id_off = (*(b1_rpn)).data.token.offset;
		B1_T_INDEX id_len = (*(b1_rpn)).data.token.length;
		auto token = B1CUtils::get_progline_substring(id_off, id_off + id_len);

		B1_T_INDEX i = 1;
		while(B1_RPNREC_GET_TYPE(b1_rpn[i].flags) == B1_RPNREC_TYPE_OPER && B1_RPNREC_GET_OPER_PRI(b1_rpn[i].flags) == 0 && B1_T_ISMINUS((*(b1_rpn + i)).data.oper.c))
		{
			i++;
		}

		if(b1_rpn[i].flags != 0)
		{
			return false;
		}

		if((i % 2) == 0)
		{
			// change sign
			auto c = token.front();
			if(c == L'+' || c == L'-')
			{
				token.erase(0, 1);
			}

			if(c != L'-')
			{
				token.insert(0, 1, L'-');
			}
		}

		res = token;
		res_type = B1_CMP_EXP_TYPE::B1_CMP_ET_IMM_VAL;

		return true;
	}

	return false;
}

B1_T_ERROR B1FileCompiler::process_expression(iterator pos, B1_CMP_EXP_TYPE &res_type, B1_CMP_ARG &res, bool get_ref /*= false*/)
{
	uint8_t tflags, args_num;
	B1_T_INDEX i;
	B1_T_INDEX id_off, id_len;
	B1_T_IDHASH hash;
	// <var> = IIF(<cond>, <val1>, <val2>)
	// ->
	// <cond>
	// LA,local
	// JF,label1
	// =,<val1>,local
	// JMP,label2
	// :label1
	// =,<val2>,local
	// :label2
	// =,local,<var>
	//                     label1        label2        local
	std::vector<std::tuple<std::wstring, std::wstring, std::wstring>> min_eval;
	//          LA stmt, assign1, assign 2
	std::vector<std::vector<std::reference_wrapper<B1_CMP_CMD>>> iif_refs;
	std::vector<std::pair<std::wstring, B1_CMP_VAL_TYPE>> stack;

	bool log_op = false;

	const_iterator last_loc_assign;
	int last_ind = -1;
	std::wstring last_token;

	bool const_name = false;

	if(correct_rpn(res_type, res, get_ref))
	{
		return B1_RES_OK;
	}

	res_type = B1_CMP_EXP_TYPE::B1_CMP_ET_UNKNOWN;
	res = B1_CMP_ARG();

	// current RPN record index
	i = 0;

	while(true)
	{
		std::wstring token;

		// (B1_RPNREC_TYPE_IMM_VALUE, B1_RPNREC_TYPE_FNVAR, B1_RPNREC_TYPE_OPER (B1_RPNREC_OPER_LEFT_ASSOC and priority))
		tflags = (*(b1_rpn + i)).flags;

		// check for RPN end
		if(tflags == 0)
		{
			break;
		}

		log_op = false;
		last_token.clear();
		const_name = false;

		if(B1_RPNREC_TEST_SPEC_ARG(tflags))
		{
			// produce instructions for minimal evaluation
			if(tflags == B1_RPNREC_TYPE_SPEC_ARG_1)
			{
				auto label1 = emit_label(true);
				auto label2 = emit_label(true);
				min_eval.push_back(std::make_tuple(label1, label2, emit_local(B1Types::B1T_UNKNOWN, pos)));

				iif_refs.push_back({ *std::prev(pos) });

				emit_command(L"JF", pos, std::get<0>(min_eval.back()));
			}
			else
			if(tflags == B1_RPNREC_TYPE_SPEC_ARG_2)
			{
				const auto &v = stack.back();
				emit_command(L"=", pos, std::vector<std::wstring>({ v.first, std::get<2>(min_eval.back()) }));

				iif_refs.back().push_back(*std::prev(pos));

				if(is_gen_local(v.first))
				{
					emit_command(L"LF", pos, v.first);
				}

				stack.pop_back();

				emit_command(L"JMP", pos, std::get<1>(min_eval.back()));
				emit_label(std::get<0>(min_eval.back()), pos);
			}
			else
			{
				const auto &v = stack.back();
				emit_command(L"=", pos, std::vector<std::wstring>({ v.first, std::get<2>(min_eval.back()) }));

				iif_refs.back().push_back(*std::prev(pos));

				if(is_gen_local(v.first))
				{
					emit_command(L"LF", pos, v.first);
				}

				stack.pop_back();

				emit_label(std::get<1>(min_eval.back()), pos);

				stack.push_back(std::pair<std::wstring, B1_CMP_VAL_TYPE>(std::get<2>(min_eval.back()), B1_CMP_VAL_TYPE::B1_CMP_VT_LOCAL));

				min_eval.pop_back();
			}

			i++;
			continue;
		}

		if(B1_RPNREC_TEST_TYPES(tflags, B1_RPNREC_TYPE_FNVAR | B1_RPNREC_TYPE_FN_ARG))
		{
			hash = (*(b1_rpn + i)).data.id.hash;
			id_off = (*(b1_rpn + i)).data.id.offset;
			id_len = (*(b1_rpn + i)).data.id.length;
			token = Utils::str_toupper(B1CUtils::get_progline_substring(id_off, id_off + id_len));

			if(B1_RPNREC_GET_TYPE(tflags) == B1_RPNREC_TYPE_FNVAR && Utils::check_const_name(token))
			{
				args_num = 0;
				const_name = true;
			}
			else
			if(B1_RPNREC_GET_TYPE(tflags) == B1_RPNREC_TYPE_FNVAR)
			{
				args_num = B1_RPNREC_GET_FNVAR_ARG_NUM(tflags);

				// save original token name
				last_token = token;

				if(hash == B1_FN_IIF_FN_HASH || hash == B1_FN_STRIIF_FN_HASH)
				{
					// special case: IIF functions
					if(args_num != 3)
					{
						return B1_RES_EWRARGCNT;
					}

					auto type = (hash == B1_FN_STRIIF_FN_HASH) ? B1Types::B1T_STRING : B1Types::B1T_COMMON;
					auto type_name = Utils::get_type_name(type);

					auto &args = iif_refs.back()[0].get().args;
					args[1] = B1_CMP_ARG(type_name, type);
					iif_refs.back()[1].get().args[1][0].type = type;
					iif_refs.back()[2].get().args[1][0].type = type;
					iif_refs.pop_back();
				}
				else
				{
					// check if the token is a function
					auto fn = get_fn(token);
					if(fn == nullptr)
					{
						// a variable
						bool expl = false;
						token = get_var_name(token, expl);
						if(_opt_explicit && !expl)
						{
							return B1_RES_EUNKIDENT;
						}
					}
					else
					{
						if(args_num != fn->args.size())
						{
							return B1_RES_EWRARGCNT;
						}
						// use get_fn_int_name() function to get function name because it leaves standard function names as is
						//token = fn->iname;
						token = get_fn_int_name(token);
					}
				}
			}
			else
			{
				args_num = B1_RPNREC_GET_FN_ARG_INDEX(tflags);
			}
		}
		else
		if(B1_RPNREC_TEST_TYPES(tflags, B1_RPNREC_TYPE_OPER))
		{
			token = (wchar_t)(*(b1_rpn + i)).data.oper.c;
			if((*(b1_rpn + i)).data.oper.c1 != 0)
			{
				token += (wchar_t)(*(b1_rpn + i)).data.oper.c1;
			}
		}
		else
		{
			// B1_RPNREC_TYPE_IMM_VALUE
			id_off = (*(b1_rpn + i)).data.token.offset;
			id_len = (*(b1_rpn + i)).data.token.length;
			token = B1CUtils::get_progline_substring(id_off, id_off + id_len, true);
		}

		if(	const_name ||
			B1_RPNREC_TEST_TYPES(tflags, B1_RPNREC_TYPE_FN_ARG | B1_RPNREC_TYPE_IMM_VALUE) ||
			(B1_RPNREC_GET_TYPE(tflags) == B1_RPNREC_TYPE_FNVAR && args_num == 0))
		{
			if(const_name)
			{
				stack.push_back(std::pair<std::wstring, B1_CMP_VAL_TYPE>(token, B1_CMP_VAL_TYPE::B1_CMP_VT_IMM_VAL));
			}
			else
			{
				uint8_t type = B1_RPNREC_GET_TYPE(tflags);
				B1_CMP_VAL_TYPE vtype = type == B1_RPNREC_TYPE_IMM_VALUE ? B1_CMP_VAL_TYPE::B1_CMP_VT_IMM_VAL :
					type == B1_RPNREC_TYPE_FNVAR ? B1_CMP_VAL_TYPE::B1_CMP_VT_FNVAR :
					B1_CMP_VAL_TYPE::B1_CMP_VT_FN_ARG;

				if(vtype == B1_CMP_VAL_TYPE::B1_CMP_VT_IMM_VAL && B1_RPNREC_TEST_IMM_VALUE_NULL_ARG(tflags))
				{
					// omitted function argument
					token.clear();
				}
				else
				if(vtype == B1_CMP_VAL_TYPE::B1_CMP_VT_FN_ARG)
				{
					wchar_t c = token.back();
					token = L"__ARG_" + std::to_wstring(args_num);
					// no floating-point types at the moment
					if(c == L'$' /*|| c == L'!' || c == L'#'*/ || c == L'%')
					{
						token += c;
					}
				}

				stack.push_back(std::pair<std::wstring, B1_CMP_VAL_TYPE>(token, vtype));
			}

			i++;
			continue;
		}

		if(B1_RPNREC_TEST_TYPES(tflags, B1_RPNREC_TYPE_FNVAR | B1_RPNREC_TYPE_OPER))
		{
			bool exclude = false;

			if(B1_RPNREC_GET_TYPE(tflags) == B1_RPNREC_TYPE_OPER)
			{
				// test for unary operator (0 - priority of unary -/+ operators and NOT operator)
				args_num = B1_RPNREC_TEST_OPER_PRI(tflags, 0) ? 1 : 2;
			}
			else
			{
				// do not call IIF and IIF$
				if(hash == B1_FN_IIF_FN_HASH || hash == B1_FN_STRIIF_FN_HASH)
				{
					exclude = true;
				}
			}

			if(!exclude)
			{
				if(stack.size() < args_num)
				{
					return B1_RES_ESYNTAX;
				}

				B1_CMP_ARGS args;

				if(B1_RPNREC_GET_TYPE(tflags) == B1_RPNREC_TYPE_OPER)
				{
					if(token == L"=")
					{
						token = L"==";
					}

					log_op = B1CUtils::is_log_op(token);
				}
				else
				{
					args.push_back(B1_CMP_ARG(token));
					token = L"=";
				}

				std::wstring local;

				if(!log_op)
				{
					local = emit_local(B1Types::B1T_UNKNOWN, pos);
				}

				std::vector<std::wstring> locals;

				for(auto a = stack.end() - args_num; a != stack.end(); a++)
				{
					const auto &v = a->first;

					if(is_gen_local(v))
					{
						locals.push_back(v);
					}

					if(B1_RPNREC_GET_TYPE(tflags) == B1_RPNREC_TYPE_OPER)
					{
						args.push_back(B1_CMP_ARG(v));
					}
					else
					{
						args.back().push_back(B1_TYPED_VALUE(v));
					}
				}

				stack.erase(stack.end() - args_num, stack.end());

				if(!log_op)
				{
					args.push_back(B1_CMP_ARG(local));
				}

				B1_CMP_CMD cmd(_curr_line_num, _curr_line_cnt, _curr_src_file_id, _curr_src_line_id);
				cmd.type = B1_CMD_TYPE::B1_CMD_TYPE_COMMAND;
				cmd.cmd = token;
				cmd.args = args;
				insert(pos, cmd);

				if(B1_RPNREC_GET_TYPE(tflags) != B1_RPNREC_TYPE_OPER)
				{
					last_loc_assign = std::prev(pos);
				}

				for(auto l = locals.crbegin(); l != locals.crend(); l++)
				{
					emit_command(L"LF", pos, *l);
				}

				if(!log_op)
				{
					stack.push_back(std::pair<std::wstring, B1_CMP_VAL_TYPE>(local, B1_CMP_VAL_TYPE::B1_CMP_VT_LOCAL));
				}

				if(B1_RPNREC_GET_TYPE(tflags) == B1_RPNREC_TYPE_OPER)
				{
					last_ind = -1;
				}
				else
				{
					last_ind = size() - 1;
				}
			}
		}

		i++;
	}

	// log_op = true - logical expression for IF/ELSEIF/WHILE statements
	if(log_op)
	{
		// logical expression
		if(stack.size() != 0)
		{
			return B1_RES_ESYNTAX;
		}

		res_type = B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL;
	}
	else
	{
		if(stack.size() != 1)
		{
			return B1_RES_ESYNTAX;
		}

		if(stack.back().second == B1_CMP_VAL_TYPE::B1_CMP_VT_IMM_VAL)
		{
			// immediate value
			if(get_ref)
			{
				return B1_RES_ESYNTAX;
			}

			res = stack.back().first;
			res_type = B1_CMP_EXP_TYPE::B1_CMP_ET_IMM_VAL;
		}
		else
		if(stack.back().second == B1_CMP_VAL_TYPE::B1_CMP_VT_LOCAL)
		{
			// local variable
			if(get_ref)
			{
				if(last_ind != size() - 1)
				{
					return B1_RES_ESYNTAX;
				}

				// subscripted variable or function call with arguments
				if(fn_exists(last_token))
				{
					return B1_RES_ESYNTAX;
				}

				for(auto a = last_loc_assign->args[0].begin(); a != last_loc_assign->args[0].end(); a++)
				{
					res.push_back(*a);
				}

				erase(std::prev(last_loc_assign), pos);

				res_type = B1_CMP_EXP_TYPE::B1_CMP_ET_VAR;
			}
			else
			{
				res = stack.back().first;
				res_type = B1_CMP_EXP_TYPE::B1_CMP_ET_LOCAL;
			}
		}
		else
		if(stack.back().second == B1_CMP_VAL_TYPE::B1_CMP_VT_FN_ARG)
		{
			if(get_ref)
			{
				return B1_RES_ESYNTAX;
			}
			res = stack.back().first;
			res_type = B1_CMP_EXP_TYPE::B1_CMP_ET_VAR;
		}
		else
		{
			// variable or function call with no arguments
			if(get_ref && fn_exists(last_token))
			{
				return B1_RES_ESYNTAX;
			}
			res = stack.back().first;
			res_type = B1_CMP_EXP_TYPE::B1_CMP_ET_VAR;
		}
	}

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::eval_chr(const std::wstring &num_val, const B1Types type, std::wstring &res_str)
{
	int32_t n;

	auto err = Utils::str2int32(num_val, n);
	if(err != B1_RES_OK)
	{
		return err;
	}

	Utils::Utils::correct_int_value(n, type);
	if(n < 0 || n > 255)
	{
		return B1_RES_EINVARG;
	}

	res_str.append(1, L'\"');

	if(n == '\0')
	{
		res_str += L"\\0";
	}
	else
	if(n == '\t')
	{
		res_str += L"\\t";
	}
	else
	if(n == '\n')
	{
		res_str += L"\\n";
	}
	else
	if(n == '\r')
	{
		res_str += L"\\r";
	}
	else
	if(n == '\"')
	{
		res_str += L"\"\"";
	}
	else
	if(n == '\\')
	{
		res_str += L"\\\\";
	}
	else
	{
		res_str.append(1, (wchar_t)n);
	}

	res_str.append(1, L'\"');

	return B1_RES_OK;
}

// eval RPNs consisting of string values, concat operators and CHR$ function calls with immediate arguments,
// e.g.: "abc" + CHR$(92) + "def" + CHR$(13) + CHR$(10)
B1_T_ERROR B1FileCompiler::concat_strings_rpn(std::wstring &res)
{
	B1_T_INDEX i;
	std::list<std::wstring> rpn;
	uint8_t tflags;

	// copy and simplify the RPN
	for(i = 0; (tflags = (*(b1_rpn + i)).flags) != 0; i++)
	{
		if(B1_RPNREC_TEST_TYPES(tflags, B1_RPNREC_TYPE_OPER))
		{
			if(B1_T_ISPLUS((*(b1_rpn + i)).data.oper.c))
			{
				rpn.push_back(L"+");
			}
			else
			{
				return B1_RES_ESYNTAX;
			}
		}
		else
		if(B1_RPNREC_TEST_TYPES(tflags, B1_RPNREC_TYPE_FNVAR))
		{
			auto id_off = (*(b1_rpn + i)).data.id.offset;
			auto id_len = (*(b1_rpn + i)).data.id.length;
			auto token = Utils::str_toupper(B1CUtils::get_progline_substring(id_off, id_off + id_len));
			if(token == L"CHR$")
			{
				rpn.push_back(L"!");
			}
			else
			{
				return B1_RES_ESYNTAX;
			}
		}
		else
		if(B1_RPNREC_TEST_TYPES(tflags, B1_RPNREC_TYPE_IMM_VALUE) && !B1_RPNREC_TEST_IMM_VALUE_NULL_ARG(tflags))
		{
			auto id_off = (*(b1_rpn + i)).data.token.offset;
			auto id_len = (*(b1_rpn + i)).data.token.length;
			rpn.push_back(B1CUtils::get_progline_substring(id_off, id_off + id_len, true));
		}
		else
		{
			return B1_RES_ESYNTAX;
		}
	}

	// evaluate simplified RPN
	while(rpn.size() > 1)
	{
		for(auto op = rpn.begin(); op != rpn.end(); op++)
		{
			if(*op == L"+")
			{
				auto op1 = std::prev(op, 2);
				auto op2 = std::prev(op);

				if(!B1CUtils::is_str_val(*op1) || !B1CUtils::is_str_val(*op2))
				{
					return B1_RES_ESYNTAX;
				}

				op1->pop_back();
				op2->erase(0, 1);
				*op = *op1 + *op2;
				rpn.erase(op1, op);
				break;
			}

			if(*op == L"!")
			{
				auto op1 = std::prev(op);
				
				op->clear();
				auto err = eval_chr(*op1, B1Types::B1T_INT, *op);
				if(err != B1_RES_OK)
				{
					return err;
				}

				rpn.erase(op1);
				break;
			}
		}
	}

	res = rpn.front();

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_option_set(const B1_T_CHAR *s, uint8_t value_type, bool onoff, int *value)
{
	B1_T_ERROR err;
	B1_TOKENDATA td;
	B1_T_INDEX len;
	uint16_t val;

	err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		return err;
	}

	len = td.length;

	if(!b1_t_strcmpi(s, b1_progline + td.offset, len))
	{
		err = b1_tok_get(td.offset + len, B1_TOK_COPY_VALUE, &td);
		if(err != B1_RES_OK)
		{
			return err;
		}

		len = td.length;
		b1_curr_prog_line_offset = td.offset;

		if(len != 0 && !(td.type & value_type))
		{
			return B1_RES_EINVARG;
		}

		if(value_type == B1_TOKEN_TYPE_NUMERIC)
		{
			if(len == 0 || len > 3)
			{
				return B1_RES_EINVARG;
			}

			b1_tmp_buf[len + 1] = 0;
			err = b1_t_strtoui16(b1_tmp_buf + 1, &val);
			if(err != B1_RES_OK)
			{
				return err;
			}

			if(val > UINT8_MAX)
			{
				return B1_RES_EINVARG;
			}

			*value = val;
		}
		else
		if(value_type == B1_TOKEN_TYPE_IDNAME)
		{
			if(onoff)
			{
				if(len == 0)
				{
					*value = 2;
				}
				else
				if(!b1_t_strcmpi(_ON, b1_tmp_buf + 1, len))
				{
					*value = 1;
				}
				else
				if(!b1_t_strcmpi(_OFF, b1_tmp_buf + 1, len))
				{
					*value = 0;
				}
				else
				{
					return B1_RES_EINVARG;
				}
			}
			else
			{
				*value = len;
			}
		}
		else
		if(value_type == B1_TOKEN_TYPE_DEVNAME)
		{
			*value = len;
		}
		else
		{
			return B1_RES_EINVARG;
		}
	}
	else
	{
		return B1_RES_EEOF;
	}

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_option_set_expr(const B1_T_CHAR *s, B1_CMP_EXP_TYPE &exp_type, B1_CMP_ARG &res)
{
	B1_T_ERROR err;
	B1_TOKENDATA td;
	B1_T_INDEX len;

	err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		return err;
	}

	len = td.length;

	if(!b1_t_strcmpi(s, b1_progline + td.offset, len))
	{
		b1_curr_prog_line_offset = td.offset + len;

		// build RPN
		err = b1_rpn_build(b1_curr_prog_line_offset, NULL, &b1_curr_prog_line_offset);
		if(err != B1_RES_OK)
		{
			return err;
		}

		exp_type = B1_CMP_EXP_TYPE::B1_CMP_ET_UNKNOWN;

		err = process_expression(end(), exp_type, res);
		if(err != B1_RES_OK)
		{
			return err;
		}

		if(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
		{
			return B1_RES_ESYNTAX;
		}
	}
	else
	{
		return B1_RES_EEOF;
	}

	return B1_RES_OK;
}

bool B1FileCompiler::st_option_check(bool first_run, bool &opt, bool &opt_def, bool val)
{
	if(first_run)
	{
		opt_def = false;
		opt = val;
	}
	else
	{
		if(!opt_def && opt != val)
		{
			return false;
		}
	}

	return true;
}

B1C_T_ERROR B1FileCompiler::st_option(bool first_run)
{
	B1_T_ERROR err;
	int value;

	err = st_option_set(_BASE, B1_TOKEN_TYPE_NUMERIC, false, &value);
	if(err == B1_RES_OK)
	{
		if(value == 0)
		{
			if(!st_option_check(first_run, _opt_base1, _opt_base1_def, false))
			{
				return B1C_T_ERROR::B1C_RES_EINCOPTS;
			}
		}
		else
		if(value == 1)
		{
			if(!st_option_check(first_run, _opt_base1, _opt_base1_def, true))
			{
				return B1C_T_ERROR::B1C_RES_EINCOPTS;
			}
		}
		else
		{
			return static_cast<B1C_T_ERROR>(B1_RES_EINVARG);
		}

		return B1C_T_ERROR::B1C_RES_OK;
	}
	else
	if(err != B1_RES_EEOF)
	{
		return static_cast<B1C_T_ERROR>(err);
	}

	err = st_option_set(_EXPLICIT, B1_TOKEN_TYPE_IDNAME, true, &value);
	if(err == B1_RES_OK)
	{
		// omitted value corresponds to ON
		if(value == 0)
		{
			if(!st_option_check(first_run, _opt_explicit, _opt_explicit_def, false))
			{
				return B1C_T_ERROR::B1C_RES_EINCOPTS;
			}
		}
		else
		if(value <= 2)
		{
			if(!st_option_check(first_run, _opt_explicit, _opt_explicit_def, true))
			{
				return B1C_T_ERROR::B1C_RES_EINCOPTS;
			}
		}
		else
		{
			return static_cast<B1C_T_ERROR>(B1_RES_EINVARG);
		}

		return B1C_T_ERROR::B1C_RES_OK;
	}
	else
	if(err != B1_RES_EEOF)
	{
		return static_cast<B1C_T_ERROR>(B1_RES_EINVSTAT);
	}

	err = st_option_set(_NOCHECK, B1_TOKEN_TYPE_IDNAME, true, &value);
	if(err == B1_RES_OK)
	{
		// omitted value corresponds to ON
		if(value == 0)
		{
			if(!st_option_check(first_run, _opt_nocheck, _opt_nocheck_def, false))
			{
				return B1C_T_ERROR::B1C_RES_EINCOPTS;
			}
		}
		else
		if(value <= 2)
		{
			if(!st_option_check(first_run, _opt_nocheck, _opt_nocheck_def, true))
			{
				return B1C_T_ERROR::B1C_RES_EINCOPTS;
			}
		}
		else
		{
			return static_cast<B1C_T_ERROR>(B1_RES_EINVARG);
		}

		return B1C_T_ERROR::B1C_RES_OK;
	}
	else
	if(err != B1_RES_EEOF)
	{
		return static_cast<B1C_T_ERROR>(B1_RES_EINVSTAT);
	}

	err = st_option_set(_INPUTDEVICE, B1_TOKEN_TYPE_DEVNAME, false, &value);
	if(err == B1_RES_OK)
	{
		if(!first_run)
		{
			if(!_opt_inputdevice_def)
			{
				return B1C_T_ERROR::B1C_RES_EINCOPTS;
			}
			_opt_inputdevice_def = false;
			_opt_inputdevice = Utils::str_toupper(B1CUtils::b1str_to_cstr(b1_tmp_buf));
			if(_global_settings.GetDevCmdsList(_global_settings.GetIoDeviceName(_opt_inputdevice)).empty())
			{
				return B1C_T_ERROR::B1C_RES_EUNKIODEV;
			}
		}
		return B1C_T_ERROR::B1C_RES_OK;
	}
	else
	if(err != B1_RES_EEOF)
	{
		return static_cast<B1C_T_ERROR>(B1_RES_EINVSTAT);
	}

	err = st_option_set(_OUTPUTDEVICE, B1_TOKEN_TYPE_DEVNAME, false, &value);
	if(err == B1_RES_OK)
	{
		if(!first_run)
		{
			if(!_opt_outputdevice_def)
			{
				return B1C_T_ERROR::B1C_RES_EINCOPTS;
			}
			_opt_outputdevice_def = false;
			_opt_outputdevice = Utils::str_toupper(B1CUtils::b1str_to_cstr(b1_tmp_buf));
			if(_global_settings.GetDevCmdsList(_global_settings.GetIoDeviceName(_opt_outputdevice)).empty())
			{
				return B1C_T_ERROR::B1C_RES_EUNKIODEV;
			}
		}
		return B1C_T_ERROR::B1C_RES_OK;
	}
	else
	if(err == B1_RES_EEOF)
	{
		return static_cast<B1C_T_ERROR>(B1_RES_EINVSTAT);
	}

	return static_cast<B1C_T_ERROR>(err);
}

B1C_T_ERROR B1FileCompiler::st_ioctl_get_symbolic_value(std::wstring &value, bool *is_numeric /*= nullptr*/)
{
	if(is_numeric != nullptr)
	{
		*is_numeric = false;
	}

	if(b1_rpn[1].flags == 0)
	{
		bool get_value = false;
		auto tflags = b1_rpn[0].flags;
		B1_T_INDEX id_off, id_len;

		if(B1_RPNREC_TEST_TYPES(tflags, B1_RPNREC_TYPE_IMM_VALUE) && !B1_RPNREC_TEST_IMM_VALUE_NULL_ARG(tflags))
		{
			if(b1_rpn[0].data.token.type & (B1_TOKEN_TYPE_NUMERIC))
			{
				get_value = true;
				id_off = b1_rpn[0].data.token.offset;
				id_len = b1_rpn[0].data.token.length;

				if(is_numeric != nullptr)
				{
					*is_numeric = true;
				}
			}
		}
		else
		if(B1_RPNREC_TEST_TYPES(tflags, B1_RPNREC_TYPE_FNVAR))
		{
			get_value = true;
			id_off = b1_rpn[0].data.id.offset;
			id_len = b1_rpn[0].data.id.length;
		}

		if(get_value)
		{
			value = Utils::str_toupper(B1CUtils::get_progline_substring(id_off, id_off + id_len));
			return B1C_T_ERROR::B1C_RES_OK;
		}
	}

	auto err = concat_strings_rpn(value);
	if(err != B1_RES_OK)
	{
		return static_cast<B1C_T_ERROR>(err);
	}
	value.pop_back();
	value.erase(0, 1);

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::st_ioctl()
{
	B1_T_ERROR err;
	std::wstring dev_name, cmd_name;

	// get device name
	err = b1_rpn_build(b1_curr_prog_line_offset, INPUT_STOP_TOKEN, &b1_curr_prog_line_offset);
	if(err != B1_RES_OK)
	{
		return static_cast<B1C_T_ERROR>(err);
	}
	if(b1_rpn[0].flags == 0)
	{
		// IOCTL statement without arguments
		return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
	}
	if(b1_curr_prog_line_offset != 0)
	{
		if(B1_T_ISCOMMA(b1_progline[b1_curr_prog_line_offset]))
		{
			b1_curr_prog_line_offset++;
		}
		else
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
		}
	}
	else
	{
		return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
	}
	auto err1 = st_ioctl_get_symbolic_value(dev_name);
	if(err1 != B1C_T_ERROR::B1C_RES_OK)
	{
		return err1;
	}

	// get command name
	err = b1_rpn_build(b1_curr_prog_line_offset, INPUT_STOP_TOKEN, &b1_curr_prog_line_offset);
	if(err != B1_RES_OK)
	{
		return static_cast<B1C_T_ERROR>(err);
	}
	if(b1_rpn[0].flags == 0)
	{
		// no command
		return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
	}
	err1 = st_ioctl_get_symbolic_value(cmd_name);
	if(err1 != B1C_T_ERROR::B1C_RES_OK)
	{
		return err1;
	}

	auto dev_real_name = _global_settings.GetIoDeviceName(dev_name);

	Settings::IoCmd cmd;
	if(!_global_settings.GetIoCmd(dev_real_name, cmd_name, cmd))
	{
		return B1C_T_ERROR::B1C_RES_EUNKDEVCMD;
	}

	if(b1_curr_prog_line_offset != 0)
	{
		if(B1_T_ISCOMMA(b1_progline[b1_curr_prog_line_offset]))
		{
			b1_curr_prog_line_offset++;
		}
		else
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
		}
	}
	else
	{
		if(cmd.accepts_data && cmd.def_val.empty())
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
		}
	}

	if(cmd.accepts_data)
	{
		bool use_def_val = false;
		std::wstring data;

		// use default value
		if(b1_curr_prog_line_offset == 0)
		{
			use_def_val = true;

			if(cmd.def_val.empty())
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
			}
		}
		else
		{
			err = b1_rpn_build(b1_curr_prog_line_offset, NULL, NULL);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}

			if(b1_rpn[0].flags == 0)
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
			}
		}

		if(use_def_val)
		{
			// do not specify the default value
			emit_command(L"IOCTL", std::vector<B1_TYPED_VALUE>({ B1_TYPED_VALUE(L"\"" + dev_name + L"\"", B1Types::B1T_STRING), B1_TYPED_VALUE(L"\"" + cmd_name + L"\"", B1Types::B1T_STRING) }));
		}
		else
		{
			if(cmd.predef_only)
			{
				err1 = st_ioctl_get_symbolic_value(data);
				if(err1 != B1C_T_ERROR::B1C_RES_OK)
				{
					return err1;
				}

				auto v = cmd.values.find(data);
				if(v == cmd.values.end())
				{
					return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
				}

				emit_command(L"IOCTL", std::vector<B1_TYPED_VALUE>({ B1_TYPED_VALUE(L"\"" + dev_name + L"\"", B1Types::B1T_STRING), B1_TYPED_VALUE(L"\"" + cmd_name + L"\"", B1Types::B1T_STRING), B1_TYPED_VALUE(L"\"" + data + L"\"", B1Types::B1T_STRING) }));
			}
			else
			{
				if(cmd.data_type == B1Types::B1T_STRING && concat_strings_rpn(data) == B1_RES_OK)
				{
					emit_command(L"IOCTL", std::vector<B1_TYPED_VALUE>({ B1_TYPED_VALUE(L"\"" + dev_name + L"\"", B1Types::B1T_STRING), B1_TYPED_VALUE(L"\"" + cmd_name + L"\"", B1Types::B1T_STRING), B1_TYPED_VALUE(data, B1Types::B1T_STRING) }));
				}
				else
				if(cmd.data_type == B1Types::B1T_LABEL)
				{
					bool is_numeric = true;
					// numeric labels are local and non-numeric ones are global and should be loaded only once
					err1 = st_ioctl_get_symbolic_value(data, &is_numeric);
					if(err1 != B1C_T_ERROR::B1C_RES_OK)
					{
						return err1;
					}

					const auto lbl_name = is_numeric ? (get_name_space_prefix() + L"__ULB_" + data) : (data + cmd.extra_data);
					_req_labels.insert(lbl_name);
					emit_command(L"IOCTL", std::vector<B1_TYPED_VALUE>({ B1_TYPED_VALUE(L"\"" + dev_name + L"\"", B1Types::B1T_STRING), B1_TYPED_VALUE(L"\"" + cmd_name + L"\"", B1Types::B1T_STRING), B1_TYPED_VALUE(L"\"" + lbl_name + L"\"", B1Types::B1T_STRING) }));
				}
				else					
				if(cmd.data_type == B1Types::B1T_VARREF)
				{
					err1 = st_ioctl_get_symbolic_value(data);
					if(err1 != B1C_T_ERROR::B1C_RES_OK)
					{
						return err1;
					}

					data += cmd.extra_data;

					emit_command(L"IOCTL", std::vector<B1_TYPED_VALUE>({ B1_TYPED_VALUE(L"\"" + dev_name + L"\"", B1Types::B1T_STRING), B1_TYPED_VALUE(L"\"" + cmd_name + L"\"", B1Types::B1T_STRING), B1_TYPED_VALUE(data, B1Types::B1T_VARREF) }));

					// save iterator to the command to correct the reference name
					auto vr = _var_refs.find(data);
					if(vr == _var_refs.end())
					{
						_var_refs.insert(std::make_pair(data, std::make_pair(L"__VAR_" + data, std::vector<iterator>({ std::prev(end()) }))));
					}
					else
					{
						vr->second.second.push_back(std::prev(end()));
					}
				}
				else					
				if(cmd.data_type == B1Types::B1T_TEXT)
				{
					err1 = st_ioctl_get_symbolic_value(data);
					if(err1 != B1C_T_ERROR::B1C_RES_OK)
					{
						return err1;
					}

					data += cmd.extra_data;

					emit_command(L"IOCTL", std::vector<B1_TYPED_VALUE>({ B1_TYPED_VALUE(L"\"" + dev_name + L"\"", B1Types::B1T_STRING), B1_TYPED_VALUE(L"\"" + cmd_name + L"\"", B1Types::B1T_STRING), B1_TYPED_VALUE(L"\"" + data + L"\"", B1Types::B1T_STRING) }));
				}
				else
				{
					// compile the expression part
					B1_CMP_EXP_TYPE exp_type;
					B1_CMP_ARG res;
					err = process_expression(end(), exp_type, res);
					if(err != B1_RES_OK)
					{
						return static_cast<B1C_T_ERROR>(err);
					}

					if(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
					{
						return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
					}

					emit_command(L"IOCTL", std::vector<B1_CMP_ARG>({ B1_CMP_ARG(L"\"" + dev_name + L"\"", B1Types::B1T_STRING), B1_CMP_ARG(L"\"" + cmd_name + L"\"", B1Types::B1T_STRING), res }));

					if(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_LOCAL)
					{
						emit_command(L"LF", res[0].value);
					}
				}
			}
		}
	}
	else
	{
		emit_command(L"IOCTL", std::vector<B1_TYPED_VALUE>({ B1_TYPED_VALUE(L"\"" + dev_name + L"\"", B1Types::B1T_STRING), B1_TYPED_VALUE(L"\"" + cmd_name + L"\"", B1Types::B1T_STRING) }));
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_let(const B1_T_CHAR **stop_tokens, B1_CMP_ARG *var_ref /*= nullptr*/)
{
	B1_T_ERROR err;
	bool stop;
	B1_T_INDEX right_exp_off, continue_offset;
	B1_TOKENDATA td;

	// look for assignment operator
	right_exp_off = b1_curr_prog_line_offset;
	continue_offset = 0;
	stop = false;

	while(!stop)
	{
		err = b1_tok_get(right_exp_off, 0, &td);
		if(err != B1_RES_OK) return err;
		right_exp_off = td.offset;
		if(td.length == 0) return B1_RES_ESYNTAX;
		// only one stop token for LET statement ("=")
		stop = !b1_t_strcmpi(LET_STOP_TOKENS[0], b1_progline + right_exp_off, td.length);
		right_exp_off += td.length;
	}

	// build RPN for the right part of the expresssion
	err = b1_rpn_build(right_exp_off, stop_tokens, &continue_offset);
	if(err != B1_RES_OK)
	{
		return err;
	}

	// compile the expression part
	B1_CMP_EXP_TYPE exp_type;
	B1_CMP_ARG res;
	err = process_expression(end(), exp_type, res);
	if(err != B1_RES_OK)
	{
		return err;
	}

	if(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
	{
		return B1_RES_ESYNTAX;
	}

	// build RPN for the left part of the expression
	right_exp_off = 0;
	err = b1_rpn_build(b1_curr_prog_line_offset, LET_STOP_TOKENS, &right_exp_off);
	if(err != B1_RES_OK)
	{
		return err;
	}

	// no assignment operation found
	if(!right_exp_off)
	{
		return B1_RES_ESYNTAX;
	}

	// compile the expression on the left side of assignment operator
	B1_CMP_EXP_TYPE exp_type1;
	B1_CMP_ARG res1;
	err = process_expression(end(), exp_type1, res1, true);
	if(err != B1_RES_OK)
	{
		return err;
	}

	if(exp_type1 != B1_CMP_EXP_TYPE::B1_CMP_ET_VAR)
	{
		return B1_RES_ESYNTAX;
	}

	if(is_const_var(res1[0].value))
	{
		return B1_RES_ETYPMISM;
	}

	if(var_ref != nullptr)
	{
		var_ref->clear();
	}

	B1_CMP_CMD cmd(_curr_line_num, _curr_line_cnt, _curr_src_file_id, _curr_src_line_id);
	cmd.type = B1_CMD_TYPE::B1_CMD_TYPE_COMMAND;
	cmd.cmd = L"=";
	cmd.args.push_back(res);
	cmd.args.push_back(res1);
	push_back(cmd);

	if(var_ref != nullptr)
	{
		*var_ref = res1;
	}

	if(res1.size() > 1)
	{
		for(auto l = res1.crbegin(); l != res1.crend() - 1; l++)
		{
			if(is_gen_local(l->value))
			{
				emit_command(L"LF", l->value);
			}
		}
	}

	if(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_LOCAL)
	{
		emit_command(L"LF", res[0].value);
	}

	b1_curr_prog_line_offset = continue_offset;

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_goto()
{
	B1_T_ERROR err;
	B1_TOKENDATA td;

	err = b1_tok_get_line_num(&b1_curr_prog_line_offset);
	if(err != B1_RES_OK)
	{
		return err;
	}

	if(b1_next_line_num == B1_T_LINE_NUM_ABSENT)
	{
		return B1_RES_ESYNTAX;
	}

	emit_command(L"JMP", get_name_space_prefix() + L"__ULB_" + std::to_wstring(b1_next_line_num));

	err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		return err;
	}

	if(td.length != 0)
	{
		return B1_RES_ESYNTAX;
	}

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_gosub()
{
	B1_T_ERROR err;
	B1_TOKENDATA td;

	err = b1_tok_get_line_num(&b1_curr_prog_line_offset);
	if(err != B1_RES_OK)
	{
		return err;
	}

	if(b1_next_line_num == B1_T_LINE_NUM_ABSENT)
	{
		return B1_RES_ESYNTAX;
	}

	emit_command(L"CALL", get_name_space_prefix() + L"__ULB_" + std::to_wstring(b1_next_line_num));

	err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		return err;
	}

	if(td.length != 0)
	{
		return B1_RES_ESYNTAX;
	}

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_dim_get_one_size(bool first_run, bool allow_TO_stop_word, bool TO_stop_word_only, std::pair<B1_CMP_ARG, B1_CMP_EXP_TYPE> &res)
{
	B1_T_ERROR err;

	// "TO" stop token must be the first in DIM_STOP_TOKENS array
	err = b1_rpn_build(b1_curr_prog_line_offset, allow_TO_stop_word ? DIM_STOP_TOKENS : (DIM_STOP_TOKENS + 1), &b1_curr_prog_line_offset);
	if(err != B1_RES_OK)
	{
		return err;
	}

	if(!b1_curr_prog_line_offset || b1_rpn[0].flags == 0)
	{
		return B1_RES_ESYNTAX;
	}

	if(TO_stop_word_only)
	{
		// TO, comma or close bracket
		auto c = b1_progline[b1_curr_prog_line_offset];

		if(c == B1_T_C_CLBRACK || B1_T_ISCOMMA(c))
		{
			return B1_RES_ESYNTAX;
		}
	}

	// do not produce code during the first run
	return first_run ? B1_RES_OK : process_expression(end(), res.second, res.first);
}

B1_T_ERROR B1FileCompiler::st_dim_get_size(bool first_run, bool range_only, std::vector<std::pair<B1_CMP_ARG, B1_CMP_EXP_TYPE>> &range)
{
	// get either dimension size or lbound
	std::pair<B1_CMP_ARG, B1_CMP_EXP_TYPE> res;
	auto err = st_dim_get_one_size(first_run, true, range_only, res);
	if(err != B1_RES_OK)
	{
		return err;
	}

	// TO, comma or close bracket
	auto c = b1_progline[b1_curr_prog_line_offset];

	if(c == B1_T_C_CLBRACK || B1_T_ISCOMMA(c))
	{
		if(!first_run)
		{
			range.push_back(std::pair<B1_CMP_ARG, B1_CMP_EXP_TYPE>(std::to_wstring(_opt_base1 ? 1 : 0), B1_CMP_EXP_TYPE::B1_CMP_ET_IMM_VAL));
			range.push_back(res);
		}
	}
	else
	{
		// get ubound
		b1_curr_prog_line_offset += 2;

		if(!first_run)
		{
			range.push_back(res);
		}

		err = st_dim_get_one_size(first_run, false, false, res);
		if(err != B1_RES_OK)
		{
			return err;
		}

		if(!first_run)
		{
			range.push_back(res);
		}
	}

	return B1_RES_OK;
}

B1C_T_ERROR B1FileCompiler::st_dim(bool first_run)
{
	B1_T_ERROR err;
	B1_T_CHAR c;
	uint8_t dimsnum;
	bool stop;
	B1_TOKENDATA td;
	B1_T_INDEX len;
	bool is_global, is_volatile, at, is_static, is_const;
	bool read_init;

	while(true)
	{
		std::wstring name;
		std::wstring address;
		std::vector<std::pair<B1_CMP_ARG, B1_CMP_EXP_TYPE>> subs;
		std::vector<B1_TYPED_VALUE> init_values;

		at = false;
		is_global = false;
		is_volatile = false;
		is_static = false;
		is_const = false;

		read_init = false;

		while(true)
		{
			// get variable name or optional variable specifier
			err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}

			len = td.length;
			b1_curr_prog_line_offset = td.offset;

			if((td.type & B1_TOKEN_TYPE_LETTERS) && len > 0)
			{
				if(!b1_t_strcmpi(_GLOBAL, b1_progline + b1_curr_prog_line_offset, len))
				{
					is_global = true;
				}
				else
				if(!b1_t_strcmpi(_VOLATILE, b1_progline + b1_curr_prog_line_offset, len))
				{
					is_volatile = true;
				}
				else
				if(!b1_t_strcmpi(_STATIC, b1_progline + b1_curr_prog_line_offset, len))
				{
					is_static = true;
				}
				else
				if(!b1_t_strcmpi(_CONST, b1_progline + b1_curr_prog_line_offset, len))
				{
					is_const = true;
				}
				else
				{
					break;
				}

				b1_curr_prog_line_offset += len;
				continue;
			}

			break;
		}

		if(is_const && is_volatile)
		{
			return B1C_T_ERROR::B1C_RES_ECNSTVOLVAR;
		}

		if(!(td.type & B1_TOKEN_TYPE_IDNAME))
		{
			return static_cast<B1C_T_ERROR>(B1_RES_EINVTOK);
		}

		name = Utils::str_toupper(B1CUtils::get_progline_substring(b1_curr_prog_line_offset, b1_curr_prog_line_offset + len));

		if(Utils::check_const_name(name))
		{
			return static_cast<B1C_T_ERROR>(B1_RES_EIDINUSE);
		}

		b1_curr_prog_line_offset += len;

		dimsnum = 0;

		// DIM [GLOBAL] [STATIC] [VOLATILE] [CONST] <var_name>[(subscript[,...])] [AS <type_name>] [AT <address>] [ = (<initializers>)][,...]
		err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}

		b1_curr_prog_line_offset = td.offset;
		len = td.length;

		if(len == 1 && b1_progline[b1_curr_prog_line_offset] == B1_T_C_OPBRACK)
		{
			b1_curr_prog_line_offset++;
			// get array dimensions
			while(true)
			{
				if(dimsnum == B1_MAX_VAR_DIM_NUM)
				{
					return static_cast<B1C_T_ERROR>(B1_RES_EWSUBSCNT);
				}

				err = st_dim_get_size(first_run, false, subs);
				if(err != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err);
				}

				dimsnum++;

				c = b1_progline[b1_curr_prog_line_offset];
				b1_curr_prog_line_offset++;

				if(B1_T_ISCOMMA(c))
				{
					continue;
				}

				if(c == B1_T_C_CLBRACK)
				{
					break;
				}
			}

			err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}

			b1_curr_prog_line_offset = td.offset;
			len = td.length;
		}

		B1Types type = B1Types::B1T_UNKNOWN;

		if(len == 0)
		{
			stop = true;
		}
		else
		if(len == 1 && B1_T_ISCOMMA(b1_progline[b1_curr_prog_line_offset]))
		{
			// continue parsing string
			stop = false;
		}
		else
		{
			// try to get optional variable type
			err = st_get_type_def(true, td, len, type, &at, &address);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}

			if(at && is_const)
			{
				return B1C_T_ERROR::B1C_RES_ECNSTADDR;
			}

			if(len == 0)
			{
				stop = true;
			}
			else
			if(len == 1 && b1_progline[b1_curr_prog_line_offset] == B1_T_C_EQ)
			{
				read_init = true;
			}
			else
			if(len == 1 && B1_T_ISCOMMA(b1_progline[b1_curr_prog_line_offset]))
			{
				// continue parsing string
				stop = false;
			}
			else
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
			}
		}

		type = Utils::get_type_by_type_spec(name, type);
		if(type == B1Types::B1T_UNKNOWN)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
		}

		if(read_init)
		{
			// read initializers
			if(!is_const)
			{
				return B1C_T_ERROR::B1C_RES_ENCNSTINIT;
			}

			b1_curr_prog_line_offset++;

			err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}

			b1_curr_prog_line_offset = td.offset;
			len = td.length;

			if(len == 0)
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
			}

			std::vector<B1Types> types;
			types.push_back(type);

			if(len == 1 && b1_progline[b1_curr_prog_line_offset] == B1_T_C_OPBRACK)
			{
				// get initializers list
				b1_curr_prog_line_offset++;

				err = st_read_data(CONST_VAL_SEPARATORS, CONST_STOP_TOKEN, &types, init_values);
				if(err != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err);
				}

				if(b1_curr_prog_line_offset == 0)
				{
					// no close bracket
					return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
				}

				b1_curr_prog_line_offset++;

				if(dimsnum == 0)
				{
					dimsnum = 1;

					subs.push_back(std::pair<B1_CMP_ARG, B1_CMP_EXP_TYPE>(std::to_wstring(_opt_base1 ? 1 : 0), B1_CMP_EXP_TYPE::B1_CMP_ET_IMM_VAL));
					subs.push_back(std::pair<B1_CMP_ARG, B1_CMP_EXP_TYPE>(std::to_wstring(init_values.size() - (_opt_base1 ? 0 : 1)), B1_CMP_EXP_TYPE::B1_CMP_ET_IMM_VAL));
				}
			}
			else
			{
				// get single initializer
				err = st_read_data(INPUT_STOP_TOKEN, INPUT_STOP_TOKEN, &types, init_values);
				if(err != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err);
				}

				if(init_values.size() != 1)
				{
					return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
				}

				if(dimsnum != 0)
				{
					// array initializers must be enclosed in brackets
					return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
				}
			}

			if(b1_curr_prog_line_offset == 0)
			{
				stop = true;
			}
			else
			{
				err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
				if(err != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err);
				}

				b1_curr_prog_line_offset = td.offset;
				len = td.length;

				if(len == 0)
				{
					stop = true;
				}
				else
				if(len == 1 && B1_T_ISCOMMA(b1_progline[b1_curr_prog_line_offset]))
				{
					// continue parsing string
					stop = false;
				}
				else
				{
					return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
				}
			}
		}
		else
		{
			if(is_const)
			{
				return B1C_T_ERROR::B1C_RES_ECNSTNOINIT;
			}
		}

		if(first_run)
		{
			if(dimsnum == 0 && is_static)
			{
				// non-subscripted variables are always static
				_warnings[b1_curr_prog_line_cnt].push_back(B1C_T_WARNING::B1C_WRN_WSTATNONSUBVAR);
				is_static = false;
			}

			if(is_const && is_static)
			{
				// CONST variables can be treated as static
				_warnings[b1_curr_prog_line_cnt].push_back(B1C_T_WARNING::B1C_WRN_WCNSTALSTAT);
				is_static = false;
			}

			// do not produce code during the first run
			auto err1 = put_var_name(name, type, dimsnum, is_global, is_volatile, at, is_static, is_const);
			if(err1 != B1C_T_ERROR::B1C_RES_OK)
			{
				return err1;
			}
		}
		else
		{
			B1_CMP_ARGS args;
			bool expl = false;

			name = get_var_name(name, expl);

			if(is_const)
			{
				std::vector<std::wstring> values;

				err = st_data_change_const_names(init_values);
				if(err != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err);
				}

				for(const auto &v: init_values)
				{
					if(v.type != B1Types::B1T_UNKNOWN && ((type == B1Types::B1T_STRING && v.type != B1Types::B1T_STRING) || (type != B1Types::B1T_STRING && v.type == B1Types::B1T_STRING)))
					{
						return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
					}

					values.push_back(v.value);
				}

				if(values.empty())
				{
					return B1C_T_ERROR::B1C_RES_ECNSTNOINIT;
				}

				auto err1 = put_const_var_init_values(name, values);
				if(err1 != B1C_T_ERROR::B1C_RES_OK)
				{
					return err1;
				}
			}

			if(!(is_const && dimsnum == 0))
			{
				if(dimsnum == 0 && is_static)
				{
					// non-subscripted variables are always static
					is_static = false;
				}

				if(is_const && is_static)
				{
					is_static = false;
				}

				// name
				args.push_back(name);
			
				// type
				args.push_back(B1_CMP_ARG(Utils::get_type_name(type), type));
				if(is_volatile || is_static || is_const)
				{
					args[1].push_back(B1_TYPED_VALUE(std::wstring(is_volatile ? L"V" : L"") + std::wstring(is_static ? L"S" : L"") + std::wstring(is_const ? L"C" : L"")));
				}

				// address
				if(at)
				{
					args.push_back(address);
				}

				for(const auto &s: subs)
				{
					if(s.second == B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
					{
						return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
					}

					args.push_back(s.first);
				}

				B1_CMP_CMD cmd(_curr_line_num, _curr_line_cnt, _curr_src_file_id, _curr_src_line_id);
				cmd.type = B1_CMD_TYPE::B1_CMD_TYPE_COMMAND;
				cmd.cmd = at ? L"MA" : L"GA";
				cmd.args = args;

				push_back(cmd);

				for(auto s = subs.crbegin(); s != subs.crend(); s++)
				{
					if(s->second == B1_CMP_EXP_TYPE::B1_CMP_ET_LOCAL)
					{
						emit_command(L"LF", s->first[0].value);
					}
				}
			}
		}

		b1_curr_prog_line_offset += len;

		if(stop)
		{
			break;
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_erase()
{
	B1_T_ERROR err;
	uint8_t type;
	B1_T_INDEX len;
	bool next;
	B1_TOKENDATA td;

	while(true)
	{
		std::wstring name;

		// get variable name
		err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
		if(err != B1_RES_OK)
		{
			return err;
		}

		type = td.type;
		b1_curr_prog_line_offset = td.offset;
		len = td.length;
		if(len == 0)
		{
			return B1_RES_ESYNTAX;
		}

		if(!(type & B1_TOKEN_TYPE_IDNAME))
		{
			return B1_RES_EINVTOK;
		}

		name = Utils::str_toupper(B1CUtils::get_progline_substring(b1_curr_prog_line_offset, b1_curr_prog_line_offset + len));

		if(Utils::check_const_name(name))
		{
			return B1_RES_EIDINUSE;
		}

		b1_curr_prog_line_offset += len;

		err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
		if(err != B1_RES_OK)
		{
			return err;
		}

		b1_curr_prog_line_offset = td.offset;
		len = td.length;
		next = (len == 1) && B1_T_ISCOMMA(*(b1_progline + b1_curr_prog_line_offset));
		if(!next && len != 0)
		{
			return B1_RES_ESYNTAX;
		}
		b1_curr_prog_line_offset++;

		bool expl = false;
		name = get_var_name(name, expl);
		if(_opt_explicit && !expl)
		{
			return B1_RES_EUNKIDENT;
		}

		emit_command(L"GF", name);

		if(!next)
		{
			break;
		}
	}

	return B1_RES_OK;
}

// get optional "AS <type_name>" clause
B1_T_ERROR B1FileCompiler::st_get_type_def(bool allow_addr, B1_TOKENDATA &td, B1_T_INDEX &len, B1Types &type, bool *addr_present /*= nullptr*/, std::wstring *address /*= nullptr*/)
{
	B1_T_ERROR err;
	bool read_type;

	read_type = true;
	type = B1Types::B1T_UNKNOWN;

	if(allow_addr)
	{
		*addr_present = false;
	}

	while(true)
	{
		if(read_type && (td.type & B1_TOKEN_TYPE_LETTERS) && !b1_t_strcmpi(_AS, b1_progline + b1_curr_prog_line_offset, len))
		{
			b1_curr_prog_line_offset += len;

			err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
			if(err != B1_RES_OK)
			{
				return err;
			}

			b1_curr_prog_line_offset = td.offset;
			len = td.length;

			type = Utils::get_type_by_name(B1CUtils::get_progline_substring(b1_curr_prog_line_offset, b1_curr_prog_line_offset + len));
			if(type == B1Types::B1T_UNKNOWN)
			{
				return B1_RES_ESYNTAX;
			}

			b1_curr_prog_line_offset += len;

			err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
			if(err != B1_RES_OK)
			{
				return err;
			}

			b1_curr_prog_line_offset = td.offset;
			len = td.length;

			read_type = false;

			continue;
		}

		if(allow_addr && (td.type & B1_TOKEN_TYPE_LETTERS) && !b1_t_strcmpi(_AT, b1_progline + b1_curr_prog_line_offset, len))
		{
			b1_curr_prog_line_offset += len;

			err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
			if(err != B1_RES_OK)
			{
				return err;
			}

			b1_curr_prog_line_offset = td.offset;
			len = td.length;

			if(td.type & (B1_TOKEN_TYPE_NUMERIC | B1_TOKEN_TYPE_IDNAME))
			{
				auto addr_tok = B1CUtils::get_progline_substring(b1_curr_prog_line_offset, b1_curr_prog_line_offset + len);

				if((td.type & B1_TOKEN_TYPE_IDNAME) && !Utils::check_const_name(addr_tok))
				{
					return B1_RES_EUNKIDENT;
				}

				*address = addr_tok;
				*addr_present = true;
			}
			else
			{
				return B1_RES_ESYNTAX;
			}

			b1_curr_prog_line_offset += len;

			err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
			if(err != B1_RES_OK)
			{
				return err;
			}

			b1_curr_prog_line_offset = td.offset;
			len = td.length;

			allow_addr = false;

			continue;
		}

		break;
	}

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_def(bool first_run)
{
	B1_T_ERROR err;
	B1_TOKENDATA td;
	B1_T_INDEX i, len;
	B1_T_CHAR c;
	std::wstring name;
	B1Types fn_type;
	std::vector<std::wstring> args;
	std::vector<B1Types> arg_types;
	uint8_t tflags;
	std::vector<B1_RPNREC> defrpn;
	const B1_RPNREC *prev_rpn;
	bool global;

	global = false;

	// get function name
	err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		return err;
	}

	b1_curr_prog_line_offset = td.offset;
	len = td.length;

	if((td.type & B1_TOKEN_TYPE_LETTERS) && len > 0 && !b1_t_strcmpi(_GLOBAL, b1_progline + b1_curr_prog_line_offset, len))
	{
		global = true;

		b1_curr_prog_line_offset += len;

		// get variable name
		err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
		if(err != B1_RES_OK)
		{
			return err;
		}

		b1_curr_prog_line_offset = td.offset;
		len = td.length;
	}


	if(!(td.type & B1_TOKEN_TYPE_IDNAME))
	{
		return B1_RES_EINVTOK;
	}

	// get name
	name = Utils::str_toupper(B1CUtils::get_progline_substring(b1_curr_prog_line_offset, b1_curr_prog_line_offset + len));

	if(Utils::check_const_name(name))
	{
		return B1_RES_EIDINUSE;
	}

	b1_curr_prog_line_offset += len;

	err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		return err;
	}

	b1_curr_prog_line_offset = td.offset;
	len = td.length;

	if(len == 1 && (td.type & B1_TOKEN_TYPE_OPERATION) && b1_progline[b1_curr_prog_line_offset] == B1_T_C_OPBRACK)
	{
		b1_curr_prog_line_offset += len;

		// read function arguments
		while(true)
		{
			err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
			if(err != B1_RES_OK)
			{
				return err;
			}

			b1_curr_prog_line_offset = td.offset;
			len = td.length;

			if(!(td.type & B1_TOKEN_TYPE_IDNAME))
			{
				return B1_RES_ESYNTAX;
			}

			if(args.size() == B1_MAX_FN_ARGS_NUM)
			{
				return B1_RES_EWRARGCNT;
			}

			// process function argument
			std::wstring arg;
			arg = Utils::str_toupper(B1CUtils::get_progline_substring(b1_curr_prog_line_offset, b1_curr_prog_line_offset + len));
			for(auto a: args)
			{
				if(a == arg)
				{
					return B1_RES_EIDINUSE;
				}
			}

			b1_curr_prog_line_offset += len;

			args.push_back(arg);

			err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
			if(err != B1_RES_OK)
			{
				return err;
			}

			b1_curr_prog_line_offset = td.offset;
			len = td.length;

			// get optional "AS <type_name>" clause
			B1Types type = B1Types::B1T_UNKNOWN;
			err = st_get_type_def(false, td, len, type);
			if(err != B1_RES_OK)
			{
				return err;
			}

			type = Utils::get_type_by_type_spec(arg, type);
			arg_types.push_back(type);
			if(arg_types.back() == B1Types::B1T_UNKNOWN)
			{
				return B1_RES_ETYPMISM;
			}

			if(len != 1 || !(td.type & B1_TOKEN_TYPE_OPERATION))
			{
				return B1_RES_ESYNTAX;
			}

			c = b1_progline[b1_curr_prog_line_offset];
			b1_curr_prog_line_offset += len;

			if(c == B1_T_C_CLBRACK)
			{
				break;
			}

			if(!B1_T_ISCOMMA(c))
			{
				return B1_RES_ESYNTAX;
			}
		}

		err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
		if(err != B1_RES_OK)
		{
			return err;
		}

		b1_curr_prog_line_offset = td.offset;
		len = td.length;
	}

	fn_type = B1Types::B1T_UNKNOWN;
	err = st_get_type_def(false, td, len, fn_type);
	if(err != B1_RES_OK)
	{
		return err;
	}

	if(len != 1 || !(td.type & B1_TOKEN_TYPE_OPERATION) || b1_progline[b1_curr_prog_line_offset] != B1_T_C_EQ)
	{
		return B1_RES_ESYNTAX;
	}

	b1_curr_prog_line_offset += len;

	// get function type
	fn_type = Utils::get_type_by_type_spec(name, fn_type);
	if(fn_type == B1Types::B1T_UNKNOWN)
	{
		return B1_RES_ETYPMISM;
	}

	if(first_run)
	{
		return add_ufn(global, name, fn_type, arg_types) ? B1_RES_OK : B1_RES_EIDINUSE;
	}
	else
	if(!global)
	{
		// forbid local and global user-defined functions with the same names
		if(_compiler.global_fn_exists(name))
		{
			return B1_RES_EIDINUSE;
		}
	}

	// build function expression RPN
	err = b1_rpn_build(b1_curr_prog_line_offset, NULL, NULL);
	if(err != B1_RES_OK)
	{
		return err;
	}

	// save function RPN
	i = 0;

	while(true)
	{
		defrpn.push_back(b1_rpn[i]);

		tflags = defrpn[i].flags;

		if(tflags == 0)
		{
			if(i == 0)
			{
				return B1_RES_ESYNTAX;
			}

			break;
		}

		if(B1_RPNREC_GET_TYPE(tflags) == B1_RPNREC_TYPE_FNVAR)
		{
			auto off = defrpn[i].data.id.offset;
			len = defrpn[i].data.id.length;

			std::wstring arg;
			arg = Utils::str_toupper(B1CUtils::get_progline_substring(off, off + len));

			for(B1_T_INDEX a = 0; a < args.size(); a++)
			{
				if(args[a] == arg)
				{
					defrpn[i].flags = B1_RPNREC_TYPE_FN_ARG | (uint8_t)(a << (B1_RPNREC_FN_ARG_INDEX_SHIFT));
					break;
				}
			}
		}

		i++;
	}

	iterator def_begin = std::prev(end());

	emit_label(L"__DEF_" + name, global);

	B1_CMP_EXP_TYPE exp_type;
	B1_CMP_ARG res;

	prev_rpn = b1_rpn;
	b1_rpn = defrpn.data();
	err = process_expression(end(), exp_type, res);
	b1_rpn = prev_rpn;

	if(err != B1_RES_OK)
	{
		return err;
	}

	if(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
	{
		return B1_RES_ESYNTAX;
	}

	B1_CMP_CMD cmd(_curr_line_num, _curr_line_cnt, _curr_src_file_id, _curr_src_line_id);
	cmd.type = B1_CMD_TYPE::B1_CMD_TYPE_COMMAND;
	cmd.cmd = L"RETVAL";
	cmd.args.push_back(res);
	cmd.args.push_back(B1_CMP_ARG(Utils::get_type_name(fn_type), fn_type));
	push_back(cmd);

	if(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_LOCAL)
	{
		emit_command(L"LF", res[0].value);
	}

	emit_command(L"RET");

	// set argument types
	for(def_begin++; def_begin != end(); def_begin++)
	{
		auto &cmd = *def_begin;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		for(auto &a: cmd.args)
		{
			for(auto &aa: a)
			{
				if(B1CUtils::is_fn_arg(aa.value))
				{
					aa.type = arg_types[B1CUtils::get_fn_arg_index(aa.value)];
				}
			}
		}
	}

	return B1_RES_OK;
}

B1C_T_ERROR B1FileCompiler::compile_simple_stmt(uint8_t stmt)
{
	B1_T_ERROR err;

	if(stmt == B1_ID_STMT_ON)
	{
		return B1C_T_ERROR::B1C_RES_ENOTIMP;
	}

	if(stmt == B1_ID_STMT_GOTO)
	{
		err = st_goto();
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}
	}

	if(stmt == B1_ID_STMT_GOSUB)
	{
		err = st_gosub();
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}
	}

	if(stmt == B1_ID_STMT_RETURN)
	{
		emit_command(L"RET");
	}

	if(stmt == B1_ID_STMT_DIM)
	{
		auto err1 = st_dim(false);
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			return err1;
		}
	}

	if(stmt == B1_ID_STMT_ERASE)
	{
		err = st_erase();
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}
	}

#ifdef B1_FEATURE_FUNCTIONS_MATH_BASIC
#ifdef B1_FRACTIONAL_TYPE_EXISTS
	if(stmt == B1_ID_STMT_RANDOMIZE)
	{
		emit_command(L"RANDOMIZE");
	}
#endif
#endif

	// DEF <fn_name>[(<arg1_name[, arg2_name, ...argN_name]>)] = <expression>
	if(stmt == B1_ID_STMT_DEF)
	{
		// process DEF statements after the main program compilation
		return static_cast<B1C_T_ERROR>(B1_RES_OK);
	}

	if(stmt == B1_ID_STMT_SET)
	{
		return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
	}

	if(stmt == B1_ID_STMT_IOCTL)
	{
		auto err1 = st_ioctl();
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			return err1;
		}
	}

	if(stmt == B1_ID_STMT_OPTION)
	{
		auto err1 = st_option(false);
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			return err1;
		}
	}

	if(stmt == B1_ID_STMT_BREAK)
	{
		err = st_break();
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}
	}

	if(stmt == B1_ID_STMT_CONTINUE)
	{
		err = st_continue();
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}
	}

	if(stmt == B1_ID_STMT_PRINT)
	{
		auto err1 = st_print();
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			return err1;
		}
	}

	if(stmt == B1_ID_STMT_INPUT)
	{
		auto err1 = st_input();
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			return err1;
		}
	}

	if(stmt == B1_ID_STMT_PUT)
	{
		auto err1 = st_put_get_trr(L"PUT", false, true);
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			return err1;
		}
	}

	if(stmt == B1_ID_STMT_GET)
	{
		auto err1 = st_put_get_trr(L"GET", true, false);
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			return err1;
		}
	}

	if(stmt == B1_ID_STMT_TRANSFER)
	{
		auto err1 = st_put_get_trr(L"TRR", true, true);
		if (err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			return err1;
		}
	}

	if(stmt == B1_ID_STMT_END)
	{
		emit_command(L"END");
	}

#ifdef B1_FEATURE_STMT_STOP
	if(stmt == B1_ID_STMT_STOP)
	{
		emit_command(L"STOP");
	}
#endif

	// implicit or explicit LET statement
	if(stmt == B1_ID_STMT_UNKNOWN || stmt == B1_ID_STMT_LET)
	{
		err = st_let(nullptr);
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}
	}

	return static_cast<B1C_T_ERROR>(B1_RES_OK);
}

B1C_T_ERROR B1FileCompiler::st_if()
{
	B1_T_ERROR err;
	std::wstring next_label;
	B1_CMP_EXP_TYPE exp_type;
	B1_CMP_ARG res;

	if(_state.first != B1_CMP_STATE::B1_CMP_STATE_ELSE)
	{
		// build RPN for the IF statement condition
		err = b1_rpn_build(b1_curr_prog_line_offset, IF_STOP_TOKENS, &b1_curr_prog_line_offset);
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}

		// no THEN clause found
		if(!b1_curr_prog_line_offset)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
		}

		if(_state.first == B1_CMP_STATE::B1_CMP_STATE_IF)
		{
			// end label
			_state.second.push_back(emit_label(true));
		}

		err = process_expression(end(), exp_type, res);
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}

		if(exp_type != B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
		}

		next_label = emit_label(true);
		emit_command(L"JF", next_label);

		// add "THEN" keyword length
		b1_curr_prog_line_offset += (B1_T_INDEX)4;
	}

	uint8_t stmt = B1_ID_STMT_ABSENT;

	err = b1_tok_stmt_init(&stmt);
	if(err != B1_RES_OK)
	{
		return static_cast<B1C_T_ERROR>(err);
	}

	if(b1_next_line_num != B1_T_LINE_NUM_ABSENT)
	{
		if(stmt != B1_ID_STMT_ABSENT)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
		}

		emit_command(L"JMP", get_name_space_prefix() + L"__ULB_" + std::to_wstring(b1_next_line_num));
	}
	else
	{
		if(	stmt == B1_ID_STMT_ABSENT || stmt == B1_ID_STMT_IF || stmt == B1_ID_STMT_ELSEIF || stmt == B1_ID_STMT_ELSE ||
			stmt == B1_ID_STMT_FOR || stmt == B1_ID_STMT_NEXT || stmt == B1_ID_STMT_WHILE || stmt == B1_ID_STMT_WEND ||
			stmt == B1_ID_STMT_OPTION || stmt == B1_ID_STMT_DEF || stmt == B1_ID_STMT_DIM || stmt == B1_ID_STMT_DATA ||
			stmt == B1_ID_STMT_END)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
		}

		auto err1 = compile_simple_stmt(stmt);
		if(err1 != static_cast<B1C_T_ERROR>(B1_RES_OK))
		{
			return err1;
		}
	}

	if(_state.first != B1_CMP_STATE::B1_CMP_STATE_ELSE)
	{
		emit_command(L"JMP", _state.second[0]);
		emit_label(next_label);
	}

	return static_cast<B1C_T_ERROR>(B1_RES_OK);
}

B1_T_ERROR B1FileCompiler::st_if_end()
{
	emit_label(_state.second[0]);

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_for()
{
	B1_T_ERROR err;
	B1_CMP_EXP_TYPE exp_type;
	B1_CMP_ARG res;
	B1_CMP_ARG ctrl_var;

	//LET own1 = limit
	//LET own2 = increment
	//LET v = initial-value

	// state: control variable, limit, increment, loop label, continue label, tmp next label, end label

	err = st_let(FOR_STOP_TOKEN1, &ctrl_var);
	if(err != B1_RES_OK)
	{
		return err;
	}

	// save control variable name and forbid using array as loop control variable
	if(ctrl_var.size() > 1)
	{
		return B1_RES_EFORSUBSVAR;
	}
	_state.second.push_back(ctrl_var[0].value);

	// no "TO" keyword
	if(b1_curr_prog_line_offset == 0)
	{
		return B1_RES_ESYNTAX;
	}

	// "TO" keyword length
	b1_curr_prog_line_offset += 2;

	err = b1_rpn_build(b1_curr_prog_line_offset, FOR_STOP_TOKEN2, &b1_curr_prog_line_offset);
	if(err != B1_RES_OK)
	{
		return err;
	}

	err = process_expression(end(), exp_type, res);
	if(err != B1_RES_OK)
	{
		return err;
	}

	if(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
	{
		return B1_RES_ESYNTAX;
	}

	if(exp_type != B1_CMP_EXP_TYPE::B1_CMP_ET_IMM_VAL && !is_gen_local(res[0].value))
	{
		std::wstring local = emit_local(B1Types::B1T_UNKNOWN);
		emit_command(L"=", std::vector<std::wstring>({ res[0].value, local }));
		res[0].value = local;
	}
	_state.second.push_back(res[0].value);

	// check for "STEP" keyword presence
	if(b1_curr_prog_line_offset != 0)
	{
		// skip "STEP" keyword
		b1_curr_prog_line_offset += 4;

		// build RPN for step expression
		err = b1_rpn_build(b1_curr_prog_line_offset, NULL, &b1_curr_prog_line_offset);
		if(err != B1_RES_OK)
		{
			return err;
		}

		// step value expression should be the last in the line
		if(b1_curr_prog_line_offset != 0)
		{
			return B1_RES_ESYNTAX;
		}

		err = process_expression(end(), exp_type, res);
		if(err != B1_RES_OK)
		{
			return err;
		}

		if(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
		{
			return B1_RES_ESYNTAX;
		}

		if(exp_type != B1_CMP_EXP_TYPE::B1_CMP_ET_IMM_VAL && !is_gen_local(res[0].value))
		{
			std::wstring local = emit_local(B1Types::B1T_UNKNOWN);
			emit_command(L"=", std::vector<std::wstring>({ res[0].value, local }));
			res[0].value = local;
		}

		_state.second.push_back(res[0].value);
	}
	else
	{
		// increment = 1
		_state.second.push_back(L"1");
	}

	//Line1 IF (v-own1) * SGN (own2) > 0 THEN line2
	// loop label, 3
	_state.second.push_back(emit_label(true));
	auto tmp_label = emit_label(true);
	// loop_stmt label, 4
	_state.second.push_back(emit_label(true));
	// continue label, 5
	_state.second.push_back(emit_label(true));
	// end label, 6
	_state.second.push_back(emit_label(true));

	//        0                 1      2          3           4                5               6
	// state: control variable, limit, increment, loop label, loop stmt label, continue label, end label
	emit_label(_state.second[3]);
	emit_command(L"<", std::vector<std::wstring>({ _state.second[2], L"0" }));
	emit_command(L"JT", tmp_label);
	emit_command(L">", std::vector<std::wstring>({ _state.second[0], _state.second[1] }));
	emit_command(L"JT", _state.second[6]);
	emit_command(L"JMP", _state.second[4]);
	emit_label(tmp_label);
	emit_command(L"<", std::vector<std::wstring>({ _state.second[0], _state.second[1] }));
	emit_command(L"JT", _state.second[6]);
	emit_label(_state.second[4]);

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_next()
{
	B1_T_ERROR err;
	B1_TOKENDATA td;

	// check NEXT statement control variable if specified
	err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		return err;
	}

	if(td.length != 0)
	{
		if(!(td.type & B1_TOKEN_TYPE_IDNAME))
		{
			return B1_RES_EINVTOK;
		}

		bool expl = false;
		if(get_var_name(Utils::str_toupper(B1CUtils::get_progline_substring(td.offset, td.offset + td.length)), expl) != _state.second[0])
		{
			return B1_RES_ENXTWOFOR;
		}
	}

	//LET v = v + own2
	//GOTO line1
	//line2 REM continued in sequence
	//        0                 1      2          3           4                5               6
	// state: control variable, limit, increment, loop label, loop stmt label, continue label, end label
	emit_label(_state.second[5]);
	
	// optimize: try to remove the equality check for some simple cases
	bool eq_check = true;

	if(B1CUtils::is_num_val(_state.second[2]) && B1CUtils::is_num_val(_state.second[1]))
	{
		int32_t n = 0;
		if(Utils::str2int32(_state.second[2], n) == B1_RES_OK && (n == 1 || n == -1))
		{
			n = 0;
			if(Utils::str2int32(_state.second[1], n) == B1_RES_OK && n >= 1 && n <= 254)
			{
				eq_check = false;
			}
		}
	}

	// the check is needed to process integer overflow, e.g. for INT I: FOR I = 0 TO 32767
	if(eq_check)
	{
		emit_command(L"==", std::vector<std::wstring>({ _state.second[0], _state.second[1] }));
		emit_command(L"JT", _state.second[6]);
	}

	emit_command(L"+", std::vector<std::wstring>({ _state.second[0], _state.second[2], _state.second[0] }));
	emit_command(L"JMP", _state.second[3]);
	emit_label(_state.second[6]);

	for(auto l = _state.second.crbegin(); l != _state.second.crend(); l++)
	{
		if(is_gen_local(*l))
		{
			emit_command(L"LF", *l);
		}
	}

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_read_data(const B1_T_CHAR **value_separators, const B1_T_CHAR **stop_tokens, const std::vector<B1Types> *types, std::vector<B1_TYPED_VALUE> &args)
{
	B1_TOKENDATA td;
	B1_T_ERROR err;
	B1Types type;
	int i = 0;

	while(b1_curr_prog_line_offset != 0)
	{
		// build RPN
		err = b1_rpn_build(b1_curr_prog_line_offset, value_separators, &b1_curr_prog_line_offset);
		if(err != B1_RES_OK)
		{
			return err;
		}

		std::wstring value;
		bool const_name;

		// try to get simple numeric values
		B1_CMP_EXP_TYPE res_type;
		B1_CMP_ARG res;

		if(correct_rpn(res_type, res, false) && ((B1CUtils::is_num_val(res[0].value) || Utils::check_const_name(res[0].value))))
		{
			value = res[0].value;
			const_name = !B1CUtils::is_num_val(res[0].value);

			// get type
			if(types != nullptr && types->size() > 0)
			{
				if((*types)[i] == B1Types::B1T_STRING)
				{
					return B1_RES_ETYPMISM;
				}

				type = (*types)[i];

				i++;
				if(i == types->size())
				{
					i = 0;
				}
			}
			else
			if(const_name)
			{
				type = Utils::get_const_type(value);
			}
			else
			{
				type = Utils::get_type_by_type_spec(value, B1Types::B1T_INT);
				if(type == B1Types::B1T_UNKNOWN)
				{
					return B1_RES_ETYPMISM;
				}
			}

			if(!const_name)
			{
				int32_t n;

				err = Utils::str2int32(value, n);
				if(err != B1_RES_OK)
				{
					return err;
				}

				Utils::Utils::correct_int_value(n, type);
				value = std::to_wstring(n);
			}
		}
		else
		if(concat_strings_rpn(value) == B1_RES_OK)
		{
			// not a numeric value
			if(types != nullptr && types->size() > 0)
			{
				if((*types)[i] != B1Types::B1T_STRING)
				{
					return B1_RES_ETYPMISM;
				}

				i++;
				if(i == types->size())
				{
					i = 0;
				}
			}

			type = B1Types::B1T_STRING;
		}
		else
		{
			// return B1_RES_ESYNTAX;
			// temporary solution: allow using const variables as other const variables initial values (using expressions is not implemented at the moment)
			if(B1_RPNREC_GET_TYPE(b1_rpn[0].flags) == B1_RPNREC_TYPE_FNVAR && b1_rpn[1].flags == 0)
			{
				type = B1Types::B1T_UNKNOWN;
				B1_T_INDEX id_off = (*(b1_rpn)).data.id.offset;
				B1_T_INDEX id_len = (*(b1_rpn)).data.id.length;
				value = B1CUtils::get_progline_substring(id_off, id_off + id_len);

				// get type
				if(types != nullptr && types->size() > 0)
				{
					// check types compatibility
					type = Utils::get_type_by_type_spec(value, (*types)[i]);
					if(type == B1Types::B1T_UNKNOWN)
					{
						return B1_RES_ETYPMISM;
					}
					type = (*types)[i];

					i++;
					if(i == types->size())
					{
						i = 0;
					}
				}
			}
			else
			{
				return B1_RES_ESYNTAX;
			}
		}

		args.push_back(B1_TYPED_VALUE(value, type));

		if(b1_curr_prog_line_offset != 0)
		{
			// get the separator or stop token
			err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
			if(err != B1_RES_OK)
			{
				return err;
			}

			if(td.length == 0)
			{
				return B1_RES_ESYNTAX;
			}

			if(stop_tokens != nullptr)
			{
				// stop reading values if a stop token is reached
				for(auto st = stop_tokens; *st != nullptr; st++)
				{
					if(b1_t_strcmpi(*st, b1_progline + td.offset, td.length) == 0)
					{
						return B1_RES_OK;
					}
				}
			}

			bool is_separator = false;

			for(auto vs = value_separators; *vs != nullptr; vs++)
			{
				if(b1_t_strcmpi(*vs, b1_progline + td.offset, td.length) == 0)
				{
					is_separator = true;
					break;
				}
			}

			if(is_separator)
			{
				b1_curr_prog_line_offset = td.offset + td.length;
			}
			else
			{
				return B1_RES_ESYNTAX;
			}
		}
	}

	return B1_RES_OK;
}

// change user names of constants used as data initializers to their generated names
// e.g.: DAT,NS1,C1,C2,C3 -> DAT,NS1,NS1::__VAR_C1,NS1::__VAR_C2,NS1::__VAR_C3
B1_T_ERROR B1FileCompiler::st_data_change_const_names(std::vector<B1_TYPED_VALUE> &args)
{
	for(auto &v: args)
	{
		if(!B1CUtils::is_imm_val(v.value) && (v.type == B1Types::B1T_UNKNOWN || !Utils::check_const_name(v.value)))
		{
			bool expl = false;
			v.value = get_var_name(v.value, expl);
			if(!expl || !is_const_var(v.value) || get_var_dim(v.value) != 0)
			{
				return B1_RES_ESYNTAX;
			}
			if(v.type == B1Types::B1T_UNKNOWN)
			{
				v.type = get_var_type(v.value);
			}
		}
	}

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_data()
{
	B1_T_ERROR err;
	B1_TOKENDATA td;
	B1_T_INDEX len;
	B1_T_CHAR c;
	B1Types type;
	std::vector<B1Types> types;
	auto init_prog_line_offset = b1_curr_prog_line_offset;

	err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		return err;
	}
	b1_curr_prog_line_offset = td.offset;
	len = td.length;

	if(len == 1 && (td.type & B1_TOKEN_TYPE_OPERATION) && b1_progline[b1_curr_prog_line_offset] == B1_T_C_OPBRACK)
	{
		// open bracket, get data types
		while(true)
		{
			err = b1_tok_get(b1_curr_prog_line_offset + len, 0, &td);
			if(err != B1_RES_OK)
			{
				return err;
			}
			b1_curr_prog_line_offset = td.offset;
			len = td.length;

			// get type
			type = Utils::get_type_by_name(B1CUtils::get_progline_substring(b1_curr_prog_line_offset, b1_curr_prog_line_offset + len));
			if(type == B1Types::B1T_UNKNOWN)
			{
				return B1_RES_ESYNTAX;
			}

			types.push_back(type);

			err = b1_tok_get(b1_curr_prog_line_offset + len, 0, &td);
			if(err != B1_RES_OK)
			{
				return err;
			}
			b1_curr_prog_line_offset = td.offset;
			len = td.length;

			if(len != 1 || !(td.type & B1_TOKEN_TYPE_OPERATION))
			{
				return B1_RES_ESYNTAX;
			}

			c = b1_progline[b1_curr_prog_line_offset];
			if(c == B1_T_C_CLBRACK)
			{
				b1_curr_prog_line_offset += 1;
				break;
			}
			if(c == B1_T_C_COMMA)
			{
				continue;
			}
			return B1_RES_ESYNTAX;
		}
	}
	else
	{
		// no types defined, restore prog. line position and read data
		b1_curr_prog_line_offset = init_prog_line_offset;
	}

	std::vector<B1_TYPED_VALUE> args;

	// read data
	err = st_read_data(INPUT_STOP_TOKEN, nullptr, &types, args);
	if(err != B1_RES_OK)
	{
		return err;
	}

	err = st_data_change_const_names(args);
	if(err != B1_RES_OK)
	{
		return err;
	}

	// put namespace name
	args.insert(args.begin(), B1_TYPED_VALUE(_curr_name_space));

	emit_command(L"DAT", args);

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_read()
{
	B1_T_ERROR err;

	while(b1_curr_prog_line_offset != 0)
	{
		// build RPN
		err = b1_rpn_build(b1_curr_prog_line_offset, INPUT_STOP_TOKEN, &b1_curr_prog_line_offset);
		if(err != B1_RES_OK)
		{
			return err;
		}
		if(b1_rpn[0].flags == 0)
		{
			// READ statement without arguments
			return B1_RES_ESYNTAX;
		}


		// compile the expression
		B1_CMP_EXP_TYPE exp_type;
		B1_CMP_ARG res;
		err = process_expression(end(), exp_type, res, true);
		if(err != B1_RES_OK)
		{
			return err;
		}

		if(exp_type != B1_CMP_EXP_TYPE::B1_CMP_ET_VAR)
		{
			return B1_RES_ESYNTAX;
		}

		if(is_const_var(res[0].value))
		{
			return B1_RES_ETYPMISM;
		}

		B1_CMP_CMD cmd(_curr_line_num, _curr_line_cnt, _curr_src_file_id, _curr_src_line_id);
		cmd.type = B1_CMD_TYPE::B1_CMD_TYPE_COMMAND;
		cmd.cmd = L"READ";
		// put namespace name
		cmd.args.push_back(B1_CMP_ARG(_curr_name_space));
		cmd.args.push_back(res);
		push_back(cmd);

		if(res.size() > 1)
		{
			for(auto l = res.crbegin(); l != res.crend() - 1; l++)
			{
				if(is_gen_local(l->value))
				{
					emit_command(L"LF", l->value);
				}
			}
		}

		if(b1_curr_prog_line_offset != 0)
		{
			if(B1_T_ISCOMMA(b1_progline[b1_curr_prog_line_offset]))
			{
				b1_curr_prog_line_offset++;
			}
			else
			{
				return B1_RES_ESYNTAX;
			}
		}
	}

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_restore()
{
	B1_T_ERROR err;

	err = b1_tok_get_line_num(&b1_curr_prog_line_offset);
	if(err != B1_RES_OK)
	{
		return err;
	}

	std::vector<B1_TYPED_VALUE> args;

	// put namespace name
	args.push_back(B1_TYPED_VALUE(_curr_name_space));

	if(b1_next_line_num != B1_T_LINE_NUM_ABSENT)
	{
		args.push_back(B1_TYPED_VALUE(get_name_space_prefix() + L"__ULB_" + std::to_wstring(b1_next_line_num)));
	}

	emit_command(L"RST", args);

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_while()
{
	B1_T_ERROR err;

	// state: loop label, end label

	// build RPN for WHILE expression
	err = b1_rpn_build(b1_curr_prog_line_offset, NULL, &b1_curr_prog_line_offset);
	if(err != B1_RES_OK)
	{
		return err;
	}

	if(b1_curr_prog_line_offset != 0)
	{
		return B1_RES_ESYNTAX;
	}

	auto loop_label = emit_label();

	_state.second.push_back(loop_label);

	// evaluate WHILE expression
	B1_CMP_EXP_TYPE exp_type;
	B1_CMP_ARG res;
	err = process_expression(end(), exp_type, res);
	if(err != B1_RES_OK)
	{
		return err;
	}

	if(exp_type != B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
	{
		return B1_RES_ESYNTAX;
	}

	auto end_label = emit_label(true);

	_state.second.push_back(end_label);

	emit_command(L"JF", end_label);

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_wend()
{
	emit_command(L"JMP", _state.second[0]);
	emit_label(_state.second[1]);

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_continue()
{
	bool if_stmt = _state.first == B1_CMP_STATE::B1_CMP_STATE_IF || _state.first == B1_CMP_STATE::B1_CMP_STATE_ELSEIF || _state.first == B1_CMP_STATE::B1_CMP_STATE_ELSE;

	if(if_stmt && _state_stack.size() < 1)
	{
		return B1_RES_ENOTINLOOP;
	}

	const auto &state = if_stmt ? _state_stack.back() : _state;

	if(state.first == B1_CMP_STATE::B1_CMP_STATE_FOR)
	{
		emit_command(L"JMP", state.second[5]);
	}
	else
	if(state.first == B1_CMP_STATE::B1_CMP_STATE_WHILE)
	{
		emit_command(L"JMP", state.second[0]);
	}
	else
	{
		return B1_RES_ENOTINLOOP;
	}

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::st_break()
{
	bool if_stmt = _state.first == B1_CMP_STATE::B1_CMP_STATE_IF || _state.first == B1_CMP_STATE::B1_CMP_STATE_ELSEIF || _state.first == B1_CMP_STATE::B1_CMP_STATE_ELSE;

	if(if_stmt && _state_stack.size() < 1)
	{
		return B1_RES_ENOTINLOOP;
	}

	const auto &state = if_stmt ? _state_stack.back() : _state;

	if(state.first == B1_CMP_STATE::B1_CMP_STATE_FOR)
	{
		emit_command(L"JMP", state.second[6]);
	}
	else
	if(state.first == B1_CMP_STATE::B1_CMP_STATE_WHILE)
	{
		emit_command(L"JMP", state.second[1]);
	}
	else
	{
		return B1_RES_ENOTINLOOP;
	}

	return B1_RES_OK;
}

B1C_T_ERROR B1FileCompiler::st_print()
{
	B1C_T_ERROR err;
	bool next_print_zone, newline;

	next_print_zone = false;
	newline = true;

	std::wstring dev_name;

	err = read_device_name(std::vector({ std::wstring(B1C_DEV_OPT_TXT), std::wstring(B1C_DEV_OPT_OUT) }), true, dev_name);
	if(err != B1C_T_ERROR::B1C_RES_OK)
	{
		return err;
	}

	// process PRINT statement arguments
	for(;; b1_curr_prog_line_offset++)
	{
		// build RPN for every PRINT statement argument
		err = static_cast<B1C_T_ERROR>(b1_rpn_build(b1_curr_prog_line_offset, PRINT_STOP_TOKENS, &b1_curr_prog_line_offset));
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			return err;
		}

		if(next_print_zone)
		{
			// go to the next print zone
			emit_command(L"OUT", std::vector<std::wstring>({ dev_name, L"TAB" }));
			back().args[1].push_back(B1_TYPED_VALUE(L"0"));

			next_print_zone = false;
		}

		// no RPN: empty expression was specified to b1_rpn_build function
		if(b1_rpn[0].flags == 0)
		{
			break;
		}

		newline = true;

		// get next print zone flag
		next_print_zone = (b1_curr_prog_line_offset != 0) && B1_T_ISCOMMA(b1_progline[b1_curr_prog_line_offset]);

		// compile the expression part
		B1_CMP_EXP_TYPE exp_type;
		B1_CMP_ARG res;
		err = static_cast<B1C_T_ERROR>(process_expression(end(), exp_type, res));
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			return err;
		}

		if(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
		}

		if(!(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_IMM_VAL && res[0].value.empty()))
		{
			std::wstring cmd;

			// process special PRINT functions (TAB and SPC)
			if(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_LOCAL && back().cmd == L"=" && back().args[0].size() == 2 && (back().args[0][0].value == L"TAB" || back().args[0][0].value == L"SPC"))
			{
				cmd = back().args[0][0].value;
				back().args[0][0] = back().args[0][1];
				back().args[0].pop_back();

				emit_command(L"OUT", std::vector<std::wstring>({ dev_name, cmd }));
				back().args[1].push_back(B1_TYPED_VALUE(res[0].value));
			}
			else
			{
				emit_command(L"OUT", std::vector<std::wstring>({ dev_name, res[0].value }));
			}

			if(exp_type == B1_CMP_EXP_TYPE::B1_CMP_ET_LOCAL)
			{
				emit_command(L"LF", res[0].value);
			}
		}

		if(!b1_curr_prog_line_offset)
		{
			break;
		}

		newline = false;
	}

	if(newline)
	{
		emit_command(L"OUT", std::vector<std::wstring>({ dev_name, L"NL"}));
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::st_input()
{
	B1C_T_ERROR err;
	bool get_prompt = true;
	bool first = true;

	std::wstring dev_name;

	err = read_device_name(std::vector({ std::wstring(B1C_DEV_OPT_TXT), std::wstring(B1C_DEV_OPT_IN) }), false, dev_name);
	if(err != B1C_T_ERROR::B1C_RES_OK)
	{
		return err;
	}

	auto rep_label = emit_label();

	while(b1_curr_prog_line_offset != 0)
	{
		// build RPN
		err = static_cast<B1C_T_ERROR>(b1_rpn_build(b1_curr_prog_line_offset, INPUT_STOP_TOKEN, &b1_curr_prog_line_offset));
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			return err;
		}
		if(b1_rpn[0].flags == 0)
		{
			// INPUT statement without arguments
			return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
		}

		if(get_prompt)
		{
			// test for optional prompt string
			std::wstring prompt;

			if(concat_strings_rpn(prompt) == B1_RES_OK)
			{
				if(b1_curr_prog_line_offset == 0)
				{
					// INPUT with prompt string only is not allowed
					return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
				}
			}
			else
			{
				prompt = L"\"" + B1CUtils::b1str_to_cstr(_PROMPT) + L"\"";
				get_prompt = false;
			}

			if(prompt != L"\"\"")
			{
				emit_command(L"OUT", std::vector<std::wstring>({ dev_name, prompt }));
			}
		}

		if(first)
		{
			emit_command(L"SET", std::vector<std::wstring>({ L"ERR", L"0" }));
			first = false;
		}

		if(!get_prompt)
		{
			// compile the expression
			B1_CMP_EXP_TYPE exp_type;
			B1_CMP_ARG res;

			err = static_cast<B1C_T_ERROR>(process_expression(end(), exp_type, res, true));
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			if(exp_type != B1_CMP_EXP_TYPE::B1_CMP_ET_VAR)
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
			}

			if(is_const_var(res[0].value))
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
			}

			emit_command(L"IN", { B1_CMP_ARG(dev_name), res });

			if(res.size() > 1)
			{
				for(auto l = res.crbegin(); l != res.crend() - 1; l++)
				{
					if(is_gen_local(l->value))
					{
						emit_command(L"LF", l->value);
					}
				}
			}

			emit_command(L"ERR", { B1_CMP_ARG(L""), rep_label });
		}

		get_prompt = false;

		if(b1_curr_prog_line_offset != 0)
		{
			if(B1_T_ISCOMMA(b1_progline[b1_curr_prog_line_offset]))
			{
				b1_curr_prog_line_offset++;
			}
			else
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::st_read_range(std::vector<std::pair<B1_CMP_ARG, B1_CMP_EXP_TYPE>> &range)
{
	B1_TOKENDATA td;
	auto saved_line_offset = b1_curr_prog_line_offset;

	auto err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		b1_curr_prog_line_offset = saved_line_offset;
		return static_cast<B1C_T_ERROR>(err);
	}
	if(td.length == 0 || !(td.type & B1_TOKEN_TYPE_IDNAME))
	{
		b1_curr_prog_line_offset = saved_line_offset;
		return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
	}

	auto name = Utils::str_toupper(B1CUtils::get_progline_substring(td.offset, td.offset + td.length));
	auto fn = get_fn(name);
	if(fn != nullptr)
	{
		b1_curr_prog_line_offset = saved_line_offset;
		return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
	}

	bool expl = true;
	name = get_var_name(name, expl);

	b1_curr_prog_line_offset = td.offset + td.length;
	err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		b1_curr_prog_line_offset = saved_line_offset;
		return static_cast<B1C_T_ERROR>(err);
	}
	if(td.length == 0 || !(td.type & B1_TOKEN_TYPE_OPERATION) || b1_progline[b1_curr_prog_line_offset] != B1_T_C_OPBRACK)
	{
		b1_curr_prog_line_offset = saved_line_offset;
		return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
	}

	b1_curr_prog_line_offset = td.offset + td.length;
	err = st_dim_get_size(true, true, range);
	if(err != B1_RES_OK)
	{
		b1_curr_prog_line_offset = saved_line_offset;
		return static_cast<B1C_T_ERROR>(err);
	}

	if(b1_progline[b1_curr_prog_line_offset] != B1_T_C_CLBRACK)
	{
		b1_curr_prog_line_offset = saved_line_offset;
		return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
	}

	b1_curr_prog_line_offset = td.offset + td.length;
	err = st_dim_get_size(false, true, range);
	if(err != B1_RES_OK)
	{
		return B1C_T_ERROR::B1C_RES_ERANGSNTX;
	}

	if(b1_progline[b1_curr_prog_line_offset] != B1_T_C_CLBRACK)
	{
		return B1C_T_ERROR::B1C_RES_ERANGSNTX;
	}

	b1_curr_prog_line_offset++;

	if(!expl || get_var_type(name) != B1Types::B1T_BYTE || get_var_dim(name) != 1)
	{
		return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
	}

	err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		b1_curr_prog_line_offset = saved_line_offset;
		return static_cast<B1C_T_ERROR>(err);
	}

	if(td.length == 0)
	{
		b1_curr_prog_line_offset = 0;
	}
	else
	{
		b1_curr_prog_line_offset = td.offset;
	}

	range[0].first.insert(range[0].first.begin(), B1_TYPED_VALUE(name, B1Types::B1T_BYTE));
	range[0].second = B1_CMP_EXP_TYPE::B1_CMP_ET_VAR;

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::read_device_name(const std::vector<std::wstring> &dev_opts, bool allow_devname_only, std::wstring &dev_name)
{
	B1_TOKENDATA td;
	auto saved_line_offset = b1_curr_prog_line_offset;

	dev_name.clear();

	// read optional device name
	auto err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		return static_cast<B1C_T_ERROR>(err);
	}

	if(td.length == 0)
	{
		if(!allow_devname_only)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
		}
	}
	else
	if(td.type & B1_TOKEN_TYPE_DEVNAME)
	{
		b1_curr_prog_line_offset = td.offset + td.length;

		dev_name = Utils::str_toupper(B1CUtils::get_progline_substring(td.offset, b1_curr_prog_line_offset));

		err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}

		if(td.length != 1 || !B1_T_ISCOMMA(b1_progline[td.offset]))
		{
			if(!(allow_devname_only && td.length == 0))
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
			}
		}
		else
		{
			b1_curr_prog_line_offset = td.offset + td.length;
		}
	}
	
	if(dev_name.empty())
	{
		// no device name specified
		b1_curr_prog_line_offset = saved_line_offset;

		bool in_dev = std::find(dev_opts.cbegin(), dev_opts.cend(), B1C_DEV_OPT_IN) != dev_opts.cend();
		bool out_dev = std::find(dev_opts.cbegin(), dev_opts.cend(), B1C_DEV_OPT_OUT) != dev_opts.cend();

		if(!_opt_inputdevice_def && in_dev)
		{
			dev_name = _opt_inputdevice;
		}

		if(!_opt_outputdevice_def && out_dev)
		{
			dev_name = _opt_outputdevice;
		}

		if(in_dev && out_dev && !dev_name.empty())
		{
			if(_opt_inputdevice != _opt_outputdevice)
			{
				return B1C_T_ERROR::B1C_RES_EINCOPTS;
			}
		}
	}

	if(!dev_name.empty())
	{
		auto dopts = _global_settings.GetDeviceOptions(dev_name);
		if(dopts == nullptr)
		{
			return B1C_T_ERROR::B1C_RES_EWDEVTYPE;
		}
		for(auto &o: dev_opts)
		{
			if(dopts->find(o) == dopts->cend())
			{
				return B1C_T_ERROR::B1C_RES_EWDEVTYPE;
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

// now the only USING clause option is XOR
B1C_T_ERROR B1FileCompiler::st_read_using_clause(B1_CMP_ARGS &args, iterator pos)
{
	B1_TOKENDATA td;

	args.clear();

	auto err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
	if(err != B1_RES_OK)
	{
		return static_cast<B1C_T_ERROR>(err);
	}
	else
	if(td.length != 0 && (td.type & B1_TOKEN_TYPE_IDNAME) && !b1_t_strcmpi(_USING, b1_progline + td.offset, td.length))
	{
		b1_curr_prog_line_offset = td.offset + td.length;
		
		err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}

		if(td.length != 0 && (td.type & B1_TOKEN_TYPE_IDNAME) && !b1_t_strcmpi(_XOR, b1_progline + td.offset, td.length))
		{
			b1_curr_prog_line_offset = td.offset + td.length;

			err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}
			if(td.length != 1 || b1_progline[td.offset] != B1_T_C_OPBRACK)
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
			}
			b1_curr_prog_line_offset = td.offset + td.length;

			bool read_second_val = false;
			B1_CMP_EXP_TYPE res_type1 = B1_CMP_EXP_TYPE::B1_CMP_ET_UNKNOWN, res_type2 = B1_CMP_EXP_TYPE::B1_CMP_ET_UNKNOWN;

			// read the first value: USING XOR(<value1>[,[<value2>]])
			err = b1_tok_get(b1_curr_prog_line_offset, 0, &td);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}

			if(td.length == 1 && b1_progline[td.offset] == B1_T_C_COMMA)
			{
				// the first value is omitted: USING XOR(,<value2>)
				b1_curr_prog_line_offset = td.offset + td.length;
				args.push_back(B1_CMP_ARG(L""));
				read_second_val = true;
			}
			else
			if(td.length == 1 && b1_progline[td.offset] == B1_T_C_CLBRACK)
			{
				// error, no values at all: USING XOR()
				return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
			}
			else
			{
				auto err1 = b1_rpn_build(b1_curr_prog_line_offset, USING_SEPARATORS, &b1_curr_prog_line_offset);
				if(err1 != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err1);
				}
				if(b1_curr_prog_line_offset == 0)
				{
					return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
				}

				args.push_back(B1_CMP_ARG());
				err1 = process_expression(pos, res_type1, args.back());
				if(err1 != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err1);
				}
				if(res_type1 == B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
				{
					return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
				}

				if(b1_progline[b1_curr_prog_line_offset] == B1_T_C_CLBRACK)
				{
					// one value for both input and output data: USING XOR(<value>)
					b1_curr_prog_line_offset++;
					args.push_back(args.back());
				}
				else
				{
					// put the first value: USING XOR(<value1>, [<value2>])
					b1_curr_prog_line_offset++;
					read_second_val = true;
				}
			}

			if(read_second_val)
			{
				auto err1 = b1_rpn_build(b1_curr_prog_line_offset, USING_SEPARATORS, &b1_curr_prog_line_offset);
				if(err1 != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err1);
				}
				if(b1_curr_prog_line_offset == 0 || b1_progline[b1_curr_prog_line_offset] != B1_T_C_CLBRACK)
				{
					return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
				}

				if(b1_rpn[0].flags == 0)
				{
					// the second value is omitted: USING XOR(<value1>,)
					if(args.back()[0].value.empty())
					{
						return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
					}
					args.push_back(B1_CMP_ARG(L""));
				}
				else
				{
					// process the second value
					args.push_back(B1_CMP_ARG());
					err1 = process_expression(pos, res_type2, args.back());
					if(err1 != B1_RES_OK)
					{
						return static_cast<B1C_T_ERROR>(err1);
					}
					if(res_type2 == B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
					{
						return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
					}
				}

				b1_curr_prog_line_offset++;
			}

			if(res_type2 == B1_CMP_EXP_TYPE::B1_CMP_ET_LOCAL)
			{
				emit_command(L"LF", args.back()[0].value);
			}
			if(res_type1 == B1_CMP_EXP_TYPE::B1_CMP_ET_LOCAL)
			{
				emit_command(L"LF", (*std::prev(args.end(), 2))[0].value);
			}
		}
		else
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
		}
	}
	else
	{
		return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

// PUT [#<dev_name>, ] <value1> [, <value2>, ..., <valueN>][USING XOR([IN_VALUE][,][OUT_VALUE])]: is_input = false, is_output = true
// GET [#<dev_name>, ] <var1> [, <var2>, ..., <varN>][USING XOR([IN_VALUE][,][OUT_VALUE])]: is_input = true, is_output = false
// TRANSFER [#<dev_name>, ] <var1> [, <var2>, ..., <varN>][USING XOR([IN_VALUE][,][OUT_VALUE])]: is_input = true, is_output = true
B1C_T_ERROR B1FileCompiler::st_put_get_trr(const std::wstring &cmd_name, bool is_input, bool is_output)
{
	std::wstring dev_name;
	auto dev_opts = std::vector({ std::wstring(B1C_DEV_OPT_BIN) });
	std::vector<iterator> cmds;
	iterator start = std::prev(end());

	if(is_input)
	{
		dev_opts.push_back(B1C_DEV_OPT_IN);
	}
	if(is_output)
	{
		dev_opts.push_back(B1C_DEV_OPT_OUT);
	}

	auto err = read_device_name(dev_opts, false, dev_name);
	if(err != B1C_T_ERROR::B1C_RES_OK)
	{
		return err;
	}

	// read other statement arguments
	while(b1_curr_prog_line_offset != 0)
	{
		// first try to read array range
		std::vector<std::pair<B1_CMP_ARG, B1_CMP_EXP_TYPE>> range;
		auto err = st_read_range(range);
		if(err == static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM) || err == B1C_T_ERROR::B1C_RES_ERANGSNTX)
		{
			return err;
		}

		if(err == B1C_T_ERROR::B1C_RES_OK)
		{
			if(range[1].second == B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
			}

			if(is_input)
			{
				if(is_const_var(range[0].first[0].value))
				{
					return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
				}
			}

			std::wstring count_local = emit_local(B1Types::B1T_UNKNOWN);
			emit_command(L"-", { range[1].first, B1_CMP_ARG(range[0].first[1].value, range[0].first[1].type), B1_CMP_ARG(count_local) });
			emit_command(L"+", { count_local, L"1", count_local });
			emit_command(cmd_name, std::vector<B1_CMP_ARG>({ dev_name, range[0].first, count_local }));
			cmds.push_back(std::prev(end()));
			emit_command(L"LF", count_local);

			if(range[1].second == B1_CMP_EXP_TYPE::B1_CMP_ET_LOCAL)
			{
				emit_command(L"LF", range[1].first[0].value);
			}

			if(is_gen_local(range[0].first[1].value))
			{
				emit_command(L"LF", range[0].first[1].value);
			}
		}
		else
		{
			// build RPN
			auto err1 = b1_rpn_build(b1_curr_prog_line_offset, PUT_GET_STOP_TOKENS, &b1_curr_prog_line_offset);
			if(err1 != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err1);
			}

			// no RPN: empty expression was specified to b1_rpn_build function
			if(b1_rpn[0].flags == 0)
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
			}

			// compile the expression
			range.push_back(std::make_pair(B1_CMP_ARG(), B1_CMP_EXP_TYPE::B1_CMP_ET_UNKNOWN));
			err1 = process_expression(end(), range[0].second, range[0].first, is_input);
			if(err1 != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err1);
			}

			if(range[0].first[0].value.empty())
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
			}

			if(is_input)
			{
				if(range[0].second != B1_CMP_EXP_TYPE::B1_CMP_ET_VAR)
				{
					return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
				}

				if(is_const_var(range[0].first[0].value))
				{
					return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
				}

				emit_command(cmd_name, std::vector<B1_CMP_ARG>({ dev_name, range[0].first }));
				cmds.push_back(std::prev(end()));

				if(range[0].first.size() > 1)
				{
					for(auto l = range[0].first.crbegin(); l != range[0].first.crend() - 1; l++)
					{
						if(is_gen_local(l->value))
						{
							emit_command(L"LF", l->value);
						}
					}
				}
			}
			else
			{
				if(range[0].second == B1_CMP_EXP_TYPE::B1_CMP_ET_LOGICAL)
				{
					return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
				}

				emit_command(cmd_name, std::vector<std::wstring>({ dev_name, range[0].first[0].value }));
				cmds.push_back(std::prev(end()));

				if(range[0].second == B1_CMP_EXP_TYPE::B1_CMP_ET_LOCAL)
				{
					emit_command(L"LF", range[0].first[0].value);
				}
			}
		}

		if(b1_curr_prog_line_offset != 0)
		{
			if(B1_T_ISCOMMA(b1_progline[b1_curr_prog_line_offset]))
			{
				b1_curr_prog_line_offset++;
			}
			else
			{
				// check for USING keyword presence
				B1_CMP_ARGS args;
				err = st_read_using_clause(args, std::next(start));
				if(err != B1C_T_ERROR::B1C_RES_OK)
				{
					return err;
				}

				for(auto &cmd: cmds)
				{
					B1_CMP_ARGS a;

					if(!args[0][0].value.empty())
					{
						a.push_back(B1_CMP_ARG(L"XORIN", B1Types::B1T_BYTE));
						a[0].push_back(args[0][0]);
						emit_command(L"XARG", cmd, a);
					}
					
					a.clear();
					if(!args[1][0].value.empty())
					{
						a.push_back(B1_CMP_ARG(L"XOROUT", B1Types::B1T_BYTE));
						a[0].push_back(args[1][0]);
						emit_command(L"XARG", cmd, a);
					}
				}

				break;
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1_CMP_CMDS::const_iterator B1FileCompiler::find_LF(B1_CMP_CMDS::const_iterator lacmd, B1_CMP_CMDS::const_iterator intlfcmd, bool& intlf_found)
{
	const auto &la = *lacmd;

	intlf_found = false;

	for(lacmd++; lacmd != cend(); lacmd++)
	{
		const auto &lf = *lacmd;

		if(B1CUtils::is_label(lf))
		{
			continue;
		}

		if(lf.cmd == L"LF")
		{
			if(lacmd == intlfcmd)
			{
				intlf_found = true;
				continue;
			}

			if(la.args[0][0].value == lf.args[0][0].value)
			{
				return lacmd;
			}
		}
	}

	return cend();
}

void B1FileCompiler::fix_LA_LF_order()
{
	std::stack<iterator> lastmt;
	iterator tmp, lf;

	for(auto i = begin(); i != end(); i++)
	{
		if(i->cmd == L"LA")
		{
			lastmt.push(i);
			continue;
		}

		if(i->cmd == L"LF")
		{
			if(i->args[0][0].value == lastmt.top()->args[0][0].value)
			{
				lastmt.pop();
			}
			else
			{
				get_LA_LF(lastmt.top(), end(), tmp, lf);
				insert(std::next(lf), *i);
				tmp = std::prev(i);
				erase(i);
				i = tmp;
			}
		}
	}
}

B1C_T_ERROR B1FileCompiler::remove_unused_labels(bool &changed)
{
	std::set<std::wstring> used_labels;
	// index and name of every label
	std::vector<std::pair<const_iterator, std::wstring>> labels;

	changed = false;

	// find and remove unused labels
	for(auto cmdi = cbegin(); cmdi != cend(); cmdi++)
	{
		// do not remove function definitions
		if(B1CUtils::is_label(*cmdi) && !B1CUtils::is_def_fn(*cmdi))
		{
			labels.push_back(std::pair<const_iterator, std::wstring>(cmdi, cmdi->cmd));
		}
		else
		if(cmdi->cmd == L"JMP" || cmdi->cmd == L"JF" || cmdi->cmd == L"JT" || cmdi->cmd == L"CALL" || cmdi->cmd == L"ERR" || (cmdi->cmd == L"RST" && cmdi->args.size() > 1))
		{
			used_labels.insert(cmdi->args[(cmdi->cmd == L"RST" || cmdi->cmd == L"ERR") ? 1 : 0][0].value);
		}
	}

	used_labels.insert(_req_labels.cbegin(), _req_labels.cend());

	for(auto l = labels.crbegin(); l != labels.crend(); l++)
	{
		if(used_labels.find(l->second) == used_labels.end())
		{
			erase(l->first);
			changed = true;
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::remove_duplicate_labels(bool &changed)
{
	// find and remove duplicates
	std::vector<std::vector<std::pair<const_iterator, bool>>> label_ranges;

	changed = false;

	decltype(label_ranges)::value_type range;

	for(auto cmdi = cbegin(); cmdi != cend(); cmdi++)
	{
		// do not take into account functions
		if(B1CUtils::is_label(*cmdi) && !B1CUtils::is_def_fn(*cmdi))
		{
			range.push_back(std::make_pair(cmdi, (_req_labels.find(cmdi->cmd) != _req_labels.cend())));
		}
		else
		{
			if(!range.empty())
			{
				label_ranges.push_back(range);
				range.clear();
			}
		}
	}
	if(!range.empty())
	{
		label_ranges.push_back(range);
		range.clear();
	}

	if(!label_ranges.empty())
	{
		std::map<std::wstring, std::wstring> toreplace;

		// build list of labels to replace
		for(const auto &lr: label_ranges)
		{
			// find label to keep
			bool keep_all = true;
			auto lk = lr.cend();

			for(auto l = lr.cbegin(); l != lr.cend(); l++)
			{
				if(l->second && lk == lr.cend())
				{
					lk = l;
				}
				if(!l->second)
				{
					keep_all = false;
				}
			}
			if(lk == lr.cend())
			{
				lk = std::prev(lr.cend());
			}

			if(!keep_all)
			{
				for(auto l = lr.cbegin(); l != lr.cend(); l++)
				{
					if(l == lk || l->second)
					{
						continue;
					}
					toreplace[l->first->cmd] = lk->first->cmd;
				}
			}
		}

		// replace labels marked for deletion
		for(auto &cmd: *this)
		{
			if(B1CUtils::is_label(cmd))
			{
				continue;
			}

			if(cmd.cmd == L"JMP" || cmd.cmd == L"JF" || cmd.cmd == L"JT" || cmd.cmd == L"CALL" || cmd.cmd == L"ERR" || (cmd.cmd == L"RST" && cmd.args.size() > 1))
			{
				int i = (cmd.cmd == L"RST" || cmd.cmd == L"ERR") ? 1 : 0;
				auto rep = toreplace.find(cmd.args[i][0].value);
				if(rep != toreplace.end())
				{
					cmd.args[i][0].value = rep->second;
				}
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

bool B1FileCompiler::is_udef_used(const B1_TYPED_VALUE &val)
{
	auto fn = get_fn(val);
	if(fn != nullptr && !fn->isstdfn)
	{
		return true;
	}

	return false;
}

bool B1FileCompiler::is_udef_used(const B1_CMP_ARG &arg)
{
	auto fn = get_fn(arg);
	if(fn != nullptr && !fn->isstdfn)
	{
		return true;
	}

	for(const auto &aa: arg)
	{
		if(is_udef_used(aa))
		{
			return true;
		}
	}

	return false;
}

bool B1FileCompiler::is_udef_used(const B1_CMP_CMD &cmd)
{
	if(B1CUtils::is_label(cmd))
	{
		return false;
	}

	if(cmd.cmd == L"GA")
	{
		for(auto a = cmd.args.begin() + 2; a != cmd.args.end(); a++)
		{
			if(is_udef_used(*a))
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"RETVAL")
	{
		return is_udef_used(cmd.args[0]);
	}

	if(cmd.cmd == L"OUT")
	{
		return is_udef_used(cmd.args[1]);
	}

	if(cmd.cmd == L"IN")
	{
		return is_udef_used(cmd.args[1]);
	}

	if(cmd.cmd == L"SET")
	{
		return is_udef_used(cmd.args[1]);
	}

	if(cmd.cmd == L"IOCTL")
	{
		return (cmd.args.size() > 2) ? is_udef_used(cmd.args[2]) : false;
	}

	if(cmd.cmd == L"READ")
	{
		return is_udef_used(cmd.args[1]);
	}

	if(cmd.cmd == L"GET" || cmd.cmd == L"PUT" || cmd.cmd == L"TRR")
	{
		return (cmd.args.size() == 2) ? is_udef_used(cmd.args[1]) : (is_udef_used(cmd.args[1]) || is_udef_used(cmd.args[2]));
	}

	if(cmd.cmd == L"XARG")
	{
		return is_udef_used(cmd.args[0]);
	}


	if(cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_un_ops)
		{
			if(cmd.cmd == op)
			{
				for(const auto &a: cmd.args)
				{
					if(is_udef_used(a))
					{
						return true;
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
					if(is_udef_used(a))
					{
						return true;
					}

					return false;
				}
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
					if(is_udef_used(a))
					{
						return true;
					}

					return false;
				}
			}
		}
	}

	return false;
}

bool B1FileCompiler::is_volatile_used(const B1_CMP_ARG &arg)
{
	for(const auto &aa: arg)
	{
		if(!B1CUtils::is_imm_val(aa.value) && is_volatile_var(aa.value))
		{
			return true;
		}
	}

	return false;
}

bool B1FileCompiler::is_volatile_used(const B1_CMP_CMD &cmd)
{
	if(B1CUtils::is_label(cmd))
	{
		return false;
	}

	if(cmd.cmd == L"GA")
	{
		for(auto a = cmd.args.begin() + 2; a != cmd.args.end(); a++)
		{
			if(is_volatile_used(*a))
			{
				return true;
			}
		}

		return false;
	}

	if(cmd.cmd == L"RETVAL")
	{
		return is_volatile_used(cmd.args[0]);
	}

	if(cmd.cmd == L"OUT")
	{
		return is_volatile_used(cmd.args[1]);
	}

	if(cmd.cmd == L"IN")
	{
		return is_volatile_used(cmd.args[1]);
	}

	if(cmd.cmd == L"SET")
	{
		return is_volatile_used(cmd.args[1]);
	}

	if(cmd.cmd == L"IOCTL")
	{
		return (cmd.args.size() > 2) ? is_volatile_used(cmd.args[2]) : false;
	}

	if(cmd.cmd == L"READ")
	{
		return is_volatile_used(cmd.args[1]);
	}

	if(cmd.cmd == L"GET" || cmd.cmd == L"PUT" || cmd.cmd == L"TRR")
	{
		return (cmd.args.size() == 2) ? is_volatile_used(cmd.args[1]) : (is_volatile_used(cmd.args[1]) || is_volatile_used(cmd.args[2]));
	}

	if(cmd.cmd == L"XARG")
	{
		return is_volatile_used(cmd.args[0]);
	}

	if(cmd.args.size() == 2)
	{
		for(auto &op: B1CUtils::_un_ops)
		{
			if(cmd.cmd == op)
			{
				for(const auto &a: cmd.args)
				{
					if(is_volatile_used(a))
					{
						return true;
					}
				}

				return false;
			}
		}

		for(auto &op: B1CUtils::_log_ops)
		{
			if(cmd.cmd == op)
			{
				for (const auto &a: cmd.args)
				{
					if(is_volatile_used(a))
					{
						return true;
					}

					return false;
				}
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
					if(is_volatile_used(a))
					{
						return true;
					}

					return false;
				}
			}
		}
	}

	return false;
}

B1C_T_ERROR B1FileCompiler::remove_duplicate_assigns(bool &changed)
{
	changed = false;

	for(auto i = cbegin(); i != cend(); i++)
	{
		const auto &cmd =*i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(is_volatile_used(cmd))
		{
			continue;
		}

		const B1_CMP_ARG *dstarg = nullptr;

		if(B1CUtils::is_un_op(cmd))
		{
			dstarg = &cmd.args[1];
		}
		else
		if(B1CUtils::is_bin_op(cmd))
		{
			dstarg = &cmd.args[2];
		}
		else
		{
			continue;
		}

		std::set<std::wstring> jumps;
		std::set<std::wstring> labels;

		for(auto j = std::next(i); j != cend(); j++)
		{
			const auto &cmd1 = *j;

			if(B1CUtils::is_label(cmd1))
			{
				labels.insert(cmd1.cmd);
				continue;
			}

			if(cmd1.cmd == L"JMP" || cmd1.cmd == L"JT" || cmd1.cmd == L"JF" || cmd1.cmd == L"ERR")
			{
				jumps.insert(cmd1.args[cmd1.cmd == L"ERR" ? 1 : 0][0].value);
			}

			if(cmd1.cmd == L"CALL" || cmd1.cmd == L"END" || cmd1.cmd == L"RET")
			{
				break;
			}

			bool is_local = false;

			if(dstarg->size() > 1)
			{
				// forbid deleting assignment to a subscripted variable if its subscripts are variables
				auto a = dstarg->begin() + 1;
				for(; a != dstarg->end(); a++)
				{
					if(!B1CUtils::is_imm_val(a->value))
					{
						break;
					}
				}
				if(a != dstarg->end())
				{
					break;
				}

				// forbid deleting assignment to a subscripted variable if the variable is used somewhere else
				// =,10,A(5)
				// =,A(I),B   ->  do not delete the first line because I can be equal to 5
				// =,15,A(5)
				if(B1CUtils::is_src(cmd1, (*dstarg)[0].value))
				{
					break;
				}
			}
			else
			{
				is_local = is_gen_local((*dstarg)[0].value);
			}

			if(B1CUtils::arg_is_src(cmd1, *dstarg))
			{
				break;
			}

			//no user defined functions calls (functions can use dstarg)
			// =,10,A
			// =,DOSOMETHING(B),C  ->  do not delete the first line because DOSOMETHING fn can use it
			// =,15,A
			if(!is_local && is_udef_used(cmd1))
			{
				break;
			}

			if(B1CUtils::arg_is_dst(cmd1, *dstarg, is_local))
			{
				if(std::includes(labels.cbegin(), labels.cend(), jumps.cbegin(), jumps.cend()))
				{
					erase(i--);
					changed = true;
				}
				break;
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::remove_self_assigns(bool &changed)
{
	changed = false;

	// remove assignments like "=,<var>,<the_same_var>"
	for(auto i = cbegin(); i != cend(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(cmd.cmd == L"=" && cmd.args[0] == cmd.args[1])
		{
			if(is_volatile_used(cmd))
			{
				continue;
			}

			// subscripts should not be functions
			bool fn_used = false;

			for(auto a = cmd.args[0].begin() + 1; a != cmd.args[0].end(); a++)
			{
				if(fn_exists(a->value))
				{
					fn_used = true;
					break;
				}
			}

			if(!fn_used)
			{
				erase(i--);
				changed = true;
			}

			continue;
		}
	}

	// optimize assignments like
	// =,<var1>,<var2>
	// =,<var2>,<var1>
	for(auto i = cbegin(); i != cend(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		auto j = i;
		j++;
		if(j == cend())
		{
			break;
		}

		auto &cmd1 = *j;

		if(	(cmd.cmd == L"=" && cmd.args[0].size() == 1 && cmd.args[1].size() == 1) && 
			(cmd1.cmd == L"=" && cmd1.args[0].size() == 1 && cmd1.args[1].size() == 1) &&
			(cmd.args[0][0].value == cmd1.args[1][0].value && cmd.args[1][0].value == cmd1.args[0][0].value))
		{
			if(	(cmd.args[0][0].type == B1Types::B1T_STRING && cmd.args[1][0].type != B1Types::B1T_STRING) ||
				(cmd.args[0][0].type != B1Types::B1T_STRING && cmd.args[1][0].type == B1Types::B1T_STRING))
			{
				continue;
			}

			if(	(cmd.args[1][0].type == B1Types::B1T_BYTE && cmd.args[0][0].type != B1Types::B1T_BYTE) ||
				(cmd.args[1][0].type != B1Types::B1T_LONG && cmd.args[0][0].type == B1Types::B1T_LONG))
			{
				continue;
			}

			if(is_volatile_used(cmd))
			{
				continue;
			}

			if(fn_exists(cmd.args[0][0].value) || fn_exists(cmd.args[1][0].value))
			{
				continue;
			}

			erase(j);
			changed = true;

			continue;
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::remove_jumps(bool &changed)
{
	changed = false;

	// remove all commands between JMP, RET or END and the next label
	for(auto i = cbegin(); i != cend(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(cmd.cmd == L"JMP" || cmd.cmd == L"RET" || cmd.cmd == L"END")
		{
			for(i++; i != cend(); i++)
			{
				auto &cmd1 = *i;

				if(B1CUtils::is_label(cmd1) || cmd1.cmd == L"LA" || cmd1.cmd == L"LF")
				{
					break;
				}

				if(!(cmd1.cmd == L"DAT" || cmd1.cmd == L"DEF" || cmd1.cmd == L"MA" || cmd1.cmd == L"NS" || cmd1.cmd == L"END" ||
					 (cmd1.cmd == L"GA" && cmd1.args[1].size() > 1) || B1CUtils::is_log_op(cmd1)
					))
				{
					erase(i--);
					changed = true;
				}
			}

			if(i == cend())
			{
				break;
			}
		}
	}

	// remove jumps on the next commmand
	for(auto i = cbegin(); i != cend(); i++)
	{
		auto &cmd = *i;

		if(cmd.cmd == L"JMP" || cmd.cmd == L"JF" || cmd.cmd == L"JT")
		{
			auto nxti = std::next(i);
			if(nxti == cend())
			{
				break;
			}

			auto &cmd1 = *nxti;
			if(B1CUtils::is_label(cmd1) && cmd.args[0][0].value == cmd1.cmd)
			{
				erase(i);
				i = nxti;

				changed = true;
			}
		}
	}

	// JF,l1
	// JMP(JT),l2  ->   JT,l2
	// :l1              :l1
	//
	// JT,l1
	// JMP(JF),l2  ->   JF,l2
	//:l1              :l1
	for(auto i = begin(); i != end(); i++)
	{
		auto &cmd = *i;

		if(cmd.cmd == L"JF" || cmd.cmd == L"JT")
		{
			auto nxti = std::next(i);
			if(nxti == end())
			{
				break;
			}
			auto &cmd1 = *nxti;

			if((cmd1.cmd == L"JMP" || cmd1.cmd == L"JF" || cmd1.cmd == L"JT") &&
				cmd.cmd != cmd1.cmd)
			{
				nxti = std::next(nxti);
				if(nxti == end())
				{
					break;
				}
				auto &cmd2 = *nxti;

				if(B1CUtils::is_label(cmd2) && cmd.args[0][0].value == cmd2.cmd)
				{
					cmd1.cmd = (cmd.cmd == L"JF") ? L"JT" : L"JF";
					erase(i);
					i = nxti;

					changed = true;
				}
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

// remove compare operation, convert JT/JF to JMP and remove JF/JT
void B1FileCompiler::remove_compare_op(B1_CMP_CMDS::iterator i, bool is_true)
{
	// remove compare operation, convert JT/JF to JMP and remove JF/JT
	erase(i++);

	for(auto j = i; j != end(); j++)
	{
		auto &cmd = *j;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if((!is_true && cmd.cmd == L"JF") || (is_true && cmd.cmd == L"JT"))
		{
			cmd.cmd = L"JMP";
			continue;
		}

		if((!is_true && cmd.cmd == L"JT") || (is_true && cmd.cmd == L"JF"))
		{
			erase(j--);
			continue;
		}

		if(B1CUtils::is_log_op(cmd) || cmd.cmd == L"JMP" || cmd.cmd == L"END" || cmd.cmd == L"RET" || cmd.cmd == L"ERR")
		{
			break;
		}
	}
}

B1C_T_ERROR B1FileCompiler::remove_redundant_comparisons(bool &changed)
{
	changed = false;

	// remove unused compare operations (no JF or JT after logical operator)
	for(auto i = cbegin(); i != cend(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(B1CUtils::is_log_op(cmd) && !is_volatile_used(cmd))
		{
			for(auto j = std::next(i); j != cend(); j++)
			{
				auto &cmd1 = *j;

				if(B1CUtils::is_label(cmd1))
				{
					continue;
				}

				if(cmd1.cmd == L"JT" || cmd1.cmd == L"JF")
				{
					break;
				}

				if(B1CUtils::is_log_op(cmd1) || cmd1.cmd == L"JMP" || cmd1.cmd == L"END" || cmd1.cmd == L"RET" || cmd1.cmd == L"ERR" || cmd1.cmd == L"RETVAL")
				{
					erase(i--);

					changed = true;
					break;
				}
			}
		}
	}

	// optimize comparisons of the same variables (e.g. A(I) is always equal to A(I)) and evaluate comparison of immediate numeric values (e.g. 2 > 1)
	for(auto i = begin(); i != end(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(B1CUtils::is_log_op(cmd) && !is_volatile_used(cmd))
		{
			bool eq = false;
			bool lt = false;
			bool lt_comp = false;

			if(cmd.args[0].size() == 1 && cmd.args[1].size() == 1 && B1CUtils::is_num_val(cmd.args[0][0].value) && B1CUtils::is_num_val(cmd.args[1][0].value))
			{
				// compare numeric values
				int32_t n1, n2;

				if(Utils::str2int32(cmd.args[0][0].value, n1) == B1_RES_OK && Utils::str2int32(cmd.args[1][0].value, n2) == B1_RES_OK)
				{
					eq = n1 == n2;
					lt = n1 < n2;
					lt_comp = true;
				}
				else
				{
					eq = cmd.args[0] == cmd.args[1];
				}
			}
			else
			{
				eq = cmd.args[0] == cmd.args[1];
			}

			bool is_true = false;

			if(cmd.cmd == L"==")
			{
				if(lt_comp)
				{
					is_true = eq;
				}
				else
				{
					if(eq)
					{
						is_true = true;
					}
					else
					{
						continue;
					}
				}
			}
			else
			if(cmd.cmd == L"<>")
			{
				if(lt_comp)
				{
					is_true = !eq;
				}
				else
				{
					if(eq)
					{
						is_true = false;
					}
					else
					{
						continue;
					}
				}
			}
			else
			if(cmd.cmd == L">")
			{
				if(lt_comp)
				{
					is_true = !(eq || lt);
				}
				else
				{
					if(eq)
					{
						is_true = false;
					}
					else
					{
						continue;
					}
				}
			}
			else
			if(cmd.cmd == L"<")
			{
				if(lt_comp)
				{
					is_true = lt;
				}
				else
				{
					if(eq)
					{
						is_true = false;
					}
					else
					{
						continue;
					}
				}
			}
			else
			if(cmd.cmd == L">=")
			{
				if(lt_comp)
				{
					is_true = !lt;
				}
				else
				{
					if(eq)
					{
						is_true = true;
					}
					else
					{
						continue;
					}
				}
			}
			else
			if(cmd.cmd == L"<=")
			{
				if(lt_comp)
				{
					is_true = lt || eq;
				}
				else
				{
					if(eq)
					{
						is_true = true;
					}
					else
					{
						continue;
					}
				}
			}

			remove_compare_op(i--, is_true);
			changed = true;
		}
	}

	// remove unnecessary comparisons of unsigned variables to negative and zero values 
	for(auto i = begin(); i != end(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(B1CUtils::is_log_op(cmd) && !is_volatile_used(cmd))
		{
			int32_t n;
			// numeric is on the left side of the expression
			bool lnum = false;
			B1Types vtype;
			int32_t min, max;
			bool proceed = false;

			if(cmd.args[0].size() == 1 && B1CUtils::is_num_val(cmd.args[0][0].value) && Utils::str2int32(cmd.args[0][0].value, n) == B1_RES_OK && !B1CUtils::is_imm_val(cmd.args[1][0].value) && cmd.args[1][0].type != B1Types::B1T_STRING)
			{
				Utils::Utils::correct_int_value(n, cmd.args[0][0].type);
				lnum = true;
				vtype = cmd.args[1][0].type;
				proceed = true;
			}
			else
			if(cmd.args[1].size() == 1 && B1CUtils::is_num_val(cmd.args[1][0].value) && Utils::str2int32(cmd.args[1][0].value, n) == B1_RES_OK && !B1CUtils::is_imm_val(cmd.args[0][0].value) && cmd.args[0][0].type != B1Types::B1T_STRING)
			{
				Utils::Utils::correct_int_value(n, cmd.args[1][0].type);
				vtype = cmd.args[0][0].type;
				proceed = true;
			}

			if(proceed)
			{
				if(vtype == B1Types::B1T_LONG)
				{
					min = INT32_MIN;
					max = INT32_MAX;
				}
				else
				if(vtype == B1Types::B1T_INT)
				{
					min = -32768;
					max = 32767;
				}
				else
				if(vtype == B1Types::B1T_WORD)
				{
					min = 0;
					max = 65535;
				}
				else
				{
					min = 0;
					max = 255;
				}

				if(n >= max)
				{
					if((!lnum && cmd.cmd == L">") || (lnum && cmd.cmd == L"<"))
					{
						remove_compare_op(i--, false);
						changed = true;
						continue;
					}

					if((!lnum && cmd.cmd == L"<=") || (lnum && cmd.cmd == L">="))
					{
						remove_compare_op(i--, true);
						changed = true;
						continue;
					}

					if((!lnum && cmd.cmd == L">=") || (lnum && cmd.cmd == L"<="))
					{
						if(n == max)
						{
							cmd.cmd = L"==";
							i--;
						}
						else
						{
							remove_compare_op(i--, false);
						}
						
						changed = true;
						continue;
					}

					if((!lnum && cmd.cmd == L"<") || (lnum && cmd.cmd == L">"))
					{
						if(n == max)
						{
							cmd.cmd = L"<>";
						}
						else
						{
							remove_compare_op(i--, true);
						}

						changed = true;
						continue;
					}
				}

				if(n <= min)
				{
					if((!lnum && cmd.cmd == L">=") || (lnum && cmd.cmd == L"<="))
					{
						remove_compare_op(i--, true);
						changed = true;
						continue;
					}

					if((!lnum && cmd.cmd == L"<") || (lnum && cmd.cmd == L">"))
					{
						remove_compare_op(i--, false);
						changed = true;
						continue;
					}

					if((!lnum && cmd.cmd == L">") || (lnum && cmd.cmd == L"<"))
					{
						if(n == min)
						{
							cmd.cmd = L"<>";
							i--;
						}
						else
						{
							remove_compare_op(i--, true);
						}
						
						changed = true;
						continue;
					}

					if((!lnum && cmd.cmd == L"<=") || (lnum && cmd.cmd == L">="))
					{
						if(n == min)
						{
							cmd.cmd = L"==";
							i--;
						}
						else
						{
							remove_compare_op(i--, false);
						}

						changed = true;
						continue;
					}
				}

				if(n > max)
				{
					if(cmd.cmd == L"==" || cmd.cmd == L"<>")
					{
						remove_compare_op(i--, cmd.cmd == L"<>");
						changed = true;
						continue;
					}
				}

				if(n < min)
				{
					if(cmd.cmd == L"==" || cmd.cmd == L"<>")
					{
						remove_compare_op(i--, cmd.cmd == L"<>");
						changed = true;
						continue;
					}
				}
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

// does not take into account value type, just changes its sign
// -,<imm_val>,var  ->  =,-<imm_val>,var
B1C_T_ERROR B1FileCompiler::replace_unary_minus(bool &changed)
{
	changed = false;

	for(auto &cmd: *this)
	{
		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		// get rid from unary minus
		// -,10,A  ->  =,-10,A
		if(cmd.cmd == L"-" && cmd.args.size() == 2 && B1CUtils::is_num_val(cmd.args[0][0].value))
		{
			cmd.cmd = L"=";
			if(cmd.args[0][0].value.find(L"-") == 0)
			{
				cmd.args[0][0].value.erase(0, 1);
			}
			else
			{
				cmd.args[0][0].value.insert(0, L"-");
			}

			changed = true;
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

// -,<imm_val>,var  ->  =,-<imm_val>,var
B1C_T_ERROR B1FileCompiler::eval_unary_ops(bool &changed)
{
	changed = false;

	for(auto &cmd: *this)
	{
		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		// get rid from unary minus and NOT operations
		// -,10,A  ->  =,-10,A
		if((cmd.cmd == L"-" || cmd.cmd == L"!") && cmd.args.size() == 2 && B1CUtils::is_num_val(cmd.args[0][0].value))
		{
			if(cmd.cmd == L"-")
			{
				// if type is absent just change the sign
				if(cmd.args[0][0].type == B1Types::B1T_UNKNOWN)
				{
					cmd.cmd = L"=";
					if(cmd.args[0][0].value.find(L"-") == 0)
					{
						cmd.args[0][0].value.erase(0, 1);
					}
					else
					{
						cmd.args[0][0].value.insert(0, L"-");
					}
				}
				// if value type is present process the value as numeric
				else
				{
					int32_t n;

					auto err = Utils::str2int32(cmd.args[0][0].value, n);
					if(err != B1_RES_OK)
					{
						return static_cast<B1C_T_ERROR>(err);
					}

					Utils::correct_int_value(n, cmd.args[0][0].type);
					n = -n;
					Utils::correct_int_value(n, cmd.args[0][0].type);

					cmd.cmd = L"=";
					cmd.args[0][0].value = std::to_wstring(n);
				}
			}
			else
			{
				// evaluate binary NOT only if value type is specified
				if(cmd.args[0][0].type == B1Types::B1T_UNKNOWN)
				{
					continue;
				}

				int32_t n;

				auto err = Utils::str2int32(cmd.args[0][0].value, n);
				if(err != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err);
				}

				Utils::correct_int_value(n, cmd.args[0][0].type);
				n = ~n;
				Utils::correct_int_value(n, cmd.args[0][0].type);

				cmd.cmd = L"=";
				cmd.args[0][0].value = std::to_wstring(n);
			}

			changed = true;
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

void B1FileCompiler::set_to_init_value_arg(B1_CMP_ARG &arg, bool is_dst, const std::map<std::wstring, std::pair<bool, std::wstring>> &vars, bool init, bool &changed)
{
	for(auto aa = arg.begin(); aa != arg.end(); aa++)
	{
		// do not process subscripted variables
		if(aa == arg.begin() && (is_dst || arg.size() > 1))
		{
			continue;
		}

		if(is_volatile_var(aa->value))
		{
			continue;
		}

		auto v = vars.find(aa->value);
		if(init)
		{
			if(v != vars.end())
			{
				if(v->second.first)
				{
					if(aa->type == B1Types::B1T_STRING && v->second.second.front() != L'\"')
					{
						aa->value = L"\"" + v->second.second + L"\"";
					}
					else
					{
						aa->value = v->second.second;
					}
					changed = true;
				}
			}
			else
			{
				auto type = get_var_type(aa->value);
				if(type != B1Types::B1T_UNKNOWN && !is_mem_var_name(aa->value))
				{
					aa->value = (type == B1Types::B1T_STRING) ? L"\"\"" : L"0";
					changed = true;
				}
			}
		}
		else
		{
			if(v != vars.end())
			{
				if(v->second.first)
				{
					if(aa->type == B1Types::B1T_STRING && v->second.second.front() != L'\"')
					{
						aa->value = L"\"" + v->second.second + L"\"";
					}
					else
					{
						aa->value = v->second.second;
					}
					changed = true;
				}
			}
		}
	}
}

// replaces variables not in vars set with their initial values
std::wstring B1FileCompiler::set_to_init_value(B1_CMP_CMD &cmd, const std::map<std::wstring, std::pair<bool, std::wstring>> &vars, bool init, bool &changed)
{
	if(B1CUtils::is_label(cmd))
	{
		return L"";
	}

	if(B1CUtils::is_inline_asm(cmd))
	{
		return L"";
	}

	if(cmd.cmd == L"GA" || cmd.cmd == L"MA")
	{
		for(auto a = cmd.args.begin() + (cmd.cmd == L"GA" ? 2 : 3); a != cmd.args.end(); a++)
		{
			set_to_init_value_arg(*a, false, vars, init, changed);
		}

		return L"";
	}

	if(cmd.cmd == L"RETVAL")
	{
		set_to_init_value_arg(cmd.args[0], false, vars, init, changed);
		return L"";
	}

	if(cmd.cmd == L"IN" || cmd.cmd == L"READ")
	{
		set_to_init_value_arg(cmd.args[1], true, vars, init, changed);
		if(cmd.args[1].size() == 1)
		{
			return cmd.args[1][0].value;
		}
		return L"";
	}

	if(cmd.cmd == L"OUT" || cmd.cmd == L"SET")
	{
		set_to_init_value_arg(cmd.args[1], false, vars, init, changed);
		return L"";
	}

	if(cmd.cmd == L"GET" || cmd.cmd == L"PUT" || cmd.cmd == L"TRR")
	{
		bool is_dst = (cmd.cmd != L"PUT");
		set_to_init_value_arg(cmd.args[1], is_dst, vars, init, changed);
		if(cmd.args.size() != 2)
		{
			set_to_init_value_arg(cmd.args[2], false, vars, init, changed);
		}

		if(is_dst && cmd.args[1].size() == 1)
		{
			return cmd.args[1][0].value;
		}

		return L"";
	}

	if(cmd.cmd == L"XARG")
	{
		set_to_init_value_arg(cmd.args[0], false, vars, init, changed);
		return L"";
	}

	if(cmd.cmd == L"IOCTL")
	{
		if(cmd.args.size() > 2 && cmd.args[2][0].type != B1Types::B1T_VARREF)
		{
			set_to_init_value_arg(cmd.args[2], false, vars, init, changed);
		}

		return L"";
	}

	if(B1CUtils::is_un_op(cmd))
	{
		set_to_init_value_arg(cmd.args[0], false, vars, init, changed);
		set_to_init_value_arg(cmd.args[1], true, vars, init, changed);
		if(cmd.args[1].size() == 1)
		{
			return cmd.args[1][0].value;
		}
		return L"";
	}

	if(B1CUtils::is_bin_op(cmd))
	{
		set_to_init_value_arg(cmd.args[0], false, vars, init, changed);
		set_to_init_value_arg(cmd.args[1], false, vars, init, changed);
		set_to_init_value_arg(cmd.args[2], true, vars, init, changed);
		if(cmd.args[2].size() == 1)
		{
			return cmd.args[2][0].value;
		}
		return L"";
	}

	if(B1CUtils::is_log_op(cmd))
	{
		set_to_init_value_arg(cmd.args[0], false, vars, init, changed);
		set_to_init_value_arg(cmd.args[1], false, vars, init, changed);
		return L"";
	}

	return L"";
}

// remove excessive GA, GF and =,0,<var>
B1C_T_ERROR B1FileCompiler::reuse_imm_values(bool init, bool &changed)
{
	std::map<std::wstring, std::pair<bool, std::wstring>> modified_vars;

	changed = false;

	auto i = begin();
	while(i != end())
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd) || cmd.cmd == L"CALL" || cmd.cmd == L"END" || cmd.cmd == L"RET" || cmd.cmd == L"INT")
		{
			modified_vars.clear();
			init = false;

			i++;
			continue;
		}

		if((cmd.cmd == L"GA" || cmd.cmd == L"GF") && !is_volatile_var(cmd.args[0][0].value) && !is_mem_var_name(cmd.args[0][0].value) && get_var_dim(cmd.args[0][0].value) == 0)
		{
			auto type = get_var_type(cmd.args[0][0].value);

			if(is_const_var(cmd.args[0][0].value))
			{
				if(cmd.cmd == L"GF")
				{
					return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
				}
			}
			else
			if(init)
			{
				if(modified_vars.find(cmd.args[0][0].value) == modified_vars.end())
				{
					erase(i++);
					changed = true;
					continue;
				}
				else
				{
					modified_vars[cmd.args[0][0].value] = std::make_pair(true, type == B1Types::B1T_STRING ? L"\"\"" : L"0");
				}
			}
			else
			{
				if(modified_vars.find(cmd.args[0][0].value) == modified_vars.end())
				{
					modified_vars[cmd.args[0][0].value] = std::make_pair(true, type == B1Types::B1T_STRING ? L"\"\"" : L"0");
				}
				else
				{
					if(modified_vars[cmd.args[0][0].value].second == (type == B1Types::B1T_STRING ? L"\"\"" : L"0"))
					{
						erase(i++);
						changed = true;
						continue;
					}
					else
					{
						modified_vars[cmd.args[0][0].value] = std::make_pair(true, type == B1Types::B1T_STRING ? L"\"\"" : L"0");
					}
				}
			}

			i++;
			continue;
		}

		auto dstvar = set_to_init_value(cmd, modified_vars, init, changed);

		if(	cmd.cmd == L"=" &&
			(is_gen_local(cmd.args[1][0].value) || (!is_volatile_var(cmd.args[1][0].value) && cmd.args[1].size() == 1 && !is_mem_var_name(cmd.args[1][0].value)))
			)
		{
			dstvar.clear();

			if(B1CUtils::is_imm_val(cmd.args[0][0].value))
			{
				auto mv = modified_vars.find(cmd.args[1][0].value);
				if(mv == modified_vars.end())
				{
					auto type = is_gen_local(cmd.args[1][0].value) ? B1Types::B1T_UNKNOWN : get_var_type(cmd.args[1][0].value);
					if(init && type != B1Types::B1T_UNKNOWN && ((type == B1Types::B1T_STRING && cmd.args[0][0].value == L"\"\"") || type != B1Types::B1T_STRING && cmd.args[0][0].value == L"0"))
					{
						erase(i++);
						changed = true;
						continue;
					}

					modified_vars[cmd.args[1][0].value] = std::make_pair(true, cmd.args[0][0].value);
				}
				else
				{
					if(mv->second.first == true && mv->second.second == cmd.args[0][0].value)
					{
						erase(i++);
						changed = true;
						continue;
					}
					else
					{
						modified_vars[cmd.args[1][0].value] = std::make_pair(true, cmd.args[0][0].value);
					}
				}
			}
			else
			{
				modified_vars[cmd.args[1][0].value] = std::make_pair(false, L"");
			}
		}

		if(!dstvar.empty())
		{
			modified_vars[dstvar] = std::make_pair(false, L"");
		}

		if(is_udef_used(cmd))
		{
			modified_vars.clear();
			init = false;
		}

		i++;
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::remove_locals(bool &changed)
{
	changed = false;

	for(auto i = begin(); i != end(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(cmd.cmd == L"LA")
		{
			auto la = i;
			const auto &vname = cmd.args[0][0].value;
			// indexes of commands that read and write the variable
			std::vector<iterator> rd, wr;
			// if the variable is used as array subscript or function argument
			bool sub_or_arg = false;

			for(i++; i != end(); i++)
			{
				auto &cmd1 = *i;

				if(B1CUtils::is_label(cmd1))
				{
					continue;
				}

				if(cmd1.cmd == L"LF" && cmd1.args[0][0].value == vname)
				{
					if(!sub_or_arg)
					{
						// indexes of commands to remove
						std::vector<iterator> rem;
						const B1_CMP_ARG *rd_arg = nullptr;
						const B1_CMP_ARG *wr_arg = nullptr;

						// LA,local
						// =,X,local
						// LF,local   ->  remove all commands
						// or
						// LA,local
						// +,A,B,C
						// LF,local   ->  remove LA and LF
						if(rd.size() == 0)
						{
							bool remove_local = true;

							for(auto &w: wr)
							{
								if(is_volatile_used(*w))
								{
									remove_local = false;
								}
								else
								{
									rem.push_back(w);
								}
							}
							wr.clear();

							if(remove_local)
							{
								rem.push_back(la);
								rem.push_back(i);
							}
						}
						else
						// LA,local
						// +,A,B,local
						// =,local,A    ->  +,A,B,A
						// LF,local
						if(rd.size() == 1 && wr.size() == 1 && (*rd.begin())->cmd == L"=" && la == std::prev(*wr.begin()) && la == std::prev(std::prev(*rd.begin())) && la == std::prev(std::prev(std::prev(i))))
						{
							wr_arg = &(*rd.begin())->args[1];
							rem.push_back(*rd.begin());
							rd.clear();
							rem.push_back(la);
							rem.push_back(i);
						}
						else
						// LA,local
						// =,X,local
						// +,local,X,Y    ->  +,X,X,Y
						// LF,local
						if(rd.size() == 1 && wr.size() == 1 && (*wr.begin())->cmd == L"=" && la == std::prev(*wr.begin()) && la == std::prev(std::prev(*rd.begin())) && la == std::prev(std::prev(std::prev(i))))
						{
							rd_arg = &(*wr.begin())->args[0];
							rem.push_back(*wr.begin());
							wr.clear();
							rem.push_back(la);
							rem.push_back(i);
						}

						if(wr_arg != nullptr)
						{
							for(auto &w: wr)
							{
								B1CUtils::replace_dst(*w, vname, (*wr_arg));
							}
						}

						if(rd_arg != nullptr)
						{
							for(auto &r: rd)
							{
								B1CUtils::replace_src(*r, vname, (*rd_arg));
							}
						}

						if(rem.size() != 0)
						{
							// remove local
							i = std::prev(la);
							for(const auto &r: rem)
							{
								erase(r);
							}

							changed = true;
						}
						else
						{
							i = la;
						}
					}
					else
					{
						i = la;
					}

					break;
				}

				if(B1CUtils::is_src(cmd1, vname))
				{
					if(std::find(rd.begin(), rd.end(), i) == rd.end())
					{
						rd.push_back(i);
					}
				}

				if(B1CUtils::is_dst(cmd1, vname))
				{
					if(std::find(wr.begin(), wr.end(), i) == wr.end())
					{
						wr.push_back(i);
					}
				}

				if(B1CUtils::is_sub_or_arg(cmd1, vname))
				{
					sub_or_arg = true;
				}
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1_T_ERROR B1FileCompiler::get_type(B1_TYPED_VALUE &v, bool read, std::map<std::wstring, std::vector<std::pair<B1Types &, B1Types>>> &iif_locals)
{
	if(v.value.empty())
	{
		// omitted function argument
		return B1_RES_OK;
	}

	if(B1CUtils::is_num_val(v.value))
	{
		// num. value
		return B1CUtils::get_num_min_type(v.value, v.type, v.value);
	}

	if(B1CUtils::is_str_val(v.value))
	{
		// string value
		v.type = B1Types::B1T_STRING;
		return B1_RES_OK;
	}

	// skip function arguments
	if(B1CUtils::is_fn_arg(v.value))
	{
		return B1_RES_OK;
	}

	if(is_gen_local(v.value))
	{
		// local variable
		auto vt = _vars.find(v.value);

		if(vt != _vars.end())
		{
			v.type = std::get<0>(vt->second);
		}
		else
		{
			if((v.type == B1Types::B1T_UNKNOWN || v.type == B1Types::B1T_COMMON) && read)
			{
				// get common type for local variable of IIF pseudo-function
				auto il = iif_locals.find(v.value);
				if(il == iif_locals.end() || il->second.size() != 2)
				{
					return B1_RES_ETYPMISM;
				}

				bool comp = false;
				B1Types com_type = B1Types::B1T_UNKNOWN;
				auto err = B1CUtils::get_com_type(il->second[0].second, il->second[1].second, com_type, comp);
				if(err != B1_RES_OK)
				{
					return err;
				}

				il->second[0].first = com_type;
				il->second[1].first = com_type;
				v.type = com_type;

				_vars[v.value] = std::make_tuple(com_type, 0, false, false, false, false);

				iif_locals.erase(il);
			}
			else
			if(v.type == B1Types::B1T_UNKNOWN || v.type == B1Types::B1T_COMMON)
			{
				return B1_RES_ETYPMISM;
			}
			else
			{
				// internal variable IIF$ pseudo-function
				_vars[v.value] = std::make_tuple(v.type, 0, false, false, false, false);
			}
		}

		return B1_RES_OK;
	}

	if(Utils::check_const_name(v.value))
	{
		v.type = Utils::get_const_type(v.value);
		return B1_RES_OK;
	}

	auto fn = get_fn(v);
	if(fn != nullptr)
	{
		// function with zero arguments
		v.type = fn->rettype;
		return B1_RES_OK;
	}

	// simple variable
	if(v.type != B1Types::B1T_VARREF)
	{
		_compiler.mark_var_used(v.value, read);
	}

	// check current file variables
	auto vt = _vars.find(v.value);

	if(vt != _vars.end())
	{
		if(v.type != B1Types::B1T_VARREF)
		{
			if(std::get<1>(vt->second) != 0)
			{
				return B1_RES_ETYPMISM;
			}
			v.type = std::get<0>(vt->second);
		}

		if(std::get<5>(vt->second))
		{
			if(v.type == B1Types::B1T_VARREF)
			{
				if(std::get<1>(vt->second) == 0)
				{
					return B1_RES_ETYPMISM;
				}
			}
			else
			{
				// replace scalar const variable with its value
				v.value = _const_init.find(v.value)->second.second[0];
			}
		}

		return B1_RES_OK;
	}

	// check global variables
	vt = _compiler._global_vars.find(v.value);

	if(vt != _compiler._global_vars.end())
	{
		if(v.type != B1Types::B1T_VARREF)
		{
			if(std::get<1>(vt->second) != 0)
			{
				return B1_RES_ETYPMISM;
			}
			v.type = std::get<0>(vt->second);
		}

		if(std::get<5>(vt->second))
		{
			if(v.type == B1Types::B1T_VARREF)
			{
				if(std::get<1>(vt->second) == 0)
				{
					return B1_RES_ETYPMISM;
				}
			}
			else
			{
				// replace scalar const variable with its value
				v.value = _compiler._global_const_init.find(v.value)->second.second[0];
			}
		}

		return B1_RES_OK;
	}

	if(v.type != B1Types::B1T_VARREF)
	{
		v.type = Utils::get_type_by_type_spec(v.value, B1Types::B1T_UNKNOWN);
		if(v.type == B1Types::B1T_UNKNOWN)
		{
			return B1_RES_ETYPMISM;
		}

		_vars[v.value] = std::make_tuple(v.type, 0, false, false, false, false);
	}

	return B1_RES_OK;
}

B1_T_ERROR B1FileCompiler::get_type(B1_CMP_ARG &a, bool read, std::map<std::wstring, std::vector<std::pair<B1Types &, B1Types>>> &iif_locals)
{
	if(a.size() == 1)
	{
		return get_type(a[0], read, iif_locals);
	}

	if(Utils::check_const_name(a[0].value))
	{
		return B1_RES_ESYNTAX;
	}

	// process arguments first
	for(auto aa = a.begin() + 1; aa != a.end(); aa++)
	{
		auto err = get_type(*aa, true, iif_locals);
		if(err != B1_RES_OK)
		{
			return err;
		}
	}

	auto fn = get_fn(a);
	if(fn != nullptr)
	{
		// function with some arguments
		a[0].type = fn->rettype;
		return B1_RES_OK;
	}

	// subscripted variable
	_compiler.mark_var_used(a[0].value, read);

	// check current file variables
	auto vt = _vars.find(a[0].value);

	if(vt != _vars.end())
	{
		if(std::get<1>(vt->second) != a.size() - 1)
		{
			return B1_RES_ETYPMISM;
		}
		a[0].type = std::get<0>(vt->second);
		return B1_RES_OK;
	}

	// check global variables
	vt = _compiler._global_vars.find(a[0].value);

	if(vt != _compiler._global_vars.end())
	{
		if(std::get<1>(vt->second) != a.size() - 1)
		{
			return B1_RES_ETYPMISM;
		}
		a[0].type = std::get<0>(vt->second);
		return B1_RES_OK;
	}


	a[0].type = Utils::get_type_by_type_spec(a[0].value, B1Types::B1T_UNKNOWN);
	if(a[0].type == B1Types::B1T_UNKNOWN)
	{
		return B1_RES_ETYPMISM;
	}

	_vars[a[0].value] = std::make_tuple(a[0].type, (int)a.size() - 1, false, false, false, false);

	return B1_RES_OK;
}

B1C_T_ERROR B1FileCompiler::put_types()
{
	B1_T_ERROR err;
	std::map<std::wstring, std::vector<std::pair<B1Types &, B1Types>>> iif_locals;

	// set types
	for(auto &cmd: *this)
	{
		// restore code line identification values (to display error position properly)
		_curr_line_cnt = cmd.line_cnt;
		_curr_line_num = cmd.line_num;
		_curr_src_line_id = cmd.src_line_id;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(	cmd.cmd == L"GF" || cmd.cmd == L"LA" || cmd.cmd == L"LF" || cmd.cmd == L"NS" ||
			cmd.cmd == L"CALL" || cmd.cmd == L"JMP" || cmd.cmd == L"JF" || cmd.cmd == L"JT" ||
			cmd.cmd == L"END" || cmd.cmd == L"RET" || cmd.cmd == L"DAT" || cmd.cmd == L"RST" ||
			cmd.cmd == L"ERR" || cmd.cmd == L"DEF" || cmd.cmd == L"INT")
		{
			continue;
		}

		if(B1CUtils::is_log_op(cmd))
		{
			err = get_type(cmd.args[0], true, iif_locals);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}
			err = get_type(cmd.args[1], true, iif_locals);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}
		}
		else
		if(B1CUtils::is_un_op(cmd))
		{
			err = get_type(cmd.args[0], true, iif_locals);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}

			err = get_type(cmd.args[1], false, iif_locals);
			if(err != B1_RES_OK)
			{
				if(err == B1_RES_ETYPMISM && is_gen_local(cmd.args[1][0].value))
				{
					if(cmd.args[1][0].type == B1Types::B1T_COMMON)
					{
						// local variable of IIF pseudo-function
						if(cmd.args[0][0].type == B1Types::B1T_STRING)
						{
							// IIF cannot return strings
							return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
						}

						iif_locals[cmd.args[1][0].value].push_back(std::make_pair(std::ref(cmd.args[1][0].type), cmd.args[0][0].type));
					}
					else
					{
						cmd.args[1][0].type = cmd.args[0][0].type;
						_vars[cmd.args[1][0].value] = std::make_tuple(cmd.args[0][0].type, 0, false, false, false, false);
					}
					continue;
				}

				return static_cast<B1C_T_ERROR>(err);
			}
		}
		else
		if(B1CUtils::is_bin_op(cmd))
		{
			err = get_type(cmd.args[0], true, iif_locals);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}

			err = get_type(cmd.args[1], true, iif_locals);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}

			err = get_type(cmd.args[2], false, iif_locals);
			if(err != B1_RES_OK)
			{
				if(err == B1_RES_ETYPMISM && is_gen_local(cmd.args[2][0].value))
				{
					B1Types com_type = B1Types::B1T_UNKNOWN;
					if(cmd.cmd == L"^")
					{
						com_type = cmd.args[0][0].type;
						if(com_type == B1Types::B1T_UNKNOWN || com_type == B1Types::B1T_INVALID)
						{
							return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
						}
					}
					else
					{
						bool comp = false;
						err = B1CUtils::get_com_type(cmd.args[0][0].type, cmd.args[1][0].type, com_type, comp);
						if(err != B1_RES_OK)
						{
							return static_cast<B1C_T_ERROR>(err);
						}
					}

					if(cmd.args[2][0].type == B1Types::B1T_COMMON)
					{
						// local variable of IIF pseudo-function
						if(com_type == B1Types::B1T_STRING)
						{
							// IIF cannot return strings
							return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
						}
						iif_locals[cmd.args[2][0].value].push_back(std::make_pair(std::ref(cmd.args[2][0].type), com_type));
					}
					else
					{
						cmd.args[2][0].type = com_type;
						_vars[cmd.args[2][0].value] = std::make_tuple(com_type, 0, false, false, false, false);
					}
					continue;
				}

				return static_cast<B1C_T_ERROR>(err);
			}
		}
		else
		{
			// GET, PUT and TRR statements is a special case because of possible array range usage
			if(cmd.cmd == L"GET" || cmd.cmd == L"PUT" || cmd.cmd == L"TRR")
			{
				bool read = (cmd.cmd == L"PUT");

				err = get_type(cmd.args[1], read, iif_locals);
				if(err != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err);
				}

				// forbid GET and TRR statements with string argument
				if(!read && cmd.args[1][0].type == B1Types::B1T_STRING)
				{
					return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
				}

				if(cmd.args.size() != 2)
				{
					err = get_type(cmd.args[2], true, iif_locals);
					if(err != B1_RES_OK)
					{
						return static_cast<B1C_T_ERROR>(err);
					}
				}

				continue;
			}

			bool read = true;
			if(cmd.cmd == L"IN" || cmd.cmd == L"READ")
			{
				read = false;
			}

			for(auto a = cmd.args.begin(); a != cmd.args.end(); a++)
			{
				if(cmd.cmd == L"GA" && a < cmd.args.begin() + 2)
				{
					continue;
				}
				if(cmd.cmd == L"MA" && a < cmd.args.begin() + 3)
				{
					continue;
				}
				if(cmd.cmd == L"RETVAL" && a != cmd.args.begin())
				{
					continue;
				}
				if(cmd.cmd == L"OUT" && a != cmd.args.begin() + 1)
				{
					continue;
				}
				if(cmd.cmd == L"IN" && a != cmd.args.begin() + 1)
				{
					continue;
				}
				if(cmd.cmd == L"SET" && a != cmd.args.begin() + 1)
				{
					continue;
				}
				if(cmd.cmd == L"IOCTL" && a != cmd.args.begin() + 2)
				{
					continue;
				}
				if(cmd.cmd == L"READ" && a == cmd.args.begin())
				{
					continue;
				}

				err = get_type(*a, read, iif_locals);
				if(err != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err);
				}
			}
		}
	}

	// process LA stmts
	for(auto i = begin(); i != end(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(cmd.cmd == L"LA")
		{
			auto vt = _vars.find(cmd.args[0][0].value);

			if(vt != _vars.end())
			{
				cmd.args[1][0].value = Utils::get_type_name(std::get<0>(vt->second));
				cmd.args[1][0].type = std::get<0>(vt->second);
			}
			else
			{
				for(auto i1 = std::next(i); i1 != end(); i1++)
				{
					if(i1->cmd == L"LF" && i1->args[0][0].value == cmd.args[0][0].value)
					{
						auto next = std::prev(i);
						erase(i);
						erase(i1);
						i = next;
						break;
					}
				}
				continue;
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::inline_fns(bool &changed)
{
	changed = false;

	// remove unnecessary ASC, VAL, CHR$ and STR$ function calls
	for(auto i = begin(); i != end(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		_curr_line_num = cmd.line_num;
		_curr_line_cnt = cmd.line_cnt;
		_curr_src_line_id = cmd.src_line_id;

		for(auto &a: cmd.args)
		{
			if(a.size() == 2 && ((a[0].value == L"VAL" && a[1].type != B1Types::B1T_STRING) || (a[0].value == L"STR$" && a[1].type == B1Types::B1T_STRING)))
			{
				a.erase(a.begin());
				changed = true;
			}
			else
			if(a.size() == 2 && a[1].type != B1Types::B1T_STRING && (a[0].value == L"CBYTE" || a[0].value == L"CINT" || a[0].value == L"CWRD" || a[0].value == L"CLNG") && B1CUtils::is_num_val(a[1].value))
			{
				auto type = a[0].type;
				int32_t n;

				if(Utils::str2int32(a[1].value, n) == B1_RES_OK)
				{
					Utils::correct_int_value(n, type);
					a[1].value = std::to_wstring(n);
				}
				a.erase(a.begin());
				a[0].type = type;

				changed = true;
			}
			else
			if(a.size() == 2 && a[0].value == L"CHR$" && B1CUtils::is_num_val(a[1].value))
			{
				a[0].value.clear();

				auto err = eval_chr(a[1].value, a[1].type, a[0].value);
				if(err != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err);
				}

				a.erase(a.begin() + 1);
				changed = true;
			}
			else
			if(a.size() == 2 && a[0].value == L"ASC" && B1CUtils::is_str_val(a[1].value))
			{
				std::wstring sval;
				auto err = B1CUtils::get_string_data(a[1].value, sval);
				if(err != B1_RES_OK)
				{
					return static_cast<B1C_T_ERROR>(err);
				}
				if(sval.empty())
				{
					return static_cast<B1C_T_ERROR>(B1_RES_EINVARG);
				}

				a[0].value = std::to_wstring(sval.front());
				a.erase(a.begin() + 1);
				changed = true;
			}
		}
	}

	// inline ABS and SGN
	for(auto i = begin(); i != end(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		_curr_line_num = cmd.line_num;
		_curr_line_cnt = cmd.line_cnt;
		_curr_src_line_id = cmd.src_line_id;

		for(int a = 0; a < cmd.args.size(); a++)
		{
			const auto &arg = cmd.args[a];

			if(arg.size() == 2 && arg[0].value == L"SGN")
			{
				const auto a1 = arg[1];

				auto ltype = arg[0].type;
				auto local = emit_local(ltype, i);

				// change original command
				i->args[a][0].type = ltype;
				i->args[a][0].value = local;
				i->args[a].pop_back();

				emit_command(L"LF", std::next(i), local);

				if(a1.type == B1Types::B1T_BYTE || a1.type == B1Types::B1T_WORD)
				{
					// =,SGN(A),B -> LA,local0
					//               =,0,local0
					//               ==,A,0
					//               JT,label0
					//               +,local0,1,local0
					//               :label0
					//               =,local0,B
					//               LF,local0
					emit_command(L"=", i, { B1_TYPED_VALUE(L"0", B1Types::B1T_BYTE), B1_TYPED_VALUE(local, ltype) });
					emit_command(L"==", i, { a1, B1_TYPED_VALUE(L"0", B1Types::B1T_BYTE) });
					auto label = emit_label(true);
					emit_command(L"JT", i, label);
					emit_command(L"=", i, { B1_TYPED_VALUE(L"1", B1Types::B1T_BYTE), B1_TYPED_VALUE(local, ltype) });
					emit_label(label, i);
				}
				else
				{
					// =,SGN(A),B -> LA,local0
					//               =,0,local0
					//               ==,A,0
					//               JT,label0
					//               <,A,0
					//               JT,label1
					//               +,local0,2,local0
					//               :label1
					//               -,local0,1,local0
					//               :label0
					//               =,local0,B
					//               LF,local0
					emit_command(L"=", i, { B1_TYPED_VALUE(L"0", B1Types::B1T_BYTE), B1_TYPED_VALUE(local, ltype) });
					emit_command(L"==", i, { a1, B1_TYPED_VALUE(L"0", B1Types::B1T_BYTE) });
					auto label1 = emit_label(true);
					auto label2 = emit_label(true);
					emit_command(L"JT", i, label2);
					emit_command(L"<", i, { a1, B1_TYPED_VALUE(L"0", B1Types::B1T_BYTE) });
					emit_command(L"JT", i, label1);
					emit_command(L"=", i, { B1_TYPED_VALUE(L"2", B1Types::B1T_BYTE), B1_TYPED_VALUE(local, ltype) });
					emit_label(label1, i);
					emit_command(L"-", i, { B1_TYPED_VALUE(local, ltype), B1_TYPED_VALUE(L"1", B1Types::B1T_BYTE), B1_TYPED_VALUE(local, ltype) });
					emit_label(label2, i);
				}

				changed = true;

				i--;
				break;
			}

			if(arg.size() == 2 && arg[0].value == L"ABS")
			{
				const auto a1 = arg[1];

				if(a1.type == B1Types::B1T_BYTE || a1.type == B1Types::B1T_WORD)
				{
					// unsigned type: just remove ABS call
					i->args[a][0].type = a1.type;
					i->args[a][0].value = a1.value;
					i->args[a].pop_back();
				}
				else
				{
					// =,ABS(A),B -> LA,local0
					//               =,A,local0
					//               <,local0,0
					//               JF,label0
					//               -,local0,local0
					//               :label0
					//               =,local0,B
					//               LF,local0
					auto local = emit_local(a1.type, i);

					// change original command
					i->args[a][0].type = a1.type;
					i->args[a][0].value = local;
					i->args[a].pop_back();

					emit_command(L"LF", std::next(i), local);

					emit_command(L"=", i, { a1, B1_TYPED_VALUE(local, a1.type) });
					emit_command(L"<", i, { B1_TYPED_VALUE(local, a1.type), B1_TYPED_VALUE(L"0", B1Types::B1T_BYTE) });
					auto label = emit_label(true);
					emit_command(L"JF", i, label);
					emit_command(L"-", i, { B1_TYPED_VALUE(local, a1.type), B1_TYPED_VALUE(local, a1.type) });
					emit_label(label, i);
				}

				changed = true;

				i--;
				break;
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

bool B1FileCompiler::get_LA_LF(iterator s, iterator e, iterator &la, iterator &lf)
{
	for(la = s; la != e; la++)
	{
		if(B1CUtils::is_label(*la))
		{
			continue;
		}

		if(la->cmd == L"LA")
		{
			for(lf = std::next(la); lf != e; lf++)
			{
				if(lf->cmd == L"LF" && la->args[0][0].value == lf->args[0][0].value)
				{
					return true;
				}
			}
		}
	}

	return false;
}

B1C_T_ERROR B1FileCompiler::reuse_locals(bool &changed)
{
	changed = false;

	// reuse external locals:
	// LA,local_0               LA,local_0
	// =,10,local_0             =,10,local_0
	// =,local_0,A              =,local_0,A
	// LA,local_1               =,11,local_0
	// = 11,local_1             + local_0,local_0,B
	// + local_1,local_1,B  ->  LF,local_0
	// LF,local_1
	// LF,local_0

	iterator s0 = begin(), e0 = end(), la0, lf0, s1, e1, la1, lf1;

	while(true)
	{
		if(!get_LA_LF(s0, e0, la0, lf0))
		{
			break;
		}

		s1 = std::next(la0);
		e1 = lf0;

		while(true)
		{
			if(!get_LA_LF(s1, e1, la1, lf1) || std::next(la1) == lf1)
			{
				break;
			}

			auto &t0 = la0->args[1][0].type;
			auto &t1 = la1->args[1][0].type;

			if(B1CUtils::local_compat_types(t0, t1))
			{
				bool var_used = false;

				for(auto i = std::next(lf1); i != lf0; i++)
				{
					if(B1CUtils::is_src(*i, la0->args[0][0].value) || B1CUtils::is_sub_or_arg(*i, la0->args[0][0].value))
					{
						var_used = true;
						break;
					}
					if(B1CUtils::is_dst(*i, la0->args[0][0].value))
					{
						break;
					}
				}

				if(var_used)
				{
					s1 = std::next(la1);
					continue;
				}

				bool l1_is_dst = false;
				for(auto i = std::next(la1); i != lf0; i++)
				{
					if(B1CUtils::is_used(*i, la0->args[0][0].value))
					{
						// allow replacements like:
						// la,l1
						// +,l0,5,l1
						// =,l1,A    ->  +,l0,5,l0
						// lf,l1         =,l0,A
						if(!l1_is_dst && B1CUtils::is_dst(*i, la1->args[0][0].value))
						{
							l1_is_dst = true;
							continue;
						}
						var_used = true;
						break;
					}
				}

				if(var_used)
				{
					s1 = std::next(la1);
					continue;
				}

				// replace internal local
				B1_TYPED_VALUE tv(la0->args[0][0].value, t1);

				for(auto i = std::next(la1); i != lf1; i++)
				{
					B1CUtils::replace_all(*i, la1->args[0][0].value, tv, true);
				}

				// remove internal la/lf
				s1 = std::next(la1);
				erase(la1);
				erase(lf1);

				changed = true;
			}
			else
			{
				s1 = std::next(la1);
			}
		}

		s0 = std::next(la0);
	}

	// =,local1,local2
	// lf,local1
	// +,local2,1,local2
	// ...
	// lf,local2
	// ->
	// +,local1,1,local1
	// ...
	// lf,local1
	// lf,local2
	for(auto i = begin(); i != end(); i++)
	{
		if(B1CUtils::is_label(*i))
		{
			continue;
		}

		auto inext = std::next(i);

		if(	i->cmd == L"=" &&
			i->args[0].size() == 1 &&
			i->args[1].size() == 1 &&
			is_gen_local(i->args[0][0].value) &&
			is_gen_local(i->args[1][0].value) &&
			i->args[0][0].value != i->args[1][0].value &&
			inext != end() &&
			inext->cmd == L"LF" &&
			inext->args[0][0].value == i->args[0][0].value)
		{
			auto last_repl = end();

			for(auto i1 = std::next(inext); i1 != end(); i1++)
			{
				if(B1CUtils::is_label(*i1))
				{
					continue;
				}

				if(i1->cmd == L"LF" && i1->args[0][0].value == i->args[1][0].value)
				{
					if(last_repl != end())
					{
						splice(std::next(last_repl), *this, inext);
					}
					inext = std::next(i);
					erase(i);
					i = std::prev(inext);
					break;
				}

				if(B1CUtils::replace_all(*i1, i->args[1][0].value, i->args[0][0], true))
				{
					last_repl = i1;
				}
			}
		}
	}


	// reuse locals
	s0 = begin(), e0 = end();

	while(true)
	{
		if(!get_LA_LF(s0, e0, la0, lf0))
		{
			break;
		}

		s1 = std::next(lf0);
		e1 = e0;

		while(true)
		{
			if(!get_LA_LF(s1, e1, la1, lf1))
			{
				break;
			}

			// check if the local can be reused (no end, rets, jumps, labels and lfs of embracing locals)
			bool can_reuse = true;

			std::set<std::wstring> la_stmts;

			for(auto i = std::next(lf0); i != la1; i++)
			{
				if(B1CUtils::is_label(*i))
				{
					can_reuse = false;
					break;
				}
				if(i->cmd == L"JMP" || i->cmd == L"JT" || i->cmd == L"JF" || i->cmd == L"ERR")
				{
					can_reuse = false;
					break;
				}
				if(i->cmd == L"RET" || i->cmd == L"END")
				{
					can_reuse = false;
					break;
				}
				if(i->cmd == L"LF" && la_stmts.find(i->args[0][0].value) == la_stmts.end())
				{
					can_reuse = false;
					break;
				}
				if(i->cmd == L"LA")
				{
					la_stmts.insert(i->args[0][0].value);
				}
			}

			if(!can_reuse)
			{
				s1 = std::next(la1);
				continue;
			}

			// check types
			if(B1CUtils::local_compat_types(la0->args[1][0].type, la1->args[1][0].type))
			{
				B1_TYPED_VALUE tv(la0->args[0][0].value, la1->args[1][0].type);

				// replace local
				for(auto i = std::next(la1); i != lf1; i++)
				{
					if(B1CUtils::is_label(*i))
					{
						continue;
					}

					B1CUtils::replace_all(*i, la1->args[0][0].value, tv, true);
				}

				lf1->args[0][0].value = tv.value;

				erase(lf0);
				erase(la1);

				lf0 = lf1;
				s1 = std::next(lf1);

				fix_LA_LF_order();

				changed = true;
				
				la0--;
				break;
			}
			else
			{
				s1 = std::next(la1);
			}
		}

		s0 = std::next(la0);
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

// try to get rid from locals (reusing user variables)
// +,a,b,local     <- here local is a local variable to replace with a user one
// -,local,c,local
// *,d,local,local
// /,100,local,e   <- last read of the local, use e intead of it
// ...             <- here the local is not used
// lf,local or writing something to the local
// ->
// +,a,b,e
// -,e,c,e
// *,d,e,e
// /,100,e,e
// ...
// lf,local or writing something to the local
B1C_T_ERROR B1FileCompiler::reuse_vars(bool &changed)
{
	changed = false;

	for(auto i = begin(); i != end(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		auto local = B1CUtils::get_dst_var(cmd, true);
		if(local == nullptr || !is_gen_local(local->value))
		{
			continue;
		}

		iterator wr = i;
		iterator rd = end();
		bool udef_used = false;

		for(auto j = std::next(i); j != end(); j++)
		{
			auto &cmd1 = *j;

			if(	B1CUtils::is_label(cmd1) || cmd1.cmd == L"JMP" || cmd1.cmd == L"JT" || cmd1.cmd == L"JF" || cmd1.cmd == L"CALL" ||
				cmd1.cmd == L"RET" || cmd1.cmd == L"ERR" || cmd1.cmd == L"END" || cmd1.cmd == L"DEF")
			{
				break;
			}

			if(is_udef_used(cmd1))
			{
				udef_used = true;
			}

			auto dst1 = B1CUtils::get_dst_var(cmd1, true);
			if(dst1 != nullptr && dst1->value == local->value)
			{
				wr = j;
			}
			if(B1CUtils::is_src(cmd1, local->value) || B1CUtils::is_sub_or_arg(cmd1, local->value))
			{
				rd = j;
			}

			if((wr == j && rd != j) || (cmd1.cmd == L"LF" && cmd1.args[0][0].value == local->value))
			{
				if(rd == end())
				{
					if(!is_volatile_used(cmd))
					{
						erase(i++);
						i--;
						changed = true;
					}
				}
				else
				{
					auto var_to_reuse = B1CUtils::get_dst_var(*rd, true);
					if(var_to_reuse == nullptr || is_volatile_var(var_to_reuse->value))
					{
						break;
					}
					if(udef_used && !is_gen_local(var_to_reuse->value))
					{
						break;
					}
					
					B1Types com_type;
					bool comp_types = false;
					if(B1CUtils::get_com_type(local->type, var_to_reuse->type, com_type, comp_types) != B1_RES_OK || !comp_types)
					{
						break;
					}

					bool var_used = false;
					for(auto r = std::next(i); r != rd; r++)
					{
						if(B1CUtils::is_used(*r, var_to_reuse->value))
						{
							var_used = true;
							break;
						}
					}
					if(var_used || B1CUtils::is_src(*rd, var_to_reuse->value) || B1CUtils::is_sub_or_arg(*rd, var_to_reuse->value))
					{
						break;
					}

					auto local_name = local->value;
					B1CUtils::replace_dst(*i, local_name, B1_CMP_ARG(var_to_reuse->value, var_to_reuse->type), true);
					for(auto r = std::next(i); r != std::next(rd); r++)
					{
						B1CUtils::replace_all(*r, local_name, *var_to_reuse, true);
					}
					changed = true;
				}

				break;
			}
		}
	}

	// = <smth>,var1
	// ... <- <smth> must not be used here
	// +,var1,var2,var1
	// ->
	// ...
	// +,<smth>,var2,var1
	// or
	// = <smth>,var1
	// ... <- <smth> must not be used here
	// +,var1,var2,var3
	// +,var3,var4,var1
	// ->
	// ...
	// +,<smth>,var2,var3
	// +,var3,var4,var1

	for(auto i = begin(); i != end(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		bool arg1_udef = false;
		bool arg1_volatile = false;

		if(cmd.cmd == L"=" && !(arg1_udef = is_udef_used(cmd.args[1])) && !(arg1_volatile = is_volatile_used(cmd.args[1])))
		{
			bool arg0_udef = is_udef_used(cmd.args[0]);
			bool arg0_volatile = is_volatile_used(cmd.args[0]);

			iterator rd = end(), wr = end();

			for(auto j = std::next(i); j != end(); j++)
			{
				auto &cmd1 = *j;

				if (B1CUtils::is_label(cmd1) || cmd1.cmd == L"JMP" || cmd1.cmd == L"JT" || cmd1.cmd == L"JF" || cmd1.cmd == L"CALL" ||
					cmd1.cmd == L"RET" || cmd1.cmd == L"ERR" || cmd1.cmd == L"END" || cmd1.cmd == L"DEF" ||
					(is_udef_used(cmd1) && !is_gen_local(cmd.args[1][0].value)))
				{
					break;
				}

				if(arg0_udef)
				{
					auto dst_var = B1CUtils::get_dst_var(cmd1, false);
					if(dst_var != nullptr)
					{
						if(!is_gen_local(dst_var->value) && !(cmd.args[1].size() == 1 && B1_CMP_ARG(dst_var->value, dst_var->type) == cmd.args[1]))
						{
							break;
						}
					}
				}

				if(cmd.args[1].size() == 1 && B1CUtils::is_sub_or_arg(cmd1, cmd.args[1][0].value))
				{
					break;
				}

				bool is_src = B1CUtils::arg_is_src(cmd1, cmd.args[1]);
				bool is_dst = B1CUtils::arg_is_dst(cmd1, cmd.args[1], false);

				if(is_dst && cmd1.cmd == L"TRR")
				{
					break;
				}

				if(!is_dst && ((cmd1.cmd == L"LF" && cmd1.args[0][0].value == cmd.args[1][0].value) || ((cmd1.cmd == L"GA" || cmd1.cmd == L"GF") && cmd1.args[0][0].value == cmd.args[1][0].value)))
				{
					is_dst = true;
				}

				if(is_src)
				{
					if(rd != end())
					{
						break;
					}

					rd = j;
				}

				if(is_dst)
				{
					if(rd == end())
					{
						break;
					}

					wr = j;
				}

				if(rd != end() && wr != end())
				{
					bool arg_or_sub_changed = false;

					if(cmd.args[0].size() > 1)
					{
						auto last = (wr == rd) ? wr : std::next(wr);
						for(auto i1 = std::next(i); !arg_or_sub_changed && i1 != last; i1++)
						{
							for(auto a = cmd.args[0].cbegin() + 1; a != cmd.args[0].cend(); a++)
							{
								if(B1CUtils::is_dst(*i1, a->value))
								{
									arg_or_sub_changed = true;
									break;
								}
							}
						}
					}
					if(arg_or_sub_changed)
					{
						break;
					}

					int count = 0;
					auto ctmp = *rd;
					B1CUtils::replace_src(ctmp, cmd.args[1], cmd.args[0], &count);
					if(count == 1 || !(arg0_volatile || arg0_udef))
					{
						*rd = ctmp;
						auto next = std::prev(i);
						erase(i);
						i = next;
						changed = true;
					}

					break;
				}
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

bool B1FileCompiler::eval_imm_fn_arg(B1_CMP_ARG &a)
{
	bool changed = false;
	auto fn = get_fn(a);

	if(fn != nullptr)
	{
		for(auto aa = a.begin() + 1; aa != a.end(); aa++)
		{
			if(aa->type != B1Types::B1T_STRING && B1CUtils::is_num_val(aa->value) && fn->args[aa - a.begin() - 1].type == B1Types::B1T_STRING)
			{
				int32_t n;

				if(Utils::str2int32(aa->value, n) == B1_RES_OK)
				{
					Utils::correct_int_value(n, aa->type);
					aa->type = B1Types::B1T_STRING;
					aa->value = L"\"" + std::to_wstring(n) + L"\"";
					changed = true;
				}
			}
		}

		if(fn->name == L"LEN" && a[1].type == B1Types::B1T_STRING && B1CUtils::is_str_val(a[1].value))
		{
			std::wstring sval;

			auto err = B1CUtils::get_string_data(a[1].value, sval);
			if(err == B1_RES_OK)
			{
				a.pop_back();
				a[0].value = std::to_wstring(sval.length());
				changed = true;
			}
		}
		else
		if(fn->name == L"VAL" && a[1].type == B1Types::B1T_STRING && B1CUtils::is_str_val(a[1].value))
		{
			std::wstring sval;

			auto err = B1CUtils::get_string_data(a[1].value, sval);
			if(err == B1_RES_OK)
			{
				B1Types type = B1Types::B1T_INVALID;

				if(B1CUtils::get_num_min_type(sval, type, sval) == B1_RES_OK)
				{
					a.pop_back();
					a[0].value = sval;
					a[0].type = type;
					changed = true;
				}
			}
		}
		else
		if((fn->name == L"CBYTE" || fn->name == L"CINT" || fn->name == L"CWRD" || fn->name == L"CLNG") && a[1].type == B1Types::B1T_STRING && B1CUtils::is_str_val(a[1].value))
		{
			std::wstring sval;

			auto err = B1CUtils::get_string_data(a[1].value, sval);
			if(err == B1_RES_OK)
			{
				a.pop_back();
				a[0].value = sval;
				a[0].type = fn->rettype;
				changed = true;
			}
		}
		else
		if(fn->name == L"STR$" && (a[1].type == B1Types::B1T_INT || a[1].type == B1Types::B1T_WORD || a[1].type == B1Types::B1T_BYTE || a[1].type == B1Types::B1T_LONG) && B1CUtils::is_num_val(a[1].value))
		{
			int32_t n;

			if(Utils::str2int32(a[1].value, n) == B1_RES_OK)
			{
				Utils::correct_int_value(n, fn->args[0].type);
				a.pop_back();
				a[0].value = L"\"" + std::to_wstring(n) + L"\"";
				changed = true;
			}
		}
	}

	return changed;
}

B1C_T_ERROR B1FileCompiler::eval_imm_exps(bool &changed)
{
	changed = false;

	// process simple expressions:
	// =,10,A$  ->  =,"10",A$
	// +,10,20,A  ->  =,30,A
	// +,10,"20",A$  ->  =,"1020",A$
	// etc.
	for(auto &cmd: *this)
	{
		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		// =,<num_val>,<string_var_name>  ->  =,<num_val_converted_to_str>,<string_var_name>
		if(cmd.cmd == L"=" && B1CUtils::is_num_val(cmd.args[0][0].value) && cmd.args[1][0].type == B1Types::B1T_STRING)
		{
			// convert integer value to string
			int32_t n;

			if(Utils::str2int32(cmd.args[0][0].value, n) == B1_RES_OK)
			{
				Utils::correct_int_value(n, cmd.args[0][0].type);
				cmd.args[0][0].value = L"\"" + std::to_wstring(n) + L"\"";
				cmd.args[0][0].type = B1Types::B1T_STRING;
				changed = true;
			}
			continue;
		}

		if(B1CUtils::is_bin_op(cmd))
		{
			bool is_n1, is_n2;

			is_n1 = B1CUtils::is_num_val(cmd.args[0][0].value);
			is_n2 = B1CUtils::is_num_val(cmd.args[1][0].value);

			int32_t n1, n2;

			if(is_n1)
			{
				is_n1 = Utils::str2int32(cmd.args[0][0].value, n1) == B1_RES_OK;
				if(is_n1)
				{
					Utils::correct_int_value(n1, cmd.args[0][0].type);
				}
			}
			if(is_n2)
			{
				is_n2 = Utils::str2int32(cmd.args[1][0].value, n2) == B1_RES_OK;
				if(is_n2)
				{
					Utils::correct_int_value(n2, cmd.args[1][0].type);
				}
			}

			if(is_n1 && is_n2)
			{
				// process numeric values
				switch(cmd.cmd.front())
				{
					case L'+':
						n1 += n2;
						break;
					case L'-':
						n1 -= n2;
						break;
					case L'*':
						n1 *= n2;
						break;
					case L'/':
						n1 /= n2;
						break;
					case L'^':
						n1 = (int32_t)std::pow(n1, n2);
						break;
					// left shift
					case L'<':
						n1 <<= n2;
						break;
					// right shift
					case L'>':
						n1 >>= n2;
						break;
					case L'%':
						n1 %= n2;
						break;
					case L'&':
						n1 &= n2;
						break;
					case L'|':
						n1 |= n2;
						break;
					case L'~':
						n1 ^= n2;
						break;
				}

				B1Types com_type = B1Types::B1T_UNKNOWN;

				if(cmd.cmd.front() == L'^')
				{
					com_type = cmd.args[0][0].type;
				}
				else
				{
					bool comp_types = false;
					B1CUtils::get_com_type(cmd.args[0][0].type, cmd.args[1][0].type, com_type, comp_types);
				}

				Utils::correct_int_value(n1, com_type);

				const auto res_type = cmd.args[2][0].type;
				B1_TYPED_VALUE tv(L"", res_type);

				if(res_type == B1Types::B1T_STRING)
				{
					tv.value = L"\"" + std::to_wstring(n1) + L"\"";
				}
				else
				{
					Utils::correct_int_value(n1, res_type);
					tv.value = std::to_wstring(n1);
				}

				cmd.cmd = L"=";
				cmd.args[0].clear();
				cmd.args[0].push_back(tv);
				cmd.args[1] = cmd.args[2];
				cmd.args.pop_back();

				changed = true;
			}
			else
			if(cmd.cmd == L"+" &&
				(is_n1 && B1CUtils::is_str_val(cmd.args[1][0].value)) ||
				(is_n2 && B1CUtils::is_str_val(cmd.args[0][0].value)) ||
				(B1CUtils::is_str_val(cmd.args[0][0].value) && B1CUtils::is_str_val(cmd.args[1][0].value))
				)
			{
				// string and numeric or two strings concatenation
				std::wstring s1, s2;

				s1 = cmd.args[0][0].value;
				if(is_n1)
				{
					s1 = L"\"" + std::to_wstring(n1) + L"\"";
				}

				s2 = cmd.args[1][0].value;
				if(is_n2)
				{
					s2 = L"\"" + std::to_wstring(n2) + L"\"";
				}

				s1.pop_back();
				s2.erase(0, 1);

				cmd.cmd = L"=";
				cmd.args.erase(cmd.args.begin());
				cmd.args[0].clear();
				cmd.args[0].push_back(B1_TYPED_VALUE(s1 + s2, B1Types::B1T_STRING));

				changed = true;
			}
			else
			if(cmd.cmd == L"+" &&
				(is_n1 && cmd.args[1][0].type == B1Types::B1T_STRING) ||
				(is_n2 && cmd.args[0][0].type == B1Types::B1T_STRING)
				)
			{
				if(is_n1)
				{
					cmd.args[0][0].value = L"\"" + std::to_wstring(n1) + L"\"";
					cmd.args[0][0].type = B1Types::B1T_STRING;

					changed = true;
				}
				else
				if(is_n2)
				{
					cmd.args[1][0].value = L"\"" + std::to_wstring(n2) + L"\"";
					cmd.args[1][0].type = B1Types::B1T_STRING;

					changed = true;
				}
			}
		}
	}

	// evaluate sequences of + or * operations like
	// +,X,10,local_0
	// +,local_0,20,_local_1
	// +,30,local_1,local_2   ->  +,X,60,local_2
	// -,local_2,25,Y             -,local_2,25,Y
	for(auto i = begin(); i != end(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(B1CUtils::is_bin_op(cmd) && is_gen_local(cmd.args[2][0].value))
		{
			int imm_ind =	B1CUtils::is_imm_val(cmd.args[0][0].value) ? 0 :
							B1CUtils::is_imm_val(cmd.args[1][0].value) ? 1 : -1;

			if(imm_ind < 0)
			{
				continue;
			}

			if(cmd.cmd == L"/" || cmd.cmd == L"^" || cmd.cmd == L">>" || cmd.cmd == L"<<" || cmd.cmd == L"%" || cmd.cmd == L"&" || cmd.cmd == L"|" || cmd.cmd == L"~" || (cmd.cmd == L"-" && imm_ind == 0))
			{
				continue;
			}

			bool str_op = false;

			if(cmd.args[2][0].type == B1Types::B1T_STRING)
			{
				if(cmd.cmd == L"+" && imm_ind == 1 && cmd.args[0][0].type == B1Types::B1T_STRING)
				{
					str_op = true;
				}
				else
				{
					continue;
				}
			}

			bool is_add = (cmd.cmd != L"*");

			for(auto j = std::next(i); j !=end(); j++)
			{
				auto &cmd1 = *j;

				if(B1CUtils::is_label(cmd1))
				{
					break;
				}

				if(cmd1.cmd == L"LA" || cmd1.cmd == L"LF")
				{
					continue;
				}

				if(B1CUtils::is_bin_op(cmd1))
				{
					int imm_ind1 =	B1CUtils::is_imm_val(cmd1.args[0][0].value) ? 0 :
									B1CUtils::is_imm_val(cmd1.args[1][0].value) ? 1 : -1;

					if(imm_ind1 >= 0 && cmd1.args[imm_ind1 == 0 ? 1 : 0][0].value == cmd.args[2][0].value)
					{
						if(str_op)
						{
							if(cmd1.cmd != L"+" || imm_ind1 != 1)
							{
								break;
							}
						}
						else
						{
							if(cmd1.args[imm_ind1][0].type == B1Types::B1T_STRING)
							{
								break;
							}
						}

						if ((is_add && (cmd1.cmd == L"+" || (cmd1.cmd == L"-" && imm_ind1 == 1))) ||
							(!is_add && cmd1.cmd == L"*"))
						{
							bool is_n1, is_n2;

							is_n1 = B1CUtils::is_num_val(cmd.args[imm_ind][0].value);
							is_n2 = B1CUtils::is_num_val(cmd1.args[imm_ind1][0].value);

							int32_t n1, n2;

							if(is_n1)
							{
								is_n1 = Utils::str2int32(cmd.args[imm_ind][0].value, n1) == B1_RES_OK;
							}
							if(is_n2)
							{
								is_n2 = Utils::str2int32(cmd1.args[imm_ind1][0].value, n2) == B1_RES_OK;
							}

							std::wstring val;
							B1Types type = B1Types::B1T_STRING;

							if(str_op)
							{
								if(is_n1)
								{
									Utils::correct_int_value(n1, cmd.args[1][0].type);
									val = L"\"" + std::to_wstring(n1) + L"\"";
								}
								else
								{
									val = cmd.args[1][0].value;
								}

								auto len = val.size();
									
								if(is_n2)
								{
									Utils::correct_int_value(n2, cmd1.args[1][0].type);
									val += L"\"" + std::to_wstring(n2) + L"\"";
								}
								else
								{
									val += cmd1.args[1][0].value;
								}

								val.erase(len - 1, 2);
							}
							else
							{
								if(!is_n1 || !is_n2)
								{
									return static_cast<B1C_T_ERROR>(B1_RES_EINVNUM);
								}

								if(cmd.cmd != cmd1.cmd)
								{
									n1 = -n1;
								}

								switch(cmd.cmd.front())
								{
									case L'+':
									case L'-':
										n1 += n2;
										break;
									case L'*':
										n1 *= n2;
										break;
								}

								if(n1 <= 0 && cmd1.cmd != L"*")
								{
									cmd1.cmd = (cmd1.cmd == L"-") ? L"+" : L"-";
									n1 = -n1;
								}

								type = cmd.args[2][0].type;
								Utils::correct_int_value(n1, type);

								val = std::to_wstring(n1);
							}

							cmd1.args[0] = cmd.args[imm_ind == 0 ? 1 : 0];
							cmd1.args[1].clear();
							cmd1.args[1].push_back(B1_TYPED_VALUE(val, type));

							erase(i--);

							changed = true;
						}
					}
				}

				break;
			}
		}
	}

	// convert imm. function arguments
	// =,LEN(10<INT>)<STRING>,A<INT>  ->  =,LEN("10"<STRING>),A<INT>
	for(auto &cmd: *this)
	{
		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(B1CUtils::is_un_op(cmd))
		{
			if(eval_imm_fn_arg(cmd.args[0]))
			{
				changed = true;
			}
		}
		else
		if(B1CUtils::is_bin_op(cmd) || B1CUtils::is_log_op(cmd))
		{
			if(eval_imm_fn_arg(cmd.args[0]))
			{
				changed = true;
			}

			if(eval_imm_fn_arg(cmd.args[1]))
			{
				changed = true;
			}
		}

		// check also GA, MA and RETVAL
		if(cmd.cmd == L"GA" || cmd.cmd == L"MA")
		{
			for(auto a = cmd.args.begin() + (cmd.cmd == L"GA" ? 2 : 3); a != cmd.args.end(); a++)
			{
				if(eval_imm_fn_arg(*a))
				{
					changed = true;
				}
			}
		}
		else
		if(cmd.cmd == L"RETVAL")
		{
			if(eval_imm_fn_arg(cmd.args[0]))
			{
				changed = true;
			}
		}
		else
		if(cmd.cmd == L"OUT")
		{
			if(eval_imm_fn_arg(cmd.args[1]))
			{
				changed = true;
			}
		}
		else
		if(cmd.cmd == L"IN")
		{
			if(eval_imm_fn_arg(cmd.args[1]))
			{
				changed = true;
			}
		}
		else
		if(cmd.cmd == L"SET")
		{
			if(eval_imm_fn_arg(cmd.args[1]))
			{
				changed = true;
			}
		}
		else
		if(cmd.cmd == L"IOCTL")
		{
			if(cmd.args.size() > 2 && eval_imm_fn_arg(cmd.args[2]))
			{
				changed = true;
			}
		}
		else
		if(cmd.cmd == L"READ")
		{
			if(eval_imm_fn_arg(cmd.args[1]))
			{
				changed = true;
			}
		}
		else
		if(cmd.cmd == L"GET" || cmd.cmd == L"PUT" || cmd.cmd == L"TRR")
		{
			if(eval_imm_fn_arg(cmd.args[1]))
			{
				changed = true;
			}

			if(cmd.args.size() != 2)
			{
				if(eval_imm_fn_arg(cmd.args[2]))
				{
					changed = true;
				}
			}
		}
		else
		if(cmd.cmd == L"XARG")
		{
			if(eval_imm_fn_arg(cmd.args[0]))
			{
				changed = true;
			}
		}
	}

	// +,A,0,B -> =,A,B
	// -,A,0,B -> =,A,B
	// *,A,1,B -> =,A,B
	// /,A,1,B -> =,A,B
	// %,A,1,B -> =,0,B
	// etc.
	for(auto &cmd: *this)
	{
		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(B1CUtils::is_bin_op(cmd) && cmd.args[0][0].type != B1Types::B1T_STRING && cmd.args[1][0].type != B1Types::B1T_STRING)
		{
			int imm_ind =	B1CUtils::is_num_val(cmd.args[0][0].value) ? 0 :
							B1CUtils::is_num_val(cmd.args[1][0].value) ? 1 : -1;

			if(imm_ind < 0)
			{
				continue;
			}

			int32_t n;

			if(Utils::str2int32(cmd.args[imm_ind][0].value, n) == B1_RES_OK)
			{
				Utils::correct_int_value(n, cmd.args[imm_ind][0].type);

				if(n == 0)
				{
					int var_ind = (imm_ind == 0) ? 1 : 0;

					if(cmd.cmd == L"+" || (cmd.cmd == L"-" && imm_ind == 1))
					{
						cmd.cmd = L"=";
						cmd.args.erase(cmd.args.begin() + imm_ind);
						changed = true;
					}
					else
					if(	(cmd.cmd == L"*" && !is_volatile_used(cmd.args[var_ind])) ||
						(cmd.cmd == L"/" && imm_ind == 0 && !is_volatile_used(cmd.args[1])) ||
						(cmd.cmd == L"%" && imm_ind == 0 && !is_volatile_used(cmd.args[1])))
					{
						cmd.cmd = L"=";
						cmd.args.erase(cmd.args.begin());
						cmd.args[0] = B1_CMP_ARG(L"0", B1Types::B1T_BYTE);
						changed = true;
					}
				}
				else
				if(n == 1)
				{
					if(cmd.cmd == L"*" || (cmd.cmd == L"/" && imm_ind == 1) || (cmd.cmd == L"%" && imm_ind == 1 && !is_volatile_used(cmd.args[0])))
					{
						cmd.cmd = L"=";
						cmd.args.erase(cmd.args.begin() + imm_ind);
						if(cmd.cmd == L"%")
						{
							cmd.args[0] = B1_CMP_ARG(L"0", B1Types::B1T_BYTE);
						}
						changed = true;
					}
				}
				else
				if(n == -1)
				{
					if(cmd.cmd == L"*" || (cmd.cmd == L"/" && imm_ind == 1))
					{
						cmd.cmd = L"-";
						cmd.args.erase(cmd.args.begin() + imm_ind);
						changed = true;
					}
				}
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

// collects MA statements, static and const GA statements
B1C_T_ERROR B1FileCompiler::check_MA_stmts()
{
	_MA_stmts.clear();

	for(auto i = cbegin(); i != cend(); i++)
	{
		const auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		_curr_line_cnt = cmd.line_cnt;
		_curr_line_num = cmd.line_num;
		_curr_src_line_id = cmd.src_line_id;

		if(cmd.cmd != L"MA" && !(cmd.cmd == L"GA" && cmd.args[1].size() > 1 && (cmd.args[1][1].value.find(L'S') != std::wstring::npos || cmd.args[1][1].value.find(L'C') != std::wstring::npos)))
		{
			continue;
		}

		bool is_ma = (cmd.cmd == L"MA");

		// check the statement
		for(auto a = cmd.args.begin() + (is_ma ? 3 : 2); a != cmd.args.end(); a++)
		{
			int32_t n;

			if(is_ma && ((*a)[0].type != B1Types::B1T_INT && (*a)[0].type != B1Types::B1T_WORD && (*a)[0].type != B1Types::B1T_BYTE && (*a)[0].type != B1Types::B1T_LONG))
			{
				return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
			}

			auto err = Utils::str2int32((*a)[0].value, n);
			if(err != B1_RES_OK)
			{
				return static_cast<B1C_T_ERROR>(err);
			}
		}

		// remove the statement from the main array (the statement is declarative, removing it enables more optimizations)
		_MA_stmts.push_back(cmd);
		erase(i--);
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::remove_DAT_stmts()
{
	std::map<std::wstring, std::wstring> dat_labels;

	_DAT_stmts.clear();

	// collect DAT statements
	for(auto i = begin(); i != end(); i++)
	{
		auto &cmd = *i;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(cmd.cmd == L"DAT")
		{
			std::wstring new_label;
			
			if(i != cbegin())
			{
				auto li = std::prev(i);
				
				while(B1CUtils::is_label(*li))
				{
					if(new_label.empty())
					{
						new_label = emit_label(true);
					}
					dat_labels.emplace(std::make_pair(li->cmd, new_label));

					if(li == cbegin())
					{
						break;
					}

					li--;
				}
			}
			if(!new_label.empty())
			{
				_DAT_stmts.emit_label(new_label);
			}

			// replace const variables' names with their values
			bool first = true;

			for(auto &v: cmd.args)
			{
				// the first arg is namespace name
				if(first)
				{
					first = false;
					continue;
				}

				if(!B1CUtils::is_imm_val(v[0].value))
				{
					_curr_line_cnt = cmd.line_cnt;
					_curr_line_num = cmd.line_num;
					_curr_src_line_id = cmd.src_line_id;

					std::wstring value;
					auto err = _compiler.get_const_var_value(v[0].value, value);
					if(err == static_cast<B1C_T_ERROR>(B1_RES_EUNKIDENT))
					{
						// maybe it's a predefined constant (not const variable)
						continue;
					}
					if(err != B1C_T_ERROR::B1C_RES_OK)
					{
						return err;
					}
					if(value.empty())
					{
						return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
					}

					if(v[0].type != B1Types::B1T_STRING)
					{
						int32_t n;

						auto err1 = Utils::str2int32(value, n);
						if(err1 != B1_RES_OK)
						{
							return static_cast<B1C_T_ERROR>(err1);
						}

						Utils::correct_int_value(n, v[0].type);
						value = std::to_wstring(n);
					}

					v[0].value = value;
				}
			}

			_DAT_stmts.push_back(cmd);
			erase(i--);
		}
	}

	// change labels of RST statements
	for(auto &cmd: *this)
	{
		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		_curr_line_cnt = cmd.line_cnt;
		_curr_line_num = cmd.line_num;
		_curr_src_line_id = cmd.src_line_id;

		if(cmd.cmd == L"RST" && cmd.args.size() > 1)
		{
			auto label = dat_labels.find(cmd.args[1][0].value);
			if(label != dat_labels.end())
			{
				cmd.args[1][0].value = label->second;
			}
			else
			{
				return B1C_T_ERROR::B1C_RES_ERSTWODAT;
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::remove_unused_vars(B1_CMP_ARG &a, bool &changed, bool subs_and_args_only /*= false*/)
{
	bool optimize = (a[0].type != B1Types::B1T_VARREF);

	// check arguments
	for(auto aa = a.begin() + 1; aa != a.end(); aa++)
	{
		auto used = _compiler.get_var_used(aa->value);

		// 0 - variable is not used
		// 1 - used for reading only
		// 2 - used for writing only
		// 3 - used for both read and write operations
		if(used == 1)
		{
			if(is_volatile_var(aa->value) || is_mem_var_name(aa->value) || is_const_var(aa->value))
			{
				// volatile fn argument or subscript
				optimize = false;
			}
			else
			{
				aa->value = (aa->type == B1Types::B1T_STRING) ? L"\"\"" : L"0";
				changed = true;
			}
		}
		else
		{
			optimize = optimize && !is_udef_used(*aa);
		}
	}

	if(!subs_and_args_only && optimize && _compiler.get_var_used(a[0].value) == 1 && !is_volatile_var(a[0].value) && !is_mem_var_name(a[0].value) && !is_const_var(a[0].value) && !is_udef_used(a))
	{
		a[0].value = (a[0].type == B1Types::B1T_STRING) ? L"\"\"" : L"0";
		a.erase(a.begin() + 1, a.end());
		changed = true;
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

// replaces variables used for reading only with their default values (in expressions)
// the function does not remove variables declarations
B1C_T_ERROR B1FileCompiler::remove_unused_vars(bool &changed)
{
	B1C_T_ERROR err;

	changed = false;

	for(auto &cmd: *this)
	{
		_curr_line_cnt = cmd.line_cnt;
		_curr_line_num = cmd.line_num;
		_curr_src_line_id = cmd.src_line_id;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(cmd.cmd == L"GA")
		{
			for(auto a = cmd.args.begin() + 2; a != cmd.args.end(); a++)
			{
				auto &arg = *a;
				err = remove_unused_vars(arg, changed);
				if(err != B1C_T_ERROR::B1C_RES_OK)
				{
					return err;
				}
			}

			continue;
		}

		if(cmd.cmd == L"RETVAL")
		{
			err = remove_unused_vars(cmd.args[0], changed);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"OUT")
		{
			err = remove_unused_vars(cmd.args[1], changed);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"IN")
		{
			err = remove_unused_vars(cmd.args[1], changed);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"READ")
		{
			err = remove_unused_vars(cmd.args[1], changed);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"GET" || cmd.cmd == L"PUT" || cmd.cmd == L"TRR")
		{
			if(cmd.args.size() != 2)
			{
				err = remove_unused_vars(cmd.args[2], changed);
				if(err != B1C_T_ERROR::B1C_RES_OK)
				{
					return err;
				}

				bool single_value = (cmd.args[2].size() == 1) && (cmd.args[2][0].value == L"1");
				if(single_value)
				{
					cmd.args.pop_back();
					changed = true;
				}
			}

			err = remove_unused_vars(cmd.args[1], changed, (cmd.args.size() != 2));
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"SET")
		{
			err = remove_unused_vars(cmd.args[1], changed);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"XARG")
		{
			err = remove_unused_vars(cmd.args[0], changed);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"IOCTL")
		{
			if(cmd.args.size() > 2)
			{
				err = remove_unused_vars(cmd.args[2], changed);
				if(err != B1C_T_ERROR::B1C_RES_OK)
				{
					return err;
				}
			}

			continue;
		}

		if(B1CUtils::is_un_op(cmd) || B1CUtils::is_log_op(cmd))
		{
			err = remove_unused_vars(cmd.args[0], changed);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			err = remove_unused_vars(cmd.args[1], changed);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(B1CUtils::is_bin_op(cmd))
		{
			err = remove_unused_vars(cmd.args[0], changed);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			err = remove_unused_vars(cmd.args[1], changed);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			err = remove_unused_vars(cmd.args[2], changed);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::get_const_var_value(const std::wstring &var_name, bool &var_found, std::wstring &value)
{
	var_found = false;
	value.clear();

	auto ci = _const_init.find(var_name);
	if(ci != _const_init.cend())
	{
		var_found = true;
		if(ci->second.second.size() != 1)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
		}
		if(B1CUtils::is_imm_val(ci->second.second[0]))
		{
			value = ci->second.second[0];
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::eval_const_vars_values_1_iter(bool &changed, bool &all_resolved)
{
	for(auto &ci: _const_init)
	{
		for(auto &civ: ci.second.second)
		{
			if(!B1CUtils::is_imm_val(civ))
			{
				std::wstring value;

				auto err = _compiler.get_const_var_value(civ, value);
				if(err != B1C_T_ERROR::B1C_RES_OK)
				{
					return err;
				}

				if(!value.empty())
				{
					if(B1CUtils::is_imm_val(value))
					{
						if(!B1CUtils::is_str_val(value))
						{
							int32_t n;

							auto err1 = Utils::str2int32(value, n);
							if(err1 != B1_RES_OK)
							{
								return static_cast<B1C_T_ERROR>(err1);
							}

							Utils::correct_int_value(n, ci.second.first);
							value = std::to_wstring(n);
						}
					}
					else
					{
						all_resolved = false;
					}

					civ = value;
					changed = true;
				}
				else
				{
					all_resolved = false;
				}
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::calc_vars_usage(B1_TYPED_VALUE &v, bool read)
{
	if(v.value.empty())
	{
		// omitted function argument
		return B1C_T_ERROR::B1C_RES_OK;
	}

	if(B1CUtils::is_num_val(v.value))
	{
		// num. value
		return B1C_T_ERROR::B1C_RES_OK;
	}

	if(B1CUtils::is_str_val(v.value))
	{
		// string value
		return B1C_T_ERROR::B1C_RES_OK;
	}

	// skip function arguments
	if(B1CUtils::is_fn_arg(v.value))
	{
		return B1C_T_ERROR::B1C_RES_OK;
	}

	if(is_gen_local(v.value))
	{
		// local variable
		return B1C_T_ERROR::B1C_RES_OK;
	}

	if(Utils::check_const_name(v.value))
	{
		// a predefined constant
		return B1C_T_ERROR::B1C_RES_OK;
	}

	auto fn = get_fn(v);
	if(fn != nullptr)
	{
		// function with zero arguments
		return B1C_T_ERROR::B1C_RES_OK;
	}

	// simple variable
	_compiler.mark_var_used(v.value, read);

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::calc_vars_usage(B1_CMP_ARG &a, bool read)
{
	if(a.size() == 1)
	{
		return calc_vars_usage(a[0], read);
	}

	// process arguments first
	for(auto aa = a.begin() + 1; aa != a.end(); aa++)
	{
		auto err = calc_vars_usage(*aa, true);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			return err;
		}
	}

	if(Utils::check_const_name(a[0].value))
	{
		// a predefined constant
		return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
	}

	auto fn = get_fn(a);
	if(fn != nullptr)
	{
		// function with some arguments
		return B1C_T_ERROR::B1C_RES_OK;
	}

	// subscripted variable
	_compiler.mark_var_used(a[0].value, read);

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::calc_vars_usage()
{
	B1C_T_ERROR err;

	for(auto &cmd: *this)
	{
		// restore code line identification values (to display error position properly)
		_curr_line_cnt = cmd.line_cnt;
		_curr_line_num = cmd.line_num;
		_curr_src_line_id = cmd.src_line_id;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(cmd.cmd == L"GF" || cmd.cmd == L"LA" || cmd.cmd == L"LF" || cmd.cmd == L"NS" ||
			cmd.cmd == L"CALL" || cmd.cmd == L"JMP" || cmd.cmd == L"JF" || cmd.cmd == L"JT" ||
			cmd.cmd == L"END" || cmd.cmd == L"RET" || cmd.cmd == L"DAT" || cmd.cmd == L"RST" ||
			cmd.cmd == L"ERR" || cmd.cmd == L"DEF" || cmd.cmd == L"INT")
		{
			continue;
		}

		if(B1CUtils::is_log_op(cmd))
		{
			err = calc_vars_usage(cmd.args[0], true);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}
			err = calc_vars_usage(cmd.args[1], true);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}
		}
		else
		if(B1CUtils::is_un_op(cmd))
		{
			err = calc_vars_usage(cmd.args[0], true);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			err = calc_vars_usage(cmd.args[1], false);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}
		}
		else
		if(B1CUtils::is_bin_op(cmd))
		{
			err = calc_vars_usage(cmd.args[0], true);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			err = calc_vars_usage(cmd.args[1], true);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			err = calc_vars_usage(cmd.args[2], false);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}
		}
		else
		{
			// PUT, GET and TRR is a special case
			if(cmd.cmd == L"GET" || cmd.cmd == L"PUT" || cmd.cmd == L"TRR")
			{
				bool read = (cmd.cmd == L"PUT");

				err = calc_vars_usage(cmd.args[1], read);
				if(err != B1C_T_ERROR::B1C_RES_OK)
				{
					return err;
				}

				if(cmd.args.size() != 2)
				{
					err = calc_vars_usage(cmd.args[2], true);
					if(err != B1C_T_ERROR::B1C_RES_OK)
					{
						return err;
					}
				}

				continue;
			}

			for(auto a = cmd.args.begin(); a != cmd.args.end(); a++)
			{
				bool read = true;

				if(cmd.cmd == L"GA" && a < cmd.args.begin() + 2)
				{
					continue;
				}
				if(cmd.cmd == L"MA" && a < cmd.args.begin() + 3)
				{
					continue;
				}
				if(cmd.cmd == L"RETVAL" && a != cmd.args.begin())
				{
					continue;
				}
				if(cmd.cmd == L"OUT" && a != cmd.args.begin() + 1)
				{
					continue;
				}
				if(cmd.cmd == L"IN" && a != cmd.args.begin() + 1)
				{
					continue;
				}
				if(cmd.cmd == L"SET" && a != cmd.args.begin() + 1)
				{
					continue;
				}
				if(cmd.cmd == L"IOCTL" && a != cmd.args.begin() + 2)
				{
					continue;
				}
				if(cmd.cmd == L"READ" && a == cmd.args.begin())
				{
					continue;
				}

				if(cmd.cmd == L"IN" || cmd.cmd == L"READ")
				{
					read = false;
				}

				err = calc_vars_usage(*a, read);
				if(err != B1C_T_ERROR::B1C_RES_OK)
				{
					return err;
				}
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::optimize_GA_GF(bool &changed)
{
	changed = false;

	for(auto i = cbegin(); i != cend(); i++)
	{
		auto &cmd = *i;

		_curr_line_cnt = cmd.line_cnt;
		_curr_line_num = cmd.line_num;
		_curr_src_line_id = cmd.src_line_id;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		// delete all non-volatile GA statements for simple variables
		/*if(cmd.cmd == L"GA" && !is_volatile_var(cmd.args[0][0].value) && cmd.args.size() == 2)
		{
			auto j = std::next(i);
			erase(i);
			i = std::prev(j);
			changed = true;
			break;
		}*/

		if(cmd.cmd == L"GA" && !is_volatile_var(cmd.args[0][0].value) && !is_const_var(cmd.args[0][0].value) && cmd.args.size() == 2)
		{
			for(auto j = std::next(i); j != cend(); j++)
			{
				auto &cmd1 = *j;

				if(B1CUtils::is_label(cmd1))
				{
					continue;
				}

				if(cmd1.cmd == L"JMP" || cmd1.cmd == L"JT" || cmd1.cmd == L"JF" || cmd1.cmd == L"CALL" || cmd1.cmd == L"RET" || cmd1.cmd == L"RETVAL" || cmd1.cmd == L"END")
				{
					break;
				}

				if(B1CUtils::is_src(cmd1, cmd.args[0][0].value) || B1CUtils::is_sub_or_arg(cmd1, cmd.args[0][0].value) || is_udef_used(cmd1))
				{
					break;
				}

				if(B1CUtils::is_dst(cmd1, cmd.args[0][0].value))
				{
					j = std::next(i);
					erase(i);
					i = std::prev(j);
					changed = true;
					break;
				}
			}
		}

		if(cmd.cmd == L"GF" && !is_volatile_var(cmd.args[0][0].value) && get_var_dim(cmd.args[0][0].value) == 0)
		{
			// remove assignments to the variable preceedeing to the GF
			for(auto j = std::prev(i); ; j--)
			{
				auto &cmd1 = *j;

				if(B1CUtils::is_label(cmd1))
				{
					continue;
				}

				if(cmd1.cmd == L"JMP" || cmd1.cmd == L"JT" || cmd1.cmd == L"JF" || cmd1.cmd == L"CALL" || cmd1.cmd == L"RET" || cmd1.cmd == L"RETVAL" || cmd1.cmd == L"END")
				{
					break;
				}

				if(is_udef_used(cmd1))
				{
					break;
				}

				if(B1CUtils::is_dst(cmd1, cmd.args[0][0].value) && !is_volatile_used(cmd1) && cmd1.cmd != L"IN" && cmd1.cmd != L"READ" && cmd1.cmd != L"GET" && cmd1.cmd != L"TRR")
				{
					auto j1 = std::next(j);
					erase(j);
					j = j1;
					changed = true;
					continue;
				}

				if(B1CUtils::is_src(cmd1, cmd.args[0][0].value) || B1CUtils::is_sub_or_arg(cmd1, cmd.args[0][0].value))
				{
					break;
				}

				if(j == cbegin())
				{
					break;
				}
			}

			for(auto j = std::next(i); j != cend(); j++)
			{
				auto &cmd1 = *j;

				if(B1CUtils::is_label(cmd1))
				{
					continue;
				}

				if(cmd1.cmd == L"JMP" || cmd1.cmd == L"JT" || cmd1.cmd == L"JF" || cmd1.cmd == L"CALL" || cmd1.cmd == L"RET" || cmd1.cmd == L"RETVAL" || cmd1.cmd == L"END")
				{
					break;
				}

				if(B1CUtils::is_src(cmd1, cmd.args[0][0].value) || B1CUtils::is_sub_or_arg(cmd1, cmd.args[0][0].value) || is_udef_used(cmd1))
				{
					break;
				}

				if(B1CUtils::is_dst(cmd1, cmd.args[0][0].value))
				{
					j = std::next(i);
					erase(i);
					i = std::prev(j);
					changed = true;
					break;
				}
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

bool B1FileCompiler::get_opt_explicit() const
{
	return _opt_explicit;
}

B1C_T_ERROR B1FileCompiler::set_opt_explicit()
{
	_opt_explicit = true;

	return B1C_T_ERROR::B1C_RES_OK;
}

bool B1FileCompiler::get_opt_base1() const
{
	return _opt_base1;
}

B1C_T_ERROR B1FileCompiler::set_opt_base1()
{
	_opt_base1 = true;

	return B1C_T_ERROR::B1C_RES_OK;
}

bool B1FileCompiler::get_opt_nocheck() const
{
	return _opt_nocheck;
}

B1C_T_ERROR B1FileCompiler::set_opt_nocheck()
{
	_opt_nocheck = true;

	return B1C_T_ERROR::B1C_RES_OK;
}

B1FileCompiler::B1FileCompiler(B1Compiler &compiler, const std::wstring &name_space, bool no_opt, bool out_src_lines)
: B1_CMP_CMDS(name_space)
, _compiler(compiler)
, _no_opt(no_opt)
, _out_src_lines(out_src_lines)
, _opt_explicit_def(true)
, _opt_explicit(false)
, _opt_base1_def(true)
, _opt_base1(false)
, _opt_nocheck_def(true)
, _opt_nocheck(false)
, _opt_inputdevice_def(true)
, _opt_outputdevice_def(true)
{
}

B1FileCompiler::~B1FileCompiler()
{
	// free memory occupied by program file
	b1_ex_prg_set_prog_file(NULL);
}

B1C_T_ERROR B1FileCompiler::Load(const std::string &file_name)
{
	b1_reset();

	_int_name = _global_settings.GetInterruptName(file_name, _file_name);

	return static_cast<B1C_T_ERROR>(b1_ex_prg_set_prog_file(_file_name.c_str()));
}

B1C_T_ERROR B1FileCompiler::FirstRun()
{
	B1_T_ERROR err;
	B1C_T_ERROR err1;
	uint8_t stmt;
	bool endstmt;
	bool option_allowed;

	// the first run to get all user-defined function names
	endstmt = false;
	option_allowed = true;

	err = B1_RES_OK;
	err1 = B1C_T_ERROR::B1C_RES_OK;

	while(true)
	{
		err = b1_ex_prg_get_prog_line(B1_T_LINE_NUM_NEXT);
		// do not treat B1_RES_EPROGUNEND as error in this case: just program end
		if(err == B1_RES_EPROGUNEND)
		{
			break;
		}

		if(err != B1_RES_OK)
		{
			break;
		}

		b1_curr_prog_line_offset = 0;

		err = b1_tok_stmt_init(&stmt);
		if(err != B1_RES_OK)
		{
			break;
		}

		if(stmt == B1_ID_STMT_ABSENT || stmt == B1_ID_STMT_REM)
		{
			continue;
		}

		if(stmt == B1_ID_STMT_OPTION)
		{
			if(!option_allowed)
			{
				return static_cast<B1C_T_ERROR>(B1_RES_EINVSTAT);
			}

			err1 = st_option(true);
			if(err1 != B1C_T_ERROR::B1C_RES_OK)
			{
				break;
			}

			continue;
		}

		option_allowed = false;

		if(stmt == B1_ID_STMT_DEF)
		{
			err = st_def(true);
			if(err != B1_RES_OK)
			{
				break;
			}

			continue;
		}

		if(stmt == B1_ID_STMT_DIM)
		{
			err1 = st_dim(true);
			if(err1 != B1C_T_ERROR::B1C_RES_OK)
			{
				break;
			}

			continue;
		}

		if(stmt == B1_ID_STMT_END)
		{
			if(endstmt)
			{
				//warning: using multiple END statements is not recommended
				_warnings[b1_curr_prog_line_cnt].push_back(B1C_T_WARNING::B1C_WRN_WMULTEND);
			}

			endstmt = true;
		}
	}

	if(err != B1_RES_EPROGUNEND && err != B1_RES_OK)
	{
		err1 = static_cast<B1C_T_ERROR>(err);
	}

	if(err1 != B1C_T_ERROR::B1C_RES_OK)
	{
		return err1;
	}

	if(!endstmt)
	{
		return static_cast<B1C_T_ERROR>(B1_RES_EPROGUNEND);
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::Compile()
{
	B1_T_ERROR err;
	B1C_T_ERROR err1;
	uint8_t stmt;
	B1_T_LINE_NUM prev_line_n;
	std::vector<std::pair<B1_T_PROG_LINE_CNT, int32_t>> defs;

	// compile
	_state.first = B1_CMP_STATE::B1_CMP_STATE_OK;
	_state.second.clear();

	prev_line_n = B1_T_LINE_NUM_ABSENT;

	_curr_src_line_id = -1;

	// emit NS statement
	emit_command(L"NS", _curr_name_space);

	if(!_int_name.empty())
	{
		emit_command(L"INT", std::wstring(_int_name.cbegin(), _int_name.cend()));
	}

	err = B1_RES_OK;
	err1 = B1C_T_ERROR::B1C_RES_OK;

	while(true)
	{
		_curr_src_line_id++;

		err = b1_ex_prg_get_prog_line(B1_T_LINE_NUM_NEXT);
		// do not treat B1_RES_EPROGUNEND as error in this case: just program end
		if(err == B1_RES_EPROGUNEND)
		{
			break;
		}

		if(err != B1_RES_OK)
		{
			break;
		}

		_src_lines[_curr_src_line_id] = B1CUtils::b1str_to_cstr(b1_progline, true);

		b1_curr_prog_line_offset = 0;

		err = b1_tok_stmt_init(&stmt);
		if(err != B1_RES_OK)
		{
			break;
		}

		// check line number
		if(b1_next_line_num != B1_T_LINE_NUM_ABSENT)
		{
			if(prev_line_n != B1_T_LINE_NUM_ABSENT && prev_line_n >= b1_next_line_num)
			{
				err = B1_RES_EINVLINEN;
				break;
			}

			prev_line_n = b1_next_line_num;
		}

		_curr_line_num = -1;
		_curr_line_cnt = b1_curr_prog_line_cnt;

		if(b1_next_line_num != B1_T_LINE_NUM_ABSENT)
		{
			_curr_line_num = b1_next_line_num;
			emit_label(L"__ULB_" + std::to_wstring(b1_next_line_num));
		}

		if(stmt == B1_ID_STMT_ABSENT)
		{
			continue;
		}

		if(stmt == B1_ID_STMT_REM)
		{
			continue;
		}

		if(stmt == B1_ID_STMT_ELSE)
		{
			if(_state.first != B1_CMP_STATE::B1_CMP_STATE_IF && _state.first != B1_CMP_STATE::B1_CMP_STATE_ELSEIF)
			{
				err = B1_RES_EELSEWOIF;
				break;
			}

			_state.first = B1_CMP_STATE::B1_CMP_STATE_ELSE;

			err1 = st_if();
			if(err1 != B1C_T_ERROR::B1C_RES_OK)
			{
				break;
			}

			err = st_if_end();
			if(err != B1_RES_OK)
			{
				break;
			}

			_state = _state_stack.back();
			_state_stack.pop_back();

			continue;
		}

		if(stmt == B1_ID_STMT_ELSEIF)
		{
			if(_state.first != B1_CMP_STATE::B1_CMP_STATE_IF && _state.first != B1_CMP_STATE::B1_CMP_STATE_ELSEIF)
			{
				err = B1_RES_EELSEWOIF;
				break;
			}

			_state.first = B1_CMP_STATE::B1_CMP_STATE_ELSEIF;

			err1 = st_if();
			if(err1 != B1C_T_ERROR::B1C_RES_OK)
			{
				break;
			}

			continue;
		}

		if(_state.first == B1_CMP_STATE::B1_CMP_STATE_IF || _state.first == B1_CMP_STATE::B1_CMP_STATE_ELSEIF)
		{
			err = st_if_end();
			if(err != B1_RES_OK)
			{
				break;
			}

			_state = _state_stack.back();
			_state_stack.pop_back();
		}

		if(stmt == B1_ID_STMT_IF)
		{
			_state_stack.push_back(_state);
			_state.first = B1_CMP_STATE::B1_CMP_STATE_IF;
			_state.second.clear();

			err1 = st_if();
			if(err1 != B1C_T_ERROR::B1C_RES_OK)
			{
				break;
			}

			continue;
		}

		if(stmt == B1_ID_STMT_FOR)
		{
			_state_stack.push_back(_state);
			_state.first = B1_CMP_STATE::B1_CMP_STATE_FOR;
			_state.second.clear();

			err = st_for();
			if(err != B1_RES_OK)
			{
				break;
			}

			continue;
		}

		if(stmt == B1_ID_STMT_NEXT)
		{
			if(_state.first != B1_CMP_STATE::B1_CMP_STATE_FOR)
			{
				err = B1_RES_ENXTWOFOR;
				break;
			}

			err = st_next();
			if(err != B1_RES_OK)
			{
				break;
			}

			_state = _state_stack.back();
			_state_stack.pop_back();

			continue;
		}

		if(stmt == B1_ID_STMT_DATA)
		{
			err = st_data();
			if(err != B1_RES_OK)
			{
				break;
			}

			continue;
		}

		if(stmt == B1_ID_STMT_READ)
		{
			err = st_read();
			if(err != B1_RES_OK)
			{
				break;
			}

			continue;
		}

		if(stmt == B1_ID_STMT_RESTORE)
		{
			err = st_restore();
			if(err != B1_RES_OK)
			{
				break;
			}

			continue;
		}

		if(stmt == B1_ID_STMT_WHILE)
		{
			_state_stack.push_back(_state);
			_state.first = B1_CMP_STATE::B1_CMP_STATE_WHILE;
			_state.second.clear();

			err = st_while();
			if(err != B1_RES_OK)
			{
				break;
			}

			continue;
		}

		if(stmt == B1_ID_STMT_WEND)
		{
			if(_state.first != B1_CMP_STATE::B1_CMP_STATE_WHILE)
			{
				err = B1_RES_EWNDWOWHILE;
				break;
			}

			err = st_wend();
			if(err != B1_RES_OK)
			{
				break;
			}

			_state = _state_stack.back();
			_state_stack.pop_back();

			continue;
		}

		if(stmt == B1_ID_STMT_DEF)
		{
			defs.push_back(std::make_pair(b1_curr_prog_line_cnt, _curr_src_line_id));
			continue;
		}

		err1 = compile_simple_stmt(stmt);
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
	}

	if(err != B1_RES_EPROGUNEND && err != B1_RES_OK)
	{
		err1 = static_cast<B1C_T_ERROR>(err);
	}

	if(err1 != B1C_T_ERROR::B1C_RES_OK)
	{
		return err1;
	}

	if(_state.first != B1_CMP_STATE::B1_CMP_STATE_OK)
	{
		if(_state.first == B1_CMP_STATE::B1_CMP_STATE_FOR)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_EFORWONXT);
		}
		else
		if(_state.first == B1_CMP_STATE::B1_CMP_STATE_WHILE)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_EWHILEWOWND);
		}

		return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
	}

	// process DEF statements
	for(const auto &def: defs)
	{
		b1_curr_prog_line_cnt = def.first - 1;
		_curr_src_line_id = def.second;

		err = b1_ex_prg_get_prog_line(B1_T_LINE_NUM_NEXT);
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}

		b1_curr_prog_line_offset = 0;

		err = b1_tok_stmt_init(&stmt);
		if(err != B1_RES_OK)
		{
			break;
		}

		_curr_line_num = -1;
		_curr_line_cnt = b1_curr_prog_line_cnt;

		if(b1_next_line_num != B1_T_LINE_NUM_ABSENT)
		{
			_curr_line_num = b1_next_line_num;
		}

		err = st_def(false);
		if(err != B1_RES_OK)
		{
			return static_cast<B1C_T_ERROR>(err);
		}
	}

	// fix LA/LF statements order to correspond to LIFO stack
	fix_LA_LF_order();

	// some basic optimizations
	err1 = B1C_T_ERROR::B1C_RES_OK;

	bool changed;
	bool stop = false;

	while(!stop)
	{
		stop = true;

		err1 = remove_unused_labels(changed);
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err1 = remove_duplicate_labels(changed);
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err1 = remove_jumps(changed);
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err1 = replace_unary_minus(changed);
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err1 = remove_locals(changed);
		if(err1 != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}
	}

	return err1;
}

B1C_T_ERROR B1FileCompiler::put_fn_def_values(B1_CMP_ARG &arg)
{
	if(fn_exists(arg[0].value))
	{
		auto fn = get_fn(arg);

		if(fn == nullptr)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
		}
		else
		{
			for(int i = 0; i < fn->args.size(); i++)
			{
				if(arg[i + 1].value.empty())
				{
					if(fn->args[i].optional)
					{
						arg[i + 1].type = fn->args[i].type;
						arg[i + 1].value = fn->args[i].defval;
					}
				}
			}
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::put_fn_def_values()
{
	B1C_T_ERROR err = B1C_T_ERROR::B1C_RES_OK;

	// find all function calls with omitted arguments and put their default values
	for(auto &cmd: *this)
	{
		_curr_line_cnt = cmd.line_cnt;
		_curr_line_num = cmd.line_num;
		_curr_src_line_id = cmd.src_line_id;

		if(B1CUtils::is_label(cmd))
		{
			continue;
		}

		if(cmd.cmd == L"GA")
		{
			for(auto a = cmd.args.begin() + 2; a != cmd.args.end(); a++)
			{
				auto &arg = *a;
				err = put_fn_def_values(arg);
				if(err != B1C_T_ERROR::B1C_RES_OK)
				{
					return err;
				}
			}

			continue;
		}

		if(cmd.cmd == L"RETVAL")
		{
			err = put_fn_def_values(cmd.args[0]);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"OUT")
		{
			err = put_fn_def_values(cmd.args[1]);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"GET" || cmd.cmd == L"PUT" || cmd.cmd == L"TRR")
		{
			if(cmd.cmd == L"PUT")
			{
				err = put_fn_def_values(cmd.args[1]);
				if(err != B1C_T_ERROR::B1C_RES_OK)
				{
					return err;
				}
			}

			if(cmd.args.size() != 2)
			{
				err = put_fn_def_values(cmd.args[2]);
				if(err != B1C_T_ERROR::B1C_RES_OK)
				{
					return err;
				}
			}

			continue;
		}

		if(cmd.cmd == L"SET")
		{
			err = put_fn_def_values(cmd.args[1]);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"XARG")
		{
			err = put_fn_def_values(cmd.args[0]);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(cmd.cmd == L"IOCTL")
		{
			if(cmd.args.size() > 2)
			{
				err = put_fn_def_values(cmd.args[2]);
				if(err != B1C_T_ERROR::B1C_RES_OK)
				{
					return err;
				}
			}

			continue;
		}

		if(B1CUtils::is_un_op(cmd))
		{
			err = put_fn_def_values(cmd.args[0]);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}

		if(B1CUtils::is_bin_op(cmd) || B1CUtils::is_log_op(cmd))
		{
			err = put_fn_def_values(cmd.args[0]);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			err = put_fn_def_values(cmd.args[1]);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}

			continue;
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

std::wstring B1FileCompiler::replace_type_spec(const std::wstring &token) const
{
	if(token.empty())
	{
		return L"";
	}
	if(token.back() == L'$')
	{
		return B1_CMP_FNS::fn_exists(token) ? token: (std::wstring(token.cbegin(), std::prev(token.cend())) + L"_S");
	}
	if(token.back() == L'%')
	{
		return std::wstring(token.cbegin(), std::prev(token.cend())) + (B1CUtils::is_num_val(token) ? L"" : L"_I");
	}
	return token;
}

// puts values and variables types, functions types and def. values and makes some essential optimzations
B1C_T_ERROR B1FileCompiler::PutTypesAndOptimize()
{
	B1C_T_ERROR err = B1C_T_ERROR::B1C_RES_OK;
	bool stop, changed;

	err = put_types();
	if(err != B1C_T_ERROR::B1C_RES_OK)
	{
		return err;
	}

	err = put_fn_def_values();
	if(err != B1C_T_ERROR::B1C_RES_OK)
	{
		return err;
	}

	// set proper references' names
	for(auto &vr: _var_refs)
	{
		auto v = _var_names.find(vr.first);
		if(v != _var_names.cend())
		{
			vr.second.first = v->second;
		}

		for(auto &c: vr.second.second)
		{
			if(c->cmd == L"IOCTL")
			{
				for(auto &a: c->args)
				{
					if(a[0].type == B1Types::B1T_VARREF && a[0].value == vr.first)
					{
						a[0].value = vr.second.first;
					}
				}
			}
		}
	}

	stop = false;

	while(!stop)
	{
		stop = true;

		err = inline_fns(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_unused_labels(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_duplicate_labels(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_self_assigns(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_duplicate_assigns(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_jumps(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = eval_unary_ops(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_locals(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = reuse_locals(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = eval_imm_exps(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}
	}

	if(err != B1C_T_ERROR::B1C_RES_OK)
	{
		return err;
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::Optimize(bool init)
{
	B1C_T_ERROR err;
	bool stop, changed;

	stop = false;

	while(!stop)
	{
		stop = true;

		err = remove_unused_labels(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_duplicate_labels(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_self_assigns(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_duplicate_assigns(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_jumps(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_redundant_comparisons(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_duplicate_assigns(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = eval_unary_ops(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = reuse_imm_values(init, changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_locals(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = reuse_locals(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = reuse_vars(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = eval_imm_exps(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_unused_vars(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = inline_fns(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = optimize_GA_GF(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}
	}

	return err;
}

B1C_T_ERROR B1FileCompiler::CollectDeclStmts()
{
	auto err = check_MA_stmts();
	if(err != B1C_T_ERROR::B1C_RES_OK)
	{
		return err;
	}

	err = remove_DAT_stmts();
	if(err != B1C_T_ERROR::B1C_RES_OK)
	{
		return err;
	}

	bool changed, stop = false;
	while(!stop)
	{
		stop = true;

		err = remove_unused_labels(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}

		err = remove_jumps(changed);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			break;
		}
		if(changed)
		{
			stop = false;
		}
	}

	return err;
}

B1C_T_ERROR B1FileCompiler::WriteUFns(const std::string &file_name) const
{
	std::FILE *ofp = std::fopen(file_name.c_str(), "a");
	if(ofp == nullptr)
	{
		return B1C_T_ERROR::B1C_RES_EFOPEN;
	}

	for(const auto &ufn: _ufns)
	{
		std::fwprintf(ofp, L"DEF,%ls,%ls", replace_type_spec(ufn.second.iname).c_str(), Utils::get_type_name(ufn.second.rettype).c_str());
		for(const auto &arg: ufn.second.args)
		{
			std::fwprintf(ofp, L",%ls", Utils::get_type_name(arg.type).c_str());
		}
		std::fwprintf(ofp, L"\n");
	}

	std::fclose(ofp);

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::WriteStmt(const B1_CMP_CMD &cmd, std::FILE *ofp, int &curr_line_id) const
{
	if(_out_src_lines)
	{
		if(curr_line_id != cmd.src_line_id)
		{
			curr_line_id = cmd.src_line_id;

			if(curr_line_id >= 0)
			{
				const auto ln = _src_lines.find(curr_line_id);
				if(std::fwprintf(ofp, L";%ls\n", ln->second.c_str()) < 0)
				{
					return B1C_T_ERROR::B1C_RES_EFWRITE;
				}
			}
		}
	}

	if(B1CUtils::is_label(cmd))
	{
		if(std::fwprintf(ofp, L":%ls", replace_type_spec(cmd.cmd).c_str()) < 0)
		{
			return B1C_T_ERROR::B1C_RES_EFWRITE;
		}
	}
	else
	{
		std::wstring sep;

		if(std::fwprintf(ofp, L"%ls", cmd.cmd.c_str()) < 0)
		{
			return B1C_T_ERROR::B1C_RES_EFWRITE;
		}

		for(auto arg = cmd.args.cbegin(); arg != cmd.args.cend(); arg++)
		{
			for(auto ai = arg->cbegin(); ai != arg->cend(); ai++)
			{
				sep = (ai == std::next(arg->cbegin())) ? L"(" : L",";
				if(std::fwprintf(ofp, L"%ls%ls", sep.c_str(), replace_type_spec(ai->value).c_str()) < 0)
				{
					return B1C_T_ERROR::B1C_RES_EFWRITE;
				}

				if(ai->type != B1Types::B1T_UNKNOWN && !((cmd.cmd == L"LA" && arg == std::next(cmd.args.cbegin())) || ((cmd.cmd == L"GA" || cmd.cmd == L"MA") && arg == std::next(cmd.args.cbegin())) || (cmd.cmd == L"RETVAL" && arg == std::next(cmd.args.cbegin()))))
				{
					if(std::fwprintf(ofp, L"<%ls>", Utils::get_type_name(ai->type).c_str()) < 0)
					{
						return B1C_T_ERROR::B1C_RES_EFWRITE;
					}
				}
			}

			if(arg->size() > 1)
			{
				if(std::fwprintf(ofp, L")") < 0)
				{
					return B1C_T_ERROR::B1C_RES_EFWRITE;
				}
			}
		}
	}

	if(std::fwprintf(ofp, L"\n") < 0)
	{
		return B1C_T_ERROR::B1C_RES_EFWRITE;
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::WriteMAs(const std::string &file_name)
{
	std::FILE *ofp = std::fopen(file_name.c_str(), "a");
	if(ofp == nullptr)
	{
		return B1C_T_ERROR::B1C_RES_EFOPEN;
	}

	int line_id = -1;
	for(const auto &c: _MA_stmts)
	{
		bool is_const = is_const_var(c.args[0][0].value);

		// remove unused variables
		auto used = _compiler.get_var_used(c.args[0][0].value);
		if(used == 0)
		{
			continue;
		}

		auto err = WriteStmt(c, ofp, line_id);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			std::fclose(ofp);
			return err;
		}

		if(is_const)
		{
			// create DAT statement for const array variable
			auto init = _const_init.find(c.args[0][0].value);
			if(init == _const_init.cend())
			{
				init = _compiler._global_const_init.find(c.args[0][0].value);
			}

			// copy _curr_line_num, _curr_line_cnt, _curr_src_file_id, _curr_src_line_id members from GA statement
			B1_CMP_CMD dc(c);
			dc.cmd = L"DAT";
			dc.args.clear();
			dc.args.push_back(c.args[0][0].value);
			for(const auto &iv: init->second.second)
			{
				dc.args.push_back(B1_CMP_ARG(iv, init->second.first));
			}
			_DAT_stmts.push_back(dc);
		}
	}

	std::fclose(ofp);

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::WriteDATs(const std::string &file_name) const
{
	std::FILE *ofp = std::fopen(file_name.c_str(), "a");
	if(ofp == nullptr)
	{
		return B1C_T_ERROR::B1C_RES_EFOPEN;
	}

	int line_id = -1;
	for(const auto &c: _DAT_stmts)
	{
		auto err = WriteStmt(c, ofp, line_id);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			std::fclose(ofp);
			return err;
		}
	}

	std::fclose(ofp);

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1FileCompiler::Write(const std::string &file_name) const
{
	std::FILE *ofp = std::fopen(file_name.c_str(), "a");
	if(ofp == nullptr)
	{
		return B1C_T_ERROR::B1C_RES_EFOPEN;
	}

	int line_id = -1;

	for(const auto &c: *this)
	{
		// remove unused variables
		if(!B1CUtils::is_label(c) && (c.cmd == L"GA" || c.cmd == L"GF"))
		{
			auto used = _compiler.get_var_used(c.args[0][0].value);
			if(used == 0)
			{
				continue;
			}
		}

		auto err = WriteStmt(c, ofp, line_id);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			std::fclose(ofp);
			return err;
		}
	}

	std::fclose(ofp);

	return B1C_T_ERROR::B1C_RES_OK;
}


bool B1Compiler::global_var_check(bool is_global, bool is_mem_var, bool is_static, bool is_const, const std::wstring &name) const
{
	auto var = _global_var_names.find(name);

	if(var != _global_var_names.end())
	{
		// forbid multiple DIMs for memory variables
		if(is_mem_var || is_static || is_const)
		{
			return false;
		}

		// forbid declaring global and local variables with the same name
		if(!is_global)
		{
			return false;
		}
	}

	return true;
}

B1C_T_ERROR B1Compiler::put_global_var_name(const std::wstring &name, const B1Types type, int dims, bool is_volatile, bool is_mem_var, bool is_static, bool is_const)
{
	// forbid multiple DIMs for memory variables
	if(!global_var_check(true, is_mem_var, is_static, is_const, name))
	{
		return static_cast<B1C_T_ERROR>(B1_RES_EIDINUSE);
	}

	auto var = _global_var_names.find(name);

	if(var != _global_var_names.end())
	{
		auto gen_name = var->second;
		auto var1 = _global_vars.find(gen_name);

		if(std::get<0>(var1->second) != type)
		{
			return B1C_T_ERROR::B1C_RES_EVARTYPMIS;
		}

		if(std::get<2>(var1->second) != is_volatile)
		{
			return B1C_T_ERROR::B1C_RES_EVARTYPMIS;
		}

		if(std::get<1>(var1->second) != dims)
		{
			return B1C_T_ERROR::B1C_RES_EVARDIMMIS;
		}
	}
	else
	{
		auto gen_name = (is_mem_var ? L"__MEM_" : L"__VAR_") + name;

		_global_var_names[name] = gen_name;
		_global_vars[gen_name] = std::make_tuple(type, dims, is_volatile, is_mem_var, is_static, is_const);
	}
	return B1C_T_ERROR::B1C_RES_OK;
}

std::wstring B1Compiler::get_global_var_name(const std::wstring &name) const
{
	auto g = _global_var_names.find(name);
	return (g == _global_var_names.end()) ? std::wstring() : g->second;
}

bool B1Compiler::is_global_mem_var_name(const std::wstring &name) const
{
	auto g = _global_vars.find(name);
	return (g != _global_vars.end() && std::get<3>(g->second));
}

bool B1Compiler::is_global_volatile_var(const std::wstring &name) const
{
	auto g = _global_vars.find(name);
	return (g != _global_vars.end() && std::get<2>(g->second));
}

bool B1Compiler::is_global_const_var(const std::wstring &name) const
{
	auto g = _global_vars.find(name);
	return (g != _global_vars.end() && std::get<5>(g->second));
}

int B1Compiler::get_global_var_dim(const std::wstring &name) const
{
	auto g = _global_vars.find(name);
	return (g == _global_vars.end()) ? -1 : std::get<1>(g->second);
}

B1Types B1Compiler::get_global_var_type(const std::wstring &name) const
{
	auto g = _global_vars.find(name);
	return (g == _global_vars.end()) ? B1Types::B1T_UNKNOWN : std::get<0>(g->second);
}

// check global user function existence by name
bool B1Compiler::global_fn_exists(const std::wstring &name)
{
	return _global_ufns.find(name) != _global_ufns.end();
}

bool B1Compiler::add_global_ufn(const std::wstring &nm, const B1Types rt, const std::vector<B1Types> &arglist, const std::wstring &in)
{
	if(global_fn_exists(nm))
	{
		return false;
	}

	_global_ufns.emplace(std::make_pair(nm, B1_CMP_FN(nm, rt, arglist, in, false)));

	return true;
}

const B1_CMP_FN *B1Compiler::get_global_ufn(const std::wstring &name)
{
	auto ufn = _global_ufns.find(name);
	return (ufn == _global_ufns.end()) ? nullptr : &(ufn->second);
}

const B1_CMP_FN *B1Compiler::get_global_ufn(const B1_TYPED_VALUE &val)
{
	auto ufn = _global_ufns.find(val.value);
	return (ufn == _global_ufns.end() || ufn->second.args.size() != 0) ? nullptr : &(ufn->second);
}

const B1_CMP_FN * B1Compiler::get_global_ufn(const B1_CMP_ARG &arg)
{
	auto ufn = _global_ufns.find(arg[0].value);
	return (ufn == _global_ufns.end() || ufn->second.args.size() != (arg.size() - 1)) ? nullptr : &(ufn->second);
}

std::wstring B1Compiler::get_global_ufn_int_name(const std::wstring &name)
{
	const auto &ufn = _global_ufns.find(name);
	if(ufn != _global_ufns.end())
	{
		return ufn->second.iname;
	}

	return std::wstring();
}

void B1Compiler::change_global_ufn_names()
{
	std::vector<B1_CMP_FN> ufns;

	for(const auto &ufn: _global_ufns)
	{
		ufns.push_back(ufn.second);
		ufns.back().name = ufn.second.iname;
	}

	_global_ufns.clear();

	for(auto &ufn: ufns)
	{
		_global_ufns.emplace(std::make_pair(ufn.name, B1_CMP_FN(ufn.name, ufn.rettype, ufn.args, ufn.iname, false)));
	}
}

B1C_T_ERROR B1Compiler::get_global_const_var_value(const std::wstring &var_name, bool &var_found, std::wstring &value)
{
	var_found = false;
	value.clear();

	auto ci = _global_const_init.find(var_name);
	if(ci != _global_const_init.cend())
	{
		var_found = true;
		if(ci->second.second.size() != 1)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ETYPMISM);
		}
		if(B1CUtils::is_imm_val(ci->second.second[0]))
		{
			value = ci->second.second[0];
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1Compiler::get_const_var_value(const std::wstring &var_name, std::wstring &value)
{
	bool var_found = false;

	value.clear();

	if(get_global_const_var_value(var_name, var_found, value) == B1C_T_ERROR::B1C_RES_OK)
	{
		if(var_found)
		{
			return B1C_T_ERROR::B1C_RES_OK;
		}
	}
	else
	{
		return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
	}

	for(auto &fc: _file_compilers)
	{
		_curr_file_name = fc.GetFileName();

		if(fc.get_const_var_value(var_name, var_found, value) == B1C_T_ERROR::B1C_RES_OK)
		{
			if(var_found)
			{
				return B1C_T_ERROR::B1C_RES_OK;
			}
		}
		else
		{
			return static_cast<B1C_T_ERROR>(B1_RES_ESYNTAX);
		}
	}

	return static_cast<B1C_T_ERROR>(B1_RES_EUNKIDENT);
}

B1C_T_ERROR B1Compiler::eval_const_vars_values()
{
	while(true)
	{
		bool all_resolved = true;
		bool changed = false;

		for(auto &gci: _global_const_init)
		{
			for(auto &gciv: gci.second.second)
			{
				if(!B1CUtils::is_imm_val(gciv))
				{
					std::wstring value;
					auto err = get_const_var_value(gciv, value);
					if(err != B1C_T_ERROR::B1C_RES_OK)
					{
						return err;
					}

					if(!value.empty())
					{
						if(B1CUtils::is_imm_val(value))
						{
							if(!B1CUtils::is_str_val(value))
							{
								int32_t n;

								auto err1 = Utils::str2int32(value, n);
								if(err1 != B1_RES_OK)
								{
									return static_cast<B1C_T_ERROR>(err1);
								}

								Utils::correct_int_value(n, gci.second.first);
								value = std::to_wstring(n);
							}
						}
						else
						{
							all_resolved = false;
						}

						gciv = value;
						changed = true;
					}
					else
					{
						all_resolved = false;
					}
				}
			}
		}

		for(auto &fc: _file_compilers)
		{
			_curr_file_name = fc.GetFileName();

			auto err = fc.eval_const_vars_values_1_iter(changed, all_resolved);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}
		}

		if(all_resolved)
		{
			break;
		}
		if(!changed)
		{
			return static_cast<B1C_T_ERROR>(B1_RES_EUNKIDENT);
		}
	}

	return B1C_T_ERROR::B1C_RES_OK;
}

void B1Compiler::mark_var_used(const std::wstring &name, bool for_read)
{
	auto v = _used_vars.find(name);
	if(v == _used_vars.end())
	{
		_used_vars[name] = for_read ? 1 : 2;
	}
	else
	{
		v->second = v->second | (for_read ? 1 : 2);
	}
}

int B1Compiler::get_var_used(const std::wstring &name)
{
	auto v = _used_vars.find(name);
	if(v == _used_vars.end())
	{
		return 0;
	}
	else
	{
		return v->second;
	}
}

B1C_T_ERROR B1Compiler::recalc_vars_usage(bool &changed)
{
	int chksum = 0;

	changed = false;

	for(const auto &v: _used_vars)
	{
		chksum += v.second;
	}

	_used_vars.clear();

	for(auto &fc: _file_compilers)
	{
		_curr_file_name = fc.GetFileName();

		auto err = fc.calc_vars_usage();
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			b1_curr_prog_line_cnt = fc._curr_line_cnt;
			return err;
		}
	}

	int chksum1 = 0;

	for(const auto &v: _used_vars)
	{
		chksum1 += v.second;
	}

	if(chksum != chksum1)
	{
		changed = true;
	}

	return B1C_T_ERROR::B1C_RES_OK;
}


B1Compiler::B1Compiler(bool no_opt, bool out_src_lines)
: _no_opt(no_opt)
, _out_src_lines(out_src_lines)
, _opt_explicit(false)
, _opt_base1(false)
, _opt_nocheck(false)
{
}

B1Compiler::~B1Compiler()
{
}

B1C_T_ERROR B1Compiler::Load(const std::vector<std::string> &file_names)
{
	_curr_file_name.clear();

	// use current locale
	std::setlocale(LC_ALL, "");

	std::copy(file_names.begin(), file_names.end(), std::back_inserter(_file_names));

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1Compiler::Compile()
{
	_opt_explicit = false;
	_opt_base1 = false;
	_opt_nocheck = false;

	_curr_file_name.clear();

	for(int i = 0; i < _file_names.size(); i++)
	{
		_curr_file_name = _file_names[i];

		std::wstring ns = L"NS" + std::to_wstring(i + 1);
		_file_compilers.push_back(B1FileCompiler(*this, ns, _no_opt, _out_src_lines));

		B1C_T_ERROR err = _file_compilers[i].Load(_file_names[i]);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			return err;
		}

		err = _file_compilers[i].FirstRun();
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			return err;
		}

		_opt_explicit = _file_compilers[i].get_opt_explicit() ? true : _opt_explicit;
		_opt_base1 = _file_compilers[i].get_opt_base1() ? true : _opt_base1;
		_opt_nocheck = _file_compilers[i].get_opt_nocheck() ? true : _opt_nocheck;
	}

	// check options
	if(_opt_explicit)
	{
		bool emit_wrn = false;

		for(auto &fc: _file_compilers)
		{
			if(!fc.get_opt_explicit())
			{
				emit_wrn = true;
			}

			fc.set_opt_explicit();
		}

		if(emit_wrn)
		{
			//warning: explicit variables declaration is enabled for all source files
			_warnings.push_back(std::make_pair(std::string(), std::vector<std::pair<int32_t, B1C_T_WARNING>>({ std::make_pair(-1, B1C_T_WARNING::B1C_WRN_WOPTEXPLEN) })));
		}
	}

	if(_opt_base1)
	{
		bool emit_wrn = false;

		for(auto &fc: _file_compilers)
		{
			if(!fc.get_opt_base1())
			{
				emit_wrn = true;
			}

			fc.set_opt_base1();
		}

		if(emit_wrn)
		{
			//warning: option BASE1 is enabled for all source files
			_warnings.push_back(std::make_pair(std::string(), std::vector<std::pair<int32_t, B1C_T_WARNING>>({ std::make_pair(-1, B1C_T_WARNING::B1C_WRN_WOPTBASE1EN) })));
		}
	}

	if(_opt_nocheck)
	{
		bool emit_wrn = false;

		for(auto &fc: _file_compilers)
		{
			if(!fc.get_opt_nocheck())
			{
				emit_wrn = true;
			}

			fc.set_opt_nocheck();
		}

		if(emit_wrn)
		{
			//warning: option NOCHECK is enabled for all source files
			_warnings.push_back(std::make_pair(std::string(), std::vector<std::pair<int32_t, B1C_T_WARNING>>({ std::make_pair(-1, B1C_T_WARNING::B1C_WRN_WOPTNOCHKEN) })));
		}
	}

	// compile
	for(int i = 0; i < _file_names.size(); i++)
	{
		_curr_file_name = _file_names[i];

		B1C_T_ERROR err = _file_compilers[i].Load(_file_names[i]);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			return err;
		}

		err = _file_compilers[i].Compile();
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			return err;
		}
	}

	// evaluate values of const variables initialized with other const variables
	// e.g.: DIM CONST V1 = 10, CONST V2 = V1
	auto err = eval_const_vars_values();
	if(err != B1C_T_ERROR::B1C_RES_OK)
	{
		return err;
	}

	// compilation changes user-defined function names 
	// change the function names to their generated names like MAX -> __DEF_MAX
	change_global_ufn_names();

	for(auto &fc: _file_compilers)
	{
		_curr_file_name = fc.GetFileName();

		fc.change_ufn_names();

		auto err = fc.PutTypesAndOptimize();
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			// restore code line counter value (after compilation it is set to the line after the last one)
			b1_curr_prog_line_cnt = fc._curr_line_cnt;
			return err;
		}
	}

	if(!_no_opt)
	{
		bool changed = true;

		while(changed)
		{
			bool init = true;

			for(auto &fc: _file_compilers)
			{
				_curr_file_name = fc.GetFileName();

				auto err = fc.Optimize(init);
				if(err != B1C_T_ERROR::B1C_RES_OK)
				{
					b1_curr_prog_line_cnt = fc._curr_line_cnt;
					return err;
				}

				init = false;
			}

			auto err = recalc_vars_usage(changed);
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				return err;
			}
		}
	}

	for(auto &fc: _file_compilers)
	{
		_curr_file_name = fc.GetFileName();

		auto err = fc.CollectDeclStmts();
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			b1_curr_prog_line_cnt = fc._curr_line_cnt;
			return err;
		}
	}

	_curr_file_name.clear();

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1Compiler::WriteUFns(const std::string &file_name) const
{
	std::FILE *ofp = std::fopen(file_name.c_str(), "a");
	if(ofp == nullptr)
	{
		return B1C_T_ERROR::B1C_RES_EFOPEN;
	}

	for(const auto &ufn: _global_ufns)
	{
		std::fwprintf(ofp, L"DEF,%ls,%ls", ufn.second.iname.c_str(), Utils::get_type_name(ufn.second.rettype).c_str());
		for(const auto &arg: ufn.second.args)
		{
			std::fwprintf(ofp, L",%ls", Utils::get_type_name(arg.type).c_str());
		}
		std::fwprintf(ofp, L"\n");
	}

	std::fclose(ofp);

	return B1C_T_ERROR::B1C_RES_OK;
}

B1C_T_ERROR B1Compiler::Write(const std::string &file_name)
{
	_curr_file_name = file_name;

	// truncate output file
	std::FILE *ofp = std::fopen(file_name.c_str(), "w");
	if(ofp == nullptr)
	{
		return B1C_T_ERROR::B1C_RES_EFOPEN;
	}
	std::fclose(ofp);

	// write MA stmts
	for(auto &fc: _file_compilers)
	{
		auto err = fc.WriteMAs(file_name);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			return err;
		}
	}

	// write DEF commands
	auto err = WriteUFns(file_name);
	if(err != B1C_T_ERROR::B1C_RES_OK)
	{
		return err;
	}
	for(const auto &fc: _file_compilers)
	{
		err = fc.WriteUFns(file_name);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			return err;
		}
	}

	for(const auto &fc: _file_compilers)
	{
		err = fc.Write(file_name);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			return err;
		}
	}

	// write DAT stmts
	for(const auto &fc: _file_compilers)
	{
		auto err = fc.WriteDATs(file_name);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			return err;
		}
	}

	_curr_file_name.clear();

	return B1C_T_ERROR::B1C_RES_OK;
}

bool B1Compiler::GetOptExplicit() const
{
	return _opt_explicit;
}

bool B1Compiler::GetOptBase1() const
{
	return _opt_base1;
}

bool B1Compiler::GetOptNoCheck() const
{
	return _opt_nocheck;
}


static void b1c_print_version(FILE *fstr)
{
	std::fputs("BASIC1 compiler\n", fstr);
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

static void b1c_print_warnings(const std::vector<std::pair<std::string, std::vector<std::pair<int32_t, B1C_T_WARNING>>>> &wrns)
{
	for(auto &fw: wrns)
	{
		for(auto &w: fw.second)
		{
			b1c_print_warning(w.second, w.first, fw.first, true);
		}
	}
}


int main(int argc, char **argv)
{
	int i;
	int retcode = 0;
	bool print_err_desc = false;
	bool print_version = false;
	bool no_comp = false;
	bool no_asm = false;
	bool out_src_lines = false;
	std::string lib_dir;
	std::string MCU_name;
	// default target name is STM8
	std::string target_name = "STM8";
	bool list_devs = false;
	bool list_cmds = false;
	std::string dev_name;
	std::string args;
	bool args_error = false;
	std::string args_error_txt;
	std::string ofn;

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
				args = args + " -hs " + argv[i];
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
				args = args + " -l " + argv[i];
				lib_dir = argv[i];
			}

			continue;
		}

		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'L' || argv[i][1] == 'l') &&
			(argv[i][2] == 'D' || argv[i][2] == 'd') &&
			argv[i][3] == 0)
		{
			list_devs = true;
			continue;
		}

		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'L' || argv[i][1] == 'l') &&
			(argv[i][2] == 'C' || argv[i][2] == 'c') &&
			argv[i][3] == 0)
		{
			list_cmds = true;

			if(i == argc - 1)
			{
				args_error = true;
				args_error_txt = "missing device name";
			}
			else
			{
				i++;
				dev_name = Utils::str_toupper(argv[i]);
			}

			continue;
		}

		// MCU settings
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
			args = args + " -na";

			continue;
		}

		// don't call c1 compiler
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'N' || argv[i][1] == 'n') &&
			(argv[i][2] == 'C' || argv[i][2] == 'c') &&
			argv[i][3] == 0)
		{
			no_comp = true;
			continue;
		}

		// assembler option, just pass it further
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'N' || argv[i][1] == 'n') &&
			(argv[i][2] == 'C' || argv[i][2] == 'c') &&
			(argv[i][3] == 'I' || argv[i][3] == 'i') &&
			argv[i][4] == 0)
		{
			args = args + " -nci";
			continue;
		}

		// disable optimizations
		if ((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'N' || argv[i][1] == 'n') &&
			(argv[i][2] == 'O' || argv[i][2] == 'o') &&
			argv[i][3] == 0)
		{
			args = args + " -no";
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
				args = args + " -ss " + argv[i];
			}

			continue;
		}

		// target
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
				target_name = Utils::str_toupper(Utils::str_trim(argv[i]));
				if(target_name.empty())
				{
					args_error = true;
					args_error_txt = "invalid target";
				}
				args = args + " -t " + target_name;
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


	_global_settings.SetTargetName(target_name);
	_global_settings.SetMCUName(MCU_name);
	_global_settings.SetLibDir(lib_dir);

	// load target-specific stuff
	if(!select_target(global_settings))
	{
		args_error = true;
		args_error_txt = "invalid target";
	}

	if((args_error || i == argc) && !(print_version || list_devs || list_cmds))
	{
		b1c_print_version(stderr);
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
		std::fputs(" [options] filename [filename1] ... [filenameN]\n", stderr);
		std::fputs("options:\n", stderr);
		std::fputs("-d or /d - print error description\n", stderr);
		std::fputs("-hs or /hs - set heap size (in bytes), e.g. -hs 1024\n", stderr);
		std::fputs("-l or /l - libraries directory, e.g. -l \"../lib\"\n", stderr);
		std::fputs("-ld or /ld - print available devices list\n", stderr);
		std::fputs("-lc or /lc - print available device commands, e.g.: -lc UART\n", stderr);
		std::fputs("-m or /m - specify MCU name, e.g. -m STM8S103F3\n", stderr);
		std::fputs("-ml or /ml - set large memory model\n", stderr);
		std::fputs("-ms or /ms - set small memory model (default)\n", stderr);
		std::fputs("-mu or /mu - print memory usage\n", stderr);
		std::fputs("-na or /na - don't run assembler\n", stderr);
		std::fputs("-nc or /nc - compile only\n", stderr);
		std::fputs("-no or /no - disable optimizations\n", stderr);
		std::fputs("-o or /o - output file name, e.g.: -o out.b1c\n", stderr);
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
		b1c_print_version(stdout);
		return 0;
	}


	// list of source files
	std::vector<std::string> src_files;
	for(int j = i; j < argc; j++)
	{
		src_files.push_back(argv[j]);
	}

	if(MCU_name.empty())
	{
		if(list_devs || list_cmds)
		{
			std::fputs("-lc and -ld options require a MCU specified with -m option\n", stderr);
			return 2;
		}
	}
	else
	{
		// read file with MCU-specific constants, variables, etc.
		bool cfg_file_read = false;

		auto file_name = _global_settings.GetLibFileName(MCU_name, ".io");
		if(!file_name.empty())
		{
			auto err = static_cast<B1C_T_ERROR>(_global_settings.ReadIoSettings(file_name));
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				b1c_print_error(err, -1, file_name, print_err_desc);
				return 3;
			}
			cfg_file_read = true;
		}

		file_name = _global_settings.GetLibFileName(MCU_name, ".cfg");
		if(!file_name.empty())
		{
			auto err = static_cast<B1C_T_ERROR>(_global_settings.Read(file_name));
			if(err != B1C_T_ERROR::B1C_RES_OK)
			{
				b1c_print_error(err, -1, file_name, print_err_desc);
				return 4;
			}
			cfg_file_read = true;
		}

		file_name = _global_settings.GetLibFileName(MCU_name, ".bsc");
		if(!file_name.empty())
		{
			src_files.push_back(file_name);
			cfg_file_read = true;
		}

		if(!cfg_file_read)
		{
			//warning: unknown MCU name
			b1c_print_warnings(std::vector<std::pair<std::string, std::vector<std::pair<int32_t, B1C_T_WARNING>>>>({ std::make_pair(MCU_name, std::vector<std::pair<int32_t, B1C_T_WARNING>>({ std::make_pair(-1, B1C_T_WARNING::B1C_WRN_WUNKNMCU) })) }));
		}
	}


	if(list_devs)
	{
		// print available peripheral devices
		std::fputs("available devices:\n", stdout);
		const auto devs = _global_settings.GetDevList();
		for(const auto &d: devs)
		{
			const auto def_name = _global_settings.GetDefaultDeviceName(d);
			if(!def_name.empty())
			{
				std::fputs(Utils::wstr2str(def_name + L" (" + d + L")\n").c_str(), stdout);
			}
			std::fputs(Utils::wstr2str(d + L"\n").c_str(), stdout);
		}
		return 0;
	}

	if(list_cmds)
	{
		// print available device commands
		std::fputs((dev_name + " commands:\n").c_str(), stdout);
		auto d = _global_settings.GetIoDeviceName(Utils::str2wstr(dev_name));
		const auto cmds = _global_settings.GetDevCmdsList(d);
		for(const auto &c:cmds)
		{
			Settings::IoCmd cmd;
			if(!_global_settings.GetIoCmd(d, c, cmd))
			{
				continue;
			}

			if(cmd.accepts_data)
			{
				if(cmd.predef_only)
				{
					std::wstring vals;

					for(const auto &v: cmd.values)
					{
						vals += v.first + L", ";
					}
					if(!vals.empty())
					{
						vals.pop_back();
						vals.pop_back();
					}

					std::fputs(Utils::wstr2str(c + L" (" + vals + L")\n").c_str(), stdout);
				}
				else
				{
					std::fputs(Utils::wstr2str(c + L" (<" + Utils::get_type_name(cmd.data_type) + L" VALUE>)\n").c_str(), stdout);
				}
			}
			else
			{
				std::fputs(Utils::wstr2str(c + L"\n").c_str(), stdout);
			}
		}
		return 0;
	}


	B1Compiler b1c(false, out_src_lines);

	// load program files
	B1C_T_ERROR err = b1c.Load(src_files);
	if(err != B1C_T_ERROR::B1C_RES_OK)
	{
		b1c_print_warnings(b1c.GetWarnings());
		b1c_print_error(err, -1, b1c.GetCurrFileName(), print_err_desc);
		return 5;
	}

	err = b1c.Compile();
	if(err != B1C_T_ERROR::B1C_RES_OK)
	{
		b1c_print_warnings(b1c.GetWarnings());
		b1c_print_error(err, b1_curr_prog_line_cnt, b1c.GetCurrFileName(), print_err_desc);
		retcode = 6;
	}

	if(retcode == 0)
	{
		// prepare output file name
		ofn = Utils::str_trim(ofn);
		if(ofn.empty())
		{
			// no output file, use input file's directory and name but with b1c extension
			ofn = argv[i];
			auto delpos = ofn.find_last_of("\\/");
			auto pntpos = ofn.find_last_of('.');
			if(pntpos != std::string::npos && (delpos == std::string::npos || pntpos > delpos))
			{
				ofn.erase(pntpos, std::string::npos);
			}
			ofn += ".b1c";
		}
		else
		if(ofn.back() == '\\' || ofn.back() == '/')
		{
			// output directory only, use input file name but with b1c extension
			std::string tmp = argv[i];
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
			tmp += ".b1c";
			ofn += tmp;
		}

		err = b1c.Write(ofn);
		if(err != B1C_T_ERROR::B1C_RES_OK)
		{
			b1c_print_warnings(b1c.GetWarnings());
			b1c_print_error(err, b1_curr_prog_line_cnt, b1c.GetCurrFileName(), print_err_desc);
			retcode = 7;
		}

		if(b1c.GetOptExplicit())
		{
			args += " -op EXPLICIT";
		}

		if(b1c.GetOptBase1())
		{
			args += " -op BASE1";
		}

		if(b1c.GetOptNoCheck())
		{
			args += " -op NOCHECK";
		}

		b1c_print_warnings(b1c.GetWarnings());

		if(!no_comp)
		{
			std::fputs("running c1 compiler...\n", stdout);
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

			int sc = std::system((cwd + get_c1_compiler_name(_global_settings) + " -fr" + args + " " + ofn).c_str());
			if(sc == -1)
			{
				std::perror("fail");
				retcode = 8;
			}
		}
	}

	return retcode;
}

#ifndef B1_FEATURE_UNICODE_UCS2
#error Unicode support must be enabled
#endif

#ifndef B1_FEATURE_DEBUG
#error Debug functions support must be enabled
#endif

#ifndef B1_FEATURE_MINIMAL_EVALUATION
#error Minimal evaluation feature must be enabled
#endif

#ifndef B1_FEATURE_STMT_DATA_READ
#error DATA/READ/RESTORE statements support must be enabled
#endif

#ifndef B1_FEATURE_STMT_ERASE
#error ERASE statement support must be enabled
#endif

#ifndef B1_FEATURE_FUNCTIONS_USER
#error Enable user functions support
#endif
