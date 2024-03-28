/*
 BASIC1 compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 b1c.h: BASIC1 compiler classes declaration
*/


#pragma once

extern "C"
{
#include "b1.h"
#include "b1ex.h"
}

#include "errors.h"
#include "b1cmp.h"


class B1Compiler;

class B1FileCompiler: B1_CMP_CMDS
{
	friend class B1Compiler;

private:
	enum class B1_CMP_EXP_TYPE
	{
		B1_CMP_ET_UNKNOWN,
		B1_CMP_ET_IMM_VAL,
		B1_CMP_ET_LOCAL,
		B1_CMP_ET_LOGICAL,
		B1_CMP_ET_VAR,
	};

	enum class B1_CMP_VAL_TYPE
	{
		B1_CMP_VT_IMM_VAL,
		B1_CMP_VT_FNVAR,
		B1_CMP_VT_FN_ARG,
		B1_CMP_VT_LOCAL,
	};

	enum class B1_CMP_STATE
	{
		B1_CMP_STATE_OK,
		B1_CMP_STATE_IF,
		B1_CMP_STATE_ELSEIF,
		B1_CMP_STATE_ELSE,
		B1_CMP_STATE_FOR,
		B1_CMP_STATE_WHILE,
	};

	std::vector<std::pair<B1_CMP_STATE, std::vector<std::wstring>>> _state_stack;

	std::pair<B1_CMP_STATE, std::vector<std::wstring>> _state;
	std::map<int32_t, std::wstring> _src_lines;

	//       gen. name                type     dim  volatile mem   static
	std::map<std::wstring, std::tuple<B1Types, int, bool,    bool, bool>> _vars;

	//       user name     gen. name
	std::map<std::wstring, std::wstring> _var_names;

	std::map<std::wstring, B1_CMP_FN> _ufns;

	B1_CMP_CMDS _MA_stmts;
	B1_CMP_CMDS _DAT_stmts;

	// labels that should not be removed (used indirectly)
	std::set<std::wstring> _req_labels;


	B1C_T_ERROR put_var_name(const std::wstring &name, const B1Types type, int dims, bool global, bool volat, bool mem_var, bool stat);
	std::wstring get_var_name(const std::wstring &name, bool &expl) const;
	bool is_mem_var_name(const std::wstring &name) const;
	bool is_volatile_var(const std::wstring &name) const;
	int get_var_dim(const std::wstring &name) const;
	B1Types get_var_type(const std::wstring &name) const;

	bool fn_exists(const std::wstring &name);
	bool add_ufn(bool global, const std::wstring &nm, const B1Types rt, const std::vector<B1Types> &arglist);
	const B1_CMP_FN *get_fn(const std::wstring &name);
	const B1_CMP_FN *get_fn(const B1_TYPED_VALUE &val);
	const B1_CMP_FN *get_fn(const B1_CMP_ARG &arg);
	std::wstring get_fn_int_name(const std::wstring &name);
	void change_ufn_names();

	bool correct_rpn(B1_CMP_EXP_TYPE &res_type, B1_CMP_ARG &res, bool get_ref);
	B1_T_ERROR process_expression(B1_CMP_EXP_TYPE &res_type, B1_CMP_ARG &res, bool get_ref = false);
	
	B1_T_ERROR eval_chr(const std::wstring &num_val, const B1Types type, std::wstring &res_str);
	B1_T_ERROR concat_strings_rpn(std::wstring &res);

	B1_T_ERROR st_option_set(const B1_T_CHAR *s, uint8_t value_type, bool onoff, int *value);
	B1_T_ERROR st_option_set_expr(const B1_T_CHAR *s, B1_CMP_EXP_TYPE &exp_type, B1_CMP_ARG &res);
	bool st_option_check(bool first_run, bool& opt, bool& opt_def, bool val);
	B1C_T_ERROR st_option(bool first_run);
	B1C_T_ERROR st_ioctl_get_symbolic_value(std::wstring &value);
	B1C_T_ERROR st_ioctl();
	B1_T_ERROR st_let(const B1_T_CHAR **stop_tokens, B1_CMP_ARG *var_ref = nullptr);
	B1_T_ERROR st_goto();
	B1_T_ERROR st_gosub();
	B1_T_ERROR st_dim_get_size(bool allow_TO_stop_word, std::pair<B1_CMP_ARG, B1_CMP_EXP_TYPE> &res, bool first_run);
	B1C_T_ERROR st_dim(bool first_run);
	B1_T_ERROR st_erase();
	B1_T_ERROR st_get_type_def(bool allow_addr, B1_TOKENDATA &td, B1_T_INDEX &len, B1Types &type, bool *addr_present = nullptr, std::wstring *address = nullptr);
	B1_T_ERROR st_def(bool first_run);

