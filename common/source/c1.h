/*
 Intermediate code compiler
 Copyright (c) 2021-2025 Nikolay Pletnev
 MIT license

 c1.h: Intermediate code compiler classes declaration
*/


#pragma once

extern "C"
{
#include "b1.h"
}

#include "c1errors.h"
#include "b1cmp.h"

#include <memory>


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


	B1_ASM_OP() = delete;

	B1_ASM_OP(AOT type, const std::wstring &data, const std::wstring &comment, bool is_volatile, bool is_inline)
	: _type(type)
	, _data(data)
	, _comment(comment)
	, _volatile(is_volatile)
	, _is_inline(is_inline)
	{
	}

	~B1_ASM_OP()
	{
	}
};


class B1_ASM_OPS: public std::list<std::unique_ptr<B1_ASM_OP>>
{
};


class C1Compiler: public B1_CMP_CMDS
{
protected:
	enum class VST
	{
		VST_UNKNOWN,
		VST_SIMPLE, // scalar variable
		VST_ARRAY,
		VST_STAT_ARRAY,
		VST_CONST_ARRAY,
	};


	bool _out_src_lines;
	bool _opt_nocheck;

	//       namespace     dat cmd iterators 
	std::map<std::wstring, std::vector<iterator>> _data_stmts;
	// list of namespaces that contain DAT stmts (to initialize data pointers)
	std::set<std::wstring> _data_stmts_init;
	
	std::map<std::wstring, B1_CMP_VAR> _locals;
	
	std::map<std::wstring, B1_CMP_VAR> _vars;
	std::map<std::wstring, B1_CMP_VAR> _mem_areas;
	// vars usage statistics
	//       name                     storage type  data type  usage count
	std::map<std::wstring, std::tuple<VST,          B1Types,   int32_t>> _vars_stats;
	
	std::vector<std::wstring> _vars_order;
	std::set<std::wstring> _vars_order_set;

	//       data                     label       written file_id  line cnt
	std::map<std::wstring, std::tuple<std::wstring, bool, int32_t, int32_t>> _str_labels;
	std::map<std::wstring, std::wstring> _dat_rst_labels;
	std::map<std::wstring, B1_CMP_FN> _ufns;

	std::set<std::wstring> _sub_entry_labels;

	int32_t _data_size;
	int32_t _const_size;

	// call statement
	std::wstring _call_stmt;
	// return statement
	std::wstring _ret_stmt;

	std::map<int32_t, std::wstring> _src_lines;

	bool _inline_asm;
	iterator _asm_stmt_it;

	std::set<std::string> _inline_code;

	std::wstring _last_dat_namespace;

	// resolved symbols
	std::set<std::wstring> _all_symbols;
	// symbols to resolve
	std::set<std::wstring> _req_symbols;

	// init. files list
	std::vector<std::wstring> _init_files;

	int32_t _next_temp_namespace_id;

	std::vector<std::string> _src_file_names;
	std::map<std::string, int32_t> _src_file_name_ids;

	std::wstring _comment;

	B1_ASM_OPS _data_sec;
	// use list (not vector) because it does not require default copy constructor existence
	std::list<B1_ASM_OPS> _const_secs;
	B1_ASM_OPS _code_init_sec;
	std::list<B1_ASM_OPS> _code_secs;

	B1_ASM_OPS *_curr_code_sec;
	B1_ASM_OPS *_curr_const_sec;

	std::vector<std::tuple<int32_t, std::string, C1_T_WARNING>> _warnings;

	// variables for optimizer log helper functions
	//     rule id      use counter
	mutable std::map<int, std::tuple<int>> _opt_rules_usage_data;
	mutable std::map<std::wstring, B1_ASM_OPS::const_iterator> _opt_labels;


	C1_T_ERROR find_first_of(const std::wstring &str, const std::wstring &delimiters, size_t &off) const;
	std::wstring get_next_value(const std::wstring &str, const std::wstring &delimiters, size_t &next_off) const;
	bool check_label_name(const std::wstring &name) const;
	bool check_stdfn_name(const std::wstring &name) const;
	bool check_cmd_name(const std::wstring &name) const;
	bool check_type_name(const std::wstring &name) const;
	bool check_namespace_name(const std::wstring &name) const;
	bool check_address(const std::wstring &address) const;
	bool check_num_val(const std::wstring &numval) const;
	bool check_str_val(const std::wstring &strval) const;
	C1_T_ERROR get_cmd_name(const std::wstring &str, std::wstring &name, size_t &next_off) const;
	C1_T_ERROR get_simple_arg(const std::wstring &str, B1_TYPED_VALUE &arg, size_t &next_off) const;
	std::wstring gen_next_tmp_namespace();
	std::wstring add_namespace(const std::wstring &name) const;
	C1_T_ERROR get_arg(const std::wstring &str, B1_CMP_ARG &arg, size_t &next_off) const;
	virtual C1_T_ERROR process_asm_cmd(const std::wstring &line) = 0;
	C1_T_ERROR replace_inline(std::wstring &line, const std::map<std::wstring, std::wstring> &inl_params, bool &empty_val) const;
	C1_T_ERROR load_inline(size_t offset, const std::wstring &line, iterator load_at, const std::map<std::wstring, std::wstring> &inl_params = std::map<std::wstring, std::wstring>(), const B1_CMP_CMD *orig_cmd = nullptr);
	C1_T_ERROR load_next_command(const std::wstring &line, const_iterator pos);

