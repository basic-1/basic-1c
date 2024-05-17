/*
 STM8 intermediate code compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 c1stm8.h: STM8 intermediate code compiler classes declaration
*/


#pragma once

extern "C"
{
#include "b1.h"
}

#include "errors.h"
#include <b1cmp.h>


// loading value types
enum class LVT
{
	LVT_NONE = 0,
	LVT_REG = 1,		// value is loaded into register (A, X, or X+Y pair)
	LVT_IMMVAL = 2,		// immediate value (for numeric types only)
	LVT_MEMREF = 4,		// memory address (e.g. __VAR_A, __STR_S$, __VAR_B + 0x10)
	LVT_STKREF = 8,		// value in stack (local or function argument, returns offset relative to SP)
};

inline LVT operator |(LVT lhs, LVT rhs)
{
	using T = std::underlying_type_t<LVT>;
	return static_cast<LVT>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

inline LVT &operator |=(LVT &lhs, LVT rhs)
{
	lhs = lhs | rhs;
	return lhs;
}

inline bool operator &(LVT lhs, LVT rhs)
{
	using T = std::underlying_type_t<LVT>;
	auto bit_and = static_cast<T>(lhs) & static_cast<T>(rhs);
	return (bit_and != 0);
}


// assembler op type
enum class AOT
{
	AOT_LABEL,
	AOT_OP,
	AOT_DATA,
};


class B1_ASM_OP
{
public:
	AOT _type;
	std::wstring _data;
	std::wstring _comment;
	bool _volatile;
	bool _is_inline;

	mutable bool _parsed;
	mutable std::wstring _op;
	mutable std::vector<std::wstring> _args;

	B1_ASM_OP(AOT type, const std::wstring &data, const std::wstring &comment, bool is_volatile, bool is_inline)
	: _type(type)
	, _data(data)
	, _comment(comment)
	, _volatile(is_volatile)
	, _parsed(false)
	, _is_inline(is_inline)
	{
	}

	bool Parse() const;
};


class B1_ASM_OPS: public std::list<B1_ASM_OP>
{
private:
	std::wstring _comment;

public:
	void add_op(const std::wstring &op, bool is_volatile, bool is_inline = false)
	{
		push_back(B1_ASM_OP(AOT::AOT_OP, op, _comment, is_volatile, is_inline));
		_comment.clear();
	}

	void add_lbl(const std::wstring &lbl, bool is_volatile, bool is_inline = false)
	{
		push_back(B1_ASM_OP(AOT::AOT_LABEL, lbl, _comment, is_volatile, is_inline));
		_comment.clear();
	}

	void add_data(const std::wstring &data, bool is_volatile, bool is_inline = false)
	{
		push_back(B1_ASM_OP(AOT::AOT_DATA, data, _comment, is_volatile, is_inline));
		_comment.clear();
	}

	void add_comment(const std::wstring &comment)
	{
		_comment = comment;
	}
};


class C1STM8Compiler: B1_CMP_CMDS
{
private:
	bool _out_src_lines;
	bool _opt_nocheck;

	//       namespace     dat cmd iterators 
	std::map<std::wstring, std::vector<iterator>> _data_stmts;
	// list of namespaces that contain DAT stmts (to initialize data pointers)
	std::set<std::wstring> _data_stmts_init;
	
	std::map<std::wstring, B1_CMP_VAR> _locals;
	
	std::map<std::wstring, B1_CMP_VAR> _vars;
	std::map<std::wstring, B1_CMP_VAR> _mem_areas;
	
	std::vector<std::wstring> _vars_order;
	std::set<std::wstring> _vars_order_set;

	//       data                     label       written file_id  line cnt
	std::map<std::wstring, std::tuple<std::wstring, bool, int32_t, int32_t>> _str_labels;
	std::map<std::wstring, std::wstring> _dat_rst_labels;
	std::map<std::wstring, B1_CMP_FN> _ufns;

	int32_t _data_size;
	int32_t _const_size;

	int32_t _stack_ptr;
	std::vector<std::pair<B1_TYPED_VALUE, int32_t>> _local_offset;

	// total size of all arguments of the current UDEF
	int32_t _curr_udef_args_size;
	// offsets of the current user-defined function's arguments passed in stack (e.g. {5, 3, 1})
	std::vector<int32_t> _curr_udef_arg_offsets;
	// offsets of the current UDEF's string arguments
	std::vector<int32_t> _curr_udef_str_arg_offsets;

	// simple call statement
	std::wstring _call_stmt;
	// return statement
	std::wstring _ret_stmt;

	std::map<int32_t, std::wstring> _src_lines;

	// _cmp_active is set to true by a comparison operator, LA, LF, JT and JF operators do not change the variable, other operators set it to false
	bool _cmp_active;
	std::wstring _cmp_op;
	B1Types _cmp_type;

	bool _retval_active;
	B1Types _retval_type;

	// just created local string variables (no need to call __LIB_STR_RLS when assigning a value)
	std::set<std::wstring> _clear_locals;

	std::set<std::wstring> _allocated_arrays;

	bool _inline_asm;
	iterator _asm_stmt_it;

	std::set<std::string> _inline_code;

	// resolved symbols
	std::set<std::wstring> _all_symbols;
	// symbols to resolve
	std::set<std::wstring> _req_symbols;

	// init. files list
	std::vector<std::wstring> _init_files;

	int32_t _next_temp_namespace_id;

	std::vector<std::string> _src_file_names;
	std::map<std::string, int32_t> _src_file_name_ids;

	bool _page0;
	B1_ASM_OPS _page0_sec;
	B1_ASM_OPS _data_sec;
	B1_ASM_OPS _const_sec;
	B1_ASM_OPS _code_init_sec;
	B1_ASM_OPS _code_sec;

	B1_ASM_OPS *_curr_code_sec;

	std::map<int, std::wstring> _irq_handlers;

	std::vector<std::pair<const_iterator, std::map<std::wstring, std::wstring>>> _end_placement;

	//                   iterator        store arg   file id  line cnt
	std::list<std::tuple<const_iterator, B1_CMP_ARG, int32_t, int32_t>> _store_at;

	std::vector<std::tuple<int32_t, std::string, C1STM8_T_WARNING>> _warnings;

	//     rule id      use counter
	std::map<int, std::tuple<int>> _opt_rules_usage_data;
	std::map<std::wstring, B1_ASM_OPS::const_iterator> _opt_labels;

	C1STM8_T_ERROR find_first_of(const std::wstring &str, const std::wstring &delimiters, size_t &off) const;
	std::wstring get_next_value(const std::wstring &str, const std::wstring &delimiters, size_t &next_off) const;
	bool check_label_name(const std::wstring &name) const;
	bool check_stdfn_name(const std::wstring &name) const;
	bool check_cmd_name(const std::wstring &name) const;
	bool check_type_name(const std::wstring &name) const;
	bool check_namespace_name(const std::wstring &name) const;
	bool check_address(const std::wstring &address) const;
	bool check_num_val(const std::wstring &numval) const;
	bool check_str_val(const std::wstring &strval) const;
	C1STM8_T_ERROR get_cmd_name(const std::wstring &str, std::wstring &name, size_t &next_off) const;
	C1STM8_T_ERROR get_simple_arg(const std::wstring &str, B1_TYPED_VALUE &arg, size_t &next_off) const;
	std::wstring gen_next_tmp_namespace();
	std::wstring add_namespace(const std::wstring &name) const;
	C1STM8_T_ERROR get_arg(const std::wstring &str, B1_CMP_ARG &arg, size_t &next_off) const;
	C1STM8_T_ERROR process_asm_cmd(const std::wstring &line);
	C1STM8_T_ERROR replace_inline(std::wstring &line, const std::map<std::wstring, std::wstring> &inl_params);
	C1STM8_T_ERROR load_inline(size_t offset, const std::wstring &line, const_iterator pos, const std::map<std::wstring, std::wstring> &inl_params = std::map<std::wstring, std::wstring>());
	C1STM8_T_ERROR load_next_command(const std::wstring &line, const_iterator pos);

	const B1_CMP_FN *get_fn(const B1_TYPED_VALUE &val) const;
	const B1_CMP_FN *get_fn(const B1_CMP_ARG &arg) const;
	C1STM8_T_ERROR check_arg(B1_CMP_ARG &arg);
	C1STM8_T_ERROR read_ufns();
	C1STM8_T_ERROR read_and_check_locals();
	C1STM8_T_ERROR read_and_check_vars();
	C1STM8_T_ERROR process_imm_str_value(const B1_CMP_ARG &arg);
	C1STM8_T_ERROR process_imm_str_values();

	C1STM8_T_ERROR write_data_sec();
	C1STM8_T_ERROR write_const_sec();

	C1STM8_T_ERROR calc_array_size(const B1_CMP_VAR &var, int32_t size1);
	C1STM8_T_ERROR stm8_st_gf(const B1_CMP_VAR &var, bool is_ma);
	C1STM8_T_ERROR stm8_arrange_types(const B1Types type_from, const B1Types type_to);
	C1STM8_T_ERROR stm8_load_from_stack(int32_t offset, const B1Types init_type, const B1Types req_type, LVT req_valtype, LVT &rvt, std::wstring &rv);
	C1STM8_T_ERROR stm8_load(const B1_TYPED_VALUE &tv, const B1Types req_type, LVT req_valtype, LVT *res_valtype = nullptr, std::wstring *res_val = nullptr, bool *volatile_var = nullptr);
	C1STM8_T_ERROR stm8_arr_alloc_def(const B1_CMP_VAR &var);
	C1STM8_T_ERROR stm8_arr_offset(const B1_CMP_ARG &arg, bool &imm_offset, int32_t &offset);
	C1STM8_T_ERROR stm8_load(const B1_CMP_ARG &arg, const B1Types req_type, LVT req_valtype, LVT *res_valtype = nullptr, std::wstring *res_val = nullptr, bool *volatile_var = nullptr);
	C1STM8_T_ERROR stm8_init_array(const B1_CMP_CMD &cmd, const B1_CMP_VAR &var);
	C1STM8_T_ERROR stm8_st_ga(const B1_CMP_CMD &cmd, const B1_CMP_VAR &var);
	C1STM8_T_ERROR stm8_store(const B1_TYPED_VALUE &tv);
	C1STM8_T_ERROR stm8_store(const B1_CMP_ARG &arg);
	C1STM8_T_ERROR stm8_assign(const B1_CMP_CMD &cmd, bool omit_zero_init);
	C1STM8_T_ERROR stm8_un_op(const B1_CMP_CMD &cmd, bool omit_zero_init);
	C1STM8_T_ERROR stm8_add_op(const B1_CMP_CMD &cmd);
	C1STM8_T_ERROR stm8_mul_op(const B1_CMP_CMD &cmd);
	C1STM8_T_ERROR stm8_bit_op(const B1_CMP_CMD &cmd);
	C1STM8_T_ERROR stm8_add_shift_op(const std::wstring &shift_cmd, const B1Types type);
	C1STM8_T_ERROR stm8_shift_op(const B1_CMP_CMD &cmd);
	C1STM8_T_ERROR stm8_num_cmp_op(const B1_CMP_CMD &cmd);
	C1STM8_T_ERROR stm8_str_cmp_op(const B1_CMP_CMD &cmd);
	bool is_udef_or_var_used(const B1_CMP_ARG &arg, bool dst, std::set<std::wstring> &vars_to_free);
	bool is_udef_or_var_used(const B1_CMP_CMD &cmd, std::set<std::wstring> &vars_to_free);
	C1STM8_T_ERROR write_ioctl(std::list<B1_CMP_CMD>::const_iterator &cmd_it);
	C1STM8_T_ERROR write_code_sec(bool code_init);
	std::wstring correct_SP_offset(const std::wstring &arg, int32_t op_size, bool &no_SP_off, int32_t *offset = nullptr);
	bool is_arithm_op(const std::wstring &op, int32_t &size);
	// init = false creates new record and sets its usage count to 0 (if the record does not exist)
	void update_opt_rule_usage_stat(int32_t rule_id, bool init = false);
	C1STM8_T_ERROR stm8_load_ptr(const B1_CMP_ARG &first, const B1_CMP_ARG &count);
	bool is_reg_used_after(B1_ASM_OPS::const_iterator i, const std::wstring &reg_name, bool branch = false) const;


public:
	C1STM8Compiler() = delete;
	C1STM8Compiler(bool out_src_lines, bool opt_nocheck);
	~C1STM8Compiler();

	// loads files with b1c instructions
	C1STM8_T_ERROR Load(const std::vector<std::string> &file_names);
	C1STM8_T_ERROR Compile();
	C1STM8_T_ERROR WriteCode(bool code_init);
	C1STM8_T_ERROR WriteCodeInitBegin();
	C1STM8_T_ERROR WriteCodeInitDAT();
	C1STM8_T_ERROR WriteCodeInitEnd();
	C1STM8_T_ERROR ReadOptLogFile(const std::string &file_name);
	C1STM8_T_ERROR WriteOptLogFile(const std::string &file_name);
	C1STM8_T_ERROR Optimize1(bool &changed);
	C1STM8_T_ERROR Optimize2(bool &changed);
	C1STM8_T_ERROR Optimize3(bool &changed);
	C1STM8_T_ERROR Save(const std::string &file_name);

	C1STM8_T_ERROR GetUndefinedSymbols(std::set<std::wstring> &symbols);
	C1STM8_T_ERROR GetResolvedSymbols(std::set<std::wstring> &symbols);
	C1STM8_T_ERROR GetInitFiles(std::vector<std::wstring> &init_files);

	int GetCurrLineNum() const
	{
		return _curr_line_cnt;
	}

	std::string GetCurrFileName() const
	{
		return _src_file_names[_curr_src_file_id];
	}

	const std::vector<std::tuple<int32_t, std::string, C1STM8_T_WARNING>> &GetWarnings() const
	{
		return _warnings;
	}
};