	B1C_T_ERROR compile_simple_stmt(uint8_t stmt);

	B1C_T_ERROR st_if();
	B1_T_ERROR st_if_end();
	B1_T_ERROR st_for();
	B1_T_ERROR st_next();
	B1_T_ERROR st_data();
	B1_T_ERROR st_read();
	B1_T_ERROR st_restore();
	B1_T_ERROR st_while();
	B1_T_ERROR st_wend();
	B1_T_ERROR st_continue();
	B1_T_ERROR st_break();
	B1_T_ERROR st_print();
	B1_T_ERROR st_input();
	B1C_T_ERROR st_put_get_trr(const std::wstring &cmd_name, bool is_input);

	B1_CMP_CMDS::const_iterator find_LF(B1_CMP_CMDS::const_iterator lacmd, B1_CMP_CMDS::const_iterator intlfcmd, bool &intlf_found);
	void fix_LA_LF_order();
	B1C_T_ERROR remove_unused_labels(bool &changed);
	B1C_T_ERROR remove_duplicate_labels(bool &changed);
	bool is_udef_used(const B1_TYPED_VALUE &val);
	bool is_udef_used(const B1_CMP_ARG &arg);
	bool is_udef_used(const B1_CMP_CMD &cmd);
	bool is_volatile_used(const B1_CMP_ARG &arg);
	bool is_volatile_used(const B1_CMP_CMD &cmd);
	B1C_T_ERROR remove_duplicate_assigns(bool &changed);
	B1C_T_ERROR remove_self_assigns(bool &changed);
	B1C_T_ERROR remove_jumps(bool &changed);
	void remove_compare_op(B1_CMP_CMDS::iterator i, bool is_true);
	B1C_T_ERROR remove_redundant_comparisons(bool &changed);
	B1C_T_ERROR replace_unary_minus(bool &changed);
	B1C_T_ERROR eval_unary_ops(bool &changed);
	void set_to_init_value_arg(B1_CMP_ARG &arg, bool is_dst, const std::map<std::wstring, std::pair<bool, std::wstring>> &vars, bool init, bool &changed);
	std::wstring set_to_init_value(B1_CMP_CMD &cmd, const std::map<std::wstring, std::pair<bool, std::wstring>> &vars, bool init, bool &changed);
	B1C_T_ERROR reuse_imm_values(bool init, bool &changed);
	B1C_T_ERROR remove_locals(bool &changed);
	B1_T_ERROR get_type(B1_TYPED_VALUE &v, bool read, std::map<std::wstring, std::vector<std::pair<B1Types &, B1Types>>> &iif_locals);
	B1_T_ERROR get_type(B1_CMP_ARG &a, bool read, std::map<std::wstring, std::vector<std::pair<B1Types &, B1Types>>> &iif_locals);
	B1C_T_ERROR put_types();
	B1C_T_ERROR put_fn_def_values(B1_CMP_ARG &arg);
	B1C_T_ERROR put_fn_def_values();
	B1C_T_ERROR inline_fns(bool &changed);
	bool get_LA_LF(iterator s, iterator e, iterator &la, iterator &lf);
	B1C_T_ERROR reuse_locals(bool &changed);
	B1C_T_ERROR reuse_vars(bool &changed);
	void correct_int_value(int32_t &n, const B1Types type);
	bool eval_imm_fn_arg(B1_CMP_ARG &a);
	B1C_T_ERROR eval_imm_exps(bool &changed);
	B1C_T_ERROR check_MA_stmts();
	B1C_T_ERROR remove_DAT_stmts();
	B1C_T_ERROR remove_unused_vars(B1_CMP_ARG &a, bool &changed);
	B1C_T_ERROR remove_unused_vars(bool &changed);
	B1C_T_ERROR calc_vars_usage(B1_TYPED_VALUE &v, bool read);
	B1C_T_ERROR calc_vars_usage(B1_CMP_ARG &a, bool read);
	B1C_T_ERROR optimize_GA_GF(bool &changed);
	bool get_opt_explicit() const;
	B1C_T_ERROR set_opt_explicit();
	bool get_opt_base1() const;
	B1C_T_ERROR set_opt_base1();
	bool get_opt_nocheck() const;
	B1C_T_ERROR set_opt_nocheck();
	std::wstring replace_type_spec(const std::wstring &token) const;

protected:
	bool _no_opt;
	bool _out_src_lines;
	B1Compiler &_compiler;

	bool _opt_explicit_def;
	bool _opt_explicit;

	bool _opt_base1_def;
	bool _opt_base1;

	bool _opt_nocheck_def;
	bool _opt_nocheck;

	std::string _file_name;
	std::string _int_name;