	const B1_CMP_FN *get_fn(const B1_TYPED_VALUE &val) const;
	const B1_CMP_FN *get_fn(const B1_CMP_ARG &arg) const;
	void update_vars_stats(const std::wstring &name, VST storage_type, B1Types data_type);
	C1_T_ERROR check_arg(B1_CMP_ARG &arg);
	C1_T_ERROR read_ufns(const_iterator begin, const_iterator end);
	C1_T_ERROR read_and_check_locals(const_iterator begin, const_iterator end);
	C1_T_ERROR read_and_check_vars(iterator begin, iterator end, bool inline_code);
	C1_T_ERROR process_imm_str_value(const B1_CMP_ARG &arg);
	C1_T_ERROR process_imm_str_values(const_iterator begin, const_iterator end);

	virtual B1_ASM_OPS::iterator create_asm_op(B1_ASM_OPS &sec, B1_ASM_OPS::const_iterator where, AOT type, const std::wstring &lbl, bool is_volatile, bool is_inline);

	virtual std::wstring ROM_string_representation(int32_t str_len, const std::wstring &str) const;

	B1_ASM_OPS::iterator add_lbl(B1_ASM_OPS &sec, B1_ASM_OPS::const_iterator where, const std::wstring &lbl, bool is_volatile, bool is_inline = false);
	B1_ASM_OPS::iterator add_data(B1_ASM_OPS &sec, B1_ASM_OPS::const_iterator where, const std::wstring &data, bool is_volatile, bool is_inline = false);
	B1_ASM_OPS::iterator add_op(B1_ASM_OPS &sec, B1_ASM_OPS::const_iterator where, const std::wstring &op, bool is_volatile, bool is_inline = false);
	B1_ASM_OPS::iterator add_op(B1_ASM_OPS &sec, const std::wstring &op, bool is_volatile, bool is_inline = false);

	virtual C1_T_ERROR add_data_def(const std::wstring &name, const std::wstring &asmtype, int32_t rep, bool is_volatile);
	virtual C1_T_ERROR write_data_sec(bool code_init);
	virtual C1_T_ERROR write_const_sec();
	virtual C1_T_ERROR write_code_sec(bool code_init) = 0;

	// optimizer log helper function
	// init = false creates new record and sets its usage count to 0 (if the record does not exist)
	void update_opt_rule_usage_stat(int32_t rule_id, bool init = false) const;

	C1_T_ERROR save_section(const std::wstring &sec_name, const B1_ASM_OPS &sec, std::FILE *fp);

public:
	C1Compiler() = delete;
	C1Compiler(bool out_src_lines, bool opt_nocheck);
	virtual ~C1Compiler();

	// optimizer log helper functions
	C1_T_ERROR ReadOptLogFile(const std::string &file_name) const;
	C1_T_ERROR WriteOptLogFile(const std::string &file_name) const;

	// loads files with b1c instructions
	C1_T_ERROR Load(const std::vector<std::string> &file_names);
	C1_T_ERROR Compile();
	// code_sec_index = -1: write _code_init_sec
	// code_sec_index >= 0: write _code_init_sec[code_sec_index]
	C1_T_ERROR WriteCode(bool code_init, int32_t code_sec_index);
	virtual C1_T_ERROR Save(const std::string &file_name, bool overwrite_existing = true);

	C1_T_ERROR GetUndefinedSymbols(std::set<std::wstring> &symbols) const;
	C1_T_ERROR GetResolvedSymbols(std::set<std::wstring> &symbols) const;
	C1_T_ERROR GetInitFiles(std::vector<std::wstring> &init_files) const;

	int GetCurrLineNum() const
	{
		return _curr_line_cnt;
	}

	std::string GetCurrFileName() const
	{
		return _src_file_names[_curr_src_file_id];
	}

	const std::vector<std::tuple<int32_t, std::string, C1_T_WARNING>> &GetWarnings() const
	{
		return _warnings;
	}
};
