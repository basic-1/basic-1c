/*
 BASIC1 compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 b1cmp.h: BASIC1 compiler helper classes declaration
*/


#pragma once

#include <string>
#include <vector>
#include <list>
#include <map>
#include <iterator>

#include "Utils.h"


class B1_CMP_CMD;
class B1_CMP_ARG;
class B1_TYPED_VALUE;


class B1CUtils
{
public:
	static const std::wstring _un_ops[3];
	static const std::wstring _bin_ops[11];
	static const std::wstring _log_ops[6];


	// converts BASIC1 string (const B1_T_CHAR *) to C string (wstring)
	static std::wstring b1str_to_cstr(const B1_T_CHAR *bstr, bool is_null_terminated = false);
	// converts C string to BASIC1 string
	static B1_T_CHAR *cstr_to_b1str(const std::wstring &cstr, B1_T_CHAR *strbuf);
	static std::wstring get_progline_substring(B1_T_INDEX begin, B1_T_INDEX end, bool double_bkslashes = false);
	// gets string data encoded according to BASIC1 rules (embraced by double-quotes, internal double-quotes are doubled): "string ""data""" -> string "data"
	static B1_T_ERROR get_string_data(const std::wstring &str, std::wstring &sdata, bool quoted_string = true);
	
	static bool is_str_val(const std::wstring &val);
	static bool is_num_val(const std::wstring &val);
	static bool is_imm_val(const std::wstring &val);

	static B1_T_ERROR get_num_min_type(const std::wstring &val, B1Types &type, std::wstring &mod_val);
	// comp_num_types - compatible numeric types (the same size, no need to convert)
	static B1_T_ERROR get_com_type(const B1Types type0, const B1Types type1, B1Types &com_type, bool &comp_num_types);
	// checks if a value of src type can be assigned to a variable of dst type
	static bool are_types_compatible(const B1Types src_type, const B1Types dst_type);

	static bool is_label(const B1_CMP_CMD &cmd);
	static bool is_cmd(const B1_CMP_CMD &cmd);
	static bool is_inline_asm(const B1_CMP_CMD &cmd);
	static bool is_def_fn(const std::wstring &name);
	static bool is_def_fn(const B1_CMP_CMD &cmd);
	static bool is_fn_arg(const std::wstring &name);
	static bool is_local(const std::wstring &name);

	static int get_fn_arg_index(const std::wstring &name);

	static bool is_src(const B1_CMP_CMD &cmd, const std::wstring &val);
	static bool is_dst(const B1_CMP_CMD &cmd, const std::wstring &val);
	static bool is_sub_or_arg(const B1_CMP_CMD &cmd, const std::wstring &val);
	static bool is_used(const B1_CMP_CMD &cmd, const std::wstring &val);

	static bool replace_dst(B1_CMP_CMD &cmd, const std::wstring &val, const B1_CMP_ARG &arg, bool preserve_type = false);
	static bool replace_src(B1_CMP_CMD &cmd, const std::wstring &val, const B1_CMP_ARG &arg, int *count_replaced = nullptr);
	static bool replace_src(B1_CMP_CMD &cmd, const B1_CMP_ARG &src_arg, const B1_CMP_ARG &arg, int *count_replaced = nullptr);
	static bool replace_src_with_subs(B1_CMP_CMD &cmd, const std::wstring &val, const B1_TYPED_VALUE &tv, bool preserve_type = false);
	static bool replace_all(B1_CMP_CMD &cmd, const std::wstring &val, const B1_TYPED_VALUE &tv, bool preserve_type = false);

	static bool arg_is_src(const B1_CMP_CMD &cmd, const B1_CMP_ARG &arg);
	// if is_local = true, the function compares variable by name only (because locals can be reused with different types)
	static bool arg_is_dst(const B1_CMP_CMD &cmd, const B1_CMP_ARG &arg, bool is_local);

	static const B1_TYPED_VALUE *get_dst_var(const B1_CMP_CMD &cmd, bool scalar_var_only);

	// checks local variable types compatibility, returns true if a local of base_type can be used instead of a local of reuse_type
	static bool local_compat_types(const B1Types base_type, const B1Types reuse_type);

	static bool is_log_op(const std::wstring &cmd);
	static bool is_un_op(const std::wstring &cmd);
	static bool is_bin_op(const std::wstring &cmd);

	static bool is_log_op(const B1_CMP_CMD &cmd);
	static bool is_un_op(const B1_CMP_CMD &cmd);
	static bool is_bin_op(const B1_CMP_CMD &cmd);

	static bool get_asm_type(const B1Types type, std::wstring *asmtype = nullptr, int32_t *size = nullptr, int32_t *rep = nullptr, int32_t dimnum = 0);
};


enum class B1_CMD_TYPE
{
	B1_CMD_TYPE_UNKNOWN,
	B1_CMD_TYPE_LABEL,
	B1_CMD_TYPE_COMMAND,
	B1_CMD_TYPE_INLINE_ASM,
};


class B1_TYPED_VALUE
{
public:
	B1Types type;
	std::wstring value;

	B1_TYPED_VALUE();
	B1_TYPED_VALUE(const std::wstring &val, const B1Types tp = B1Types::B1T_UNKNOWN);

	bool operator!=(const B1_TYPED_VALUE &tv) const;

	void clear();
};