	std::map<int32_t, std::vector<B1C_T_WARNING>> _warnings;


	B1C_T_ERROR calc_vars_usage();


public:
	B1FileCompiler() = delete;
	B1FileCompiler(B1Compiler &compiler, const std::wstring &name_space, bool no_opt, bool out_src_lines);

	~B1FileCompiler();

	B1C_T_ERROR Load(const std::string &file_name);
	B1C_T_ERROR FirstRun();
	B1C_T_ERROR Compile();
	B1C_T_ERROR PutTypesAndOptimize();
	B1C_T_ERROR Optimize(bool init);
	B1C_T_ERROR CollectDeclStmts();
	B1C_T_ERROR WriteUFns(const std::string &file_name) const;
	B1C_T_ERROR WriteStmt(const B1_CMP_CMD &cmd, std::FILE *ofp, int &curr_line_id) const;
	B1C_T_ERROR WriteMAs(const std::string &file_name) const;
	B1C_T_ERROR WriteDATs(const std::string &file_name) const;
	B1C_T_ERROR Write(const std::string &file_name) const;

	std::string GetFileName() const
	{
		return _file_name;
	}

	const std::map<int32_t, std::vector<B1C_T_WARNING>> &GetWarnings() const
	{
		return _warnings;
	}
};

class B1Compiler
{
	friend class B1FileCompiler;

private:
	//       gen. name                type     dim  volatile mem   stat
	std::map<std::wstring, std::tuple<B1Types, int, bool,    bool, bool>> _global_vars;

	//       user name     gen. name
	std::map<std::wstring, std::wstring> _global_var_names;

	std::map<std::wstring, B1_CMP_FN> _global_ufns;

	//                     1 - reading, 2 - writing, 3 - reading + writing
	std::map<std::wstring, int> _used_vars;

	std::vector<std::pair<std::string, std::vector<std::pair<int32_t, B1C_T_WARNING>>>> _warnings;


protected:
	bool _no_opt;
	bool _out_src_lines;
	std::vector<B1FileCompiler> _file_compilers;

	std::vector<std::string> _file_names;

	bool _opt_explicit;
	bool _opt_base1;
	bool _opt_nocheck;

	mutable std::string _curr_file_name;


	B1C_T_ERROR put_global_var_name(const std::wstring &name, const B1Types type, int dims, bool volat, bool mem_var, bool stat);
	bool global_var_check(bool global, bool mem_var, bool stat, const std::wstring &name) const;
	std::wstring get_global_var_name(const std::wstring &name) const;
	bool is_global_mem_var_name(const std::wstring &name) const;
	bool is_global_volatile_var(const std::wstring &name) const;
	int get_global_var_dim(const std::wstring &name) const;
	B1Types get_global_var_type(const std::wstring &name) const;

	bool global_fn_exists(const std::wstring &name);
	bool add_global_ufn(const std::wstring &nm, const B1Types rt, const std::vector<B1Types> &arglist, const std::wstring &in);
	const B1_CMP_FN *get_global_ufn(const std::wstring &name);
	const B1_CMP_FN *get_global_ufn(const B1_TYPED_VALUE &val);
	const B1_CMP_FN *get_global_ufn(const B1_CMP_ARG &arg);
	std::wstring get_global_ufn_int_name(const std::wstring &name);
	void change_global_ufn_names();

	void mark_var_used(const std::wstring &name, bool for_read);
	bool is_var_used(const std::wstring &name);
	int get_var_used(const std::wstring &name);

	B1C_T_ERROR recalc_vars_usage(bool &changed);


public:
	B1Compiler() = delete;
	B1Compiler(bool no_opt, bool out_src_lines);

	~B1Compiler();

	B1C_T_ERROR Load(const std::vector<std::string> &file_names);
	B1C_T_ERROR Compile();
	B1C_T_ERROR WriteUFns(const std::string &file_name) const;
	B1C_T_ERROR Write(const std::string &file_name) const;

	bool GetOptExplicit() const;
	bool GetOptBase1() const;
	bool GetOptNoCheck() const;

	std::string GetCurrFileName() const
	{
		return _curr_file_name;
	}

	const std::vector<std::pair<std::string, std::vector<std::pair<int32_t, B1C_T_WARNING>>>> &GetWarnings()
	{
		for(const auto &fc: _file_compilers)
		{
			auto &ws = fc.GetWarnings();
			std::pair<std::string, std::vector<std::pair<int32_t, B1C_T_WARNING>>> ws1;

			for(auto &fw: ws)
			{
				for(auto w: fw.second)
				{
					ws1.second.push_back(std::make_pair(fw.first, w));
				}
			}
			ws1.first = fc.GetFileName();
			_warnings.push_back(ws1);
		}

		return _warnings;
	}
};