class B1_CMP_ARG: public std::vector<B1_TYPED_VALUE>
{
public:
	B1_CMP_ARG();
	B1_CMP_ARG(const std::wstring &val, const B1Types tp = B1Types::B1T_UNKNOWN);

	bool operator==(const B1_CMP_ARG &arg) const;
};

class B1_CMP_ARGS: public std::vector<B1_CMP_ARG>
{
public:
};


class B1_CMP_CMD
{
public:
	B1_CMD_TYPE type;
	std::wstring cmd;
	B1_CMP_ARGS args;

	int32_t line_num;
	int32_t line_cnt;
	int32_t src_file_id;
	int32_t src_line_id;

	B1_CMP_CMD() = delete;
	B1_CMP_CMD(int32_t line_num, int32_t line_cnt, int32_t src_file_id, int32_t src_line_id);

	void clear();
};

class B1_CMP_CMDS: public std::list<B1_CMP_CMD>
{
protected:
	int32_t _next_label;
	int32_t _next_local;

	std::wstring _curr_name_space;

	int32_t _curr_line_num;
	int32_t _curr_line_cnt;
	int32_t _curr_src_file_id;
	int32_t _curr_src_line_id;

public:
	B1_CMP_CMDS();
	B1_CMP_CMDS(const std::wstring &name_space, int32_t next_label = 0, int32_t next_local = 0);

	std::wstring get_name_space_prefix() const;

	void emit_label(const std::wstring &name, const_iterator pos, bool global = false);
	void emit_label(const std::wstring &name, bool global = false);
	
	std::wstring emit_label(const_iterator pos, bool gen_name_only = false);
	std::wstring emit_label(bool gen_name_only = false);

	std::wstring emit_local(const B1Types type, const_iterator pos);
	std::wstring emit_local(const B1Types type);

	// checks if the string is autogenerated local name
	bool is_gen_local(const std::wstring &s) const;

	std::wstring emit_command(const std::wstring &name, const_iterator pos, const std::vector<std::wstring> &args = std::vector<std::wstring>());
	std::wstring emit_command(const std::wstring &name, const std::vector<std::wstring> &args = std::vector<std::wstring>());
	std::wstring emit_command(const std::wstring &name, const_iterator pos, const std::wstring &arg);
	std::wstring emit_command(const std::wstring &name, const std::wstring &arg);
	std::wstring emit_command(const std::wstring &name, const_iterator pos, const std::vector<B1_TYPED_VALUE> &args);
	std::wstring emit_command(const std::wstring &name, const std::vector<B1_TYPED_VALUE> &args);
	std::wstring emit_command(const std::wstring &name, const_iterator pos, const B1_TYPED_VALUE &arg);
	std::wstring emit_command(const std::wstring &name, const_iterator pos, const std::vector<B1_CMP_ARG> &args);
	std::wstring emit_command(const std::wstring &name, const std::vector<B1_CMP_ARG> &args);

	iterator emit_inline_asm(const_iterator pos);
	iterator emit_inline_asm(void);
};


class B1_CMP_FN_ARG
{
public:
	B1Types type;
	bool optional;
	std::wstring defval;

	B1_CMP_FN_ARG() = delete;
	B1_CMP_FN_ARG(const B1Types tp, bool opt = false, const std::wstring &dv = std::wstring());
};

class B1_CMP_FN
{
public:
	std::wstring name;
	B1Types rettype;
	std::vector<B1_CMP_FN_ARG> args;
	std::wstring iname;
	bool isstdfn;

	B1_CMP_FN() = delete;
	B1_CMP_FN(const std::wstring &nm, const B1Types rt, const std::initializer_list<B1_CMP_FN_ARG> &arglist, const std::wstring &in, bool stdfn = true);
	B1_CMP_FN(const std::wstring &nm, const B1Types rt, const std::initializer_list<B1Types> &arglist, const std::wstring &in, bool stdfn = true);
	B1_CMP_FN(const std::wstring &nm, const B1Types rt, const std::vector<B1Types> &arglist, const std::wstring &in, bool stdfn = true);
	B1_CMP_FN(const std::wstring &nm, const B1Types rt, const std::vector<B1_CMP_FN_ARG> &arglist, const std::wstring &in, bool stdfn = true);
};

class B1_CMP_FNS
{
private:
	static const B1_CMP_FN _fns[];


public:
	static bool fn_exists(const std::wstring &name);
	static const B1_CMP_FN *get_fn(const std::wstring &name);
	static const B1_CMP_FN *get_fn(const B1_TYPED_VALUE &val);
	static const B1_CMP_FN *get_fn(const B1_CMP_ARG &arg);
	static std::wstring get_fn_int_name(const std::wstring &name);
};

class B1_CMP_VAR
{
public:
	std::wstring name;
	B1Types type;
	int32_t size;				// variable size in bytes (sengle element size for subscripted variable)
	bool is_volatile;			// volatile variable
	bool is_const;				// constant variable
	int32_t dim_num;			// dimensions count (0 for simple variable)
	bool use_symbol;			// use symbolic name instead of address (address is unknown on this stage)
	int32_t address;			// variable address
	std::wstring symbol;		// constant name
	bool fixed_size;			// fixed size array
	std::vector<int32_t> dims;	// dimensions (for fixed size array)

	int32_t src_line_cnt;
	int32_t src_file_id;

	B1_CMP_VAR();
	B1_CMP_VAR(const std::wstring &nm, const B1Types tp, int32_t dn, bool vlt, bool cnst, int32_t sfid, int32_t slc);
};
