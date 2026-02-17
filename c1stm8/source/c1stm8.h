/*
 STM8 intermediate code compiler
 Copyright (c) 2021-2026 Nikolay Pletnev
 MIT license

 c1stm8.h: STM8 intermediate code compiler classes declaration
*/


#pragma once

#include "../../common/source/c1.h"


class B1_ASM_OP_STM8: public B1_ASM_OP
{
public:
	mutable bool _parsed;
	mutable std::wstring _op;
	mutable std::vector<std::wstring> _args;


	B1_ASM_OP_STM8() = delete;

	B1_ASM_OP_STM8(AOT type, const std::wstring &data, const std::wstring &comment, bool is_volatile, bool is_inline)
	: B1_ASM_OP(type, data, comment, is_volatile, is_inline)
	, _parsed(false)
	{
	}

	B1_ASM_OP_STM8(B1_ASM_OP_STM8 &op)
	: B1_ASM_OP_STM8(op._type, op._data, op._comment, op._volatile, op._is_inline)
	{
	}

	bool ParseNumeric(const std::wstring &num_str, int32_t &n) const;
	bool Parse() const;
};

class C1STM8Compiler: public C1Compiler
{
protected:
	bool _page0;
	B1_ASM_OPS _page0_sec;

	int32_t _stack_ptr;
	std::vector<std::pair<B1_TYPED_VALUE, int32_t>> _local_offset;

	// total size of all arguments of the current UDEF
	int32_t _curr_udef_args_size;
	// offsets of the current user-defined function's arguments passed in stack (e.g. {5, 3, 1})
	std::vector<int32_t> _curr_udef_arg_offsets;
	// offsets of the current UDEF's string arguments
	std::vector<int32_t> _curr_udef_str_arg_offsets;
	std::map<int32_t, B1_ASM_OPS::const_iterator> _curr_udef_str_arg_last_use;

	// _cmp_active is set to true by a comparison operator, LA, LF, JT and JF operators do not change the variable, other operators set it to false
	bool _cmp_active;
	std::wstring _cmp_op;
	B1Types _cmp_type;

	bool _retval_active;
	B1Types _retval_type;

	// just created local string variables (no need to call __LIB_STR_RLS when assigning a value)
	std::set<std::wstring> _clear_locals;

	std::set<std::wstring> _allocated_arrays;

	std::map<int, std::wstring> _irq_handlers;

	std::vector<std::pair<const_iterator, std::map<std::wstring, std::wstring>>> _end_placement;

	//                   iterator        store arg   file id  line cnt
	std::list<std::tuple<const_iterator, B1_CMP_ARG, int32_t, int32_t>> _store_at;

	C1_T_ERROR process_asm_cmd(const std::wstring &line) override;

	B1_ASM_OPS::iterator create_asm_op(B1_ASM_OPS &sec, B1_ASM_OPS::const_iterator where, AOT type, const std::wstring &lbl, bool is_volatile, bool is_inline) override;

	C1_T_ERROR stm8_calc_array_size(const B1_CMP_VAR &var, int32_t size1);
	C1_T_ERROR stm8_st_gf(const B1_CMP_VAR &var, bool is_ma);
	C1_T_ERROR stm8_arrange_types(const B1Types type_from, const B1Types type_to);
	int32_t stm8_get_local_offset(const std::wstring &local_name);
	int32_t stm8_get_type_cvt_offset(B1Types type_from, B1Types type_to);
	C1_T_ERROR stm8_load_from_stack(int32_t offset, const B1Types init_type, const B1Types req_type, LVT req_valtype, LVT &rvt, std::wstring &rv, B1_ASM_OPS::const_iterator *str_last_use_it = nullptr);
	std::wstring stm8_get_var_addr(const std::wstring &var_name, B1Types type_from, B1Types type_to, bool direct_cvt, bool *volatile_var = nullptr);
	C1_T_ERROR stm8_load(const B1_TYPED_VALUE &tv, const B1Types req_type, LVT req_valtype, LVT *res_valtype = nullptr, std::wstring *res_val = nullptr, bool *volatile_var = nullptr);
	C1_T_ERROR stm8_arr_alloc_def(const B1_CMP_VAR &var);
	C1_T_ERROR stm8_arr_offset(const B1_CMP_ARG &arg, bool &imm_offset, int32_t &offset);
	C1_T_ERROR stm8_load(const B1_CMP_ARG &arg, const B1Types req_type, LVT req_valtype, LVT *res_valtype = nullptr, std::wstring *res_val = nullptr, bool *volatile_var = nullptr);
	C1_T_ERROR stm8_init_array(const B1_CMP_CMD &cmd, const B1_CMP_VAR &var);
	C1_T_ERROR stm8_st_ga(const B1_CMP_CMD &cmd, const B1_CMP_VAR &var);
	C1_T_ERROR stm8_store(const B1_TYPED_VALUE &tv);
	C1_T_ERROR stm8_store(const B1_CMP_ARG &arg);
	C1_T_ERROR stm8_assign(const B1_CMP_CMD &cmd, bool omit_zero_init);
	C1_T_ERROR stm8_un_op(const B1_CMP_CMD &cmd, bool omit_zero_init);
	C1_T_ERROR stm8_add_op(const B1_CMP_CMD &cmd);
	C1_T_ERROR stm8_mul_op(const B1_CMP_CMD &cmd);
	C1_T_ERROR stm8_bit_op(const B1_CMP_CMD &cmd);
	C1_T_ERROR stm8_add_shift_op(const std::wstring &shift_cmd, const B1Types type);
	C1_T_ERROR stm8_shift_op(const B1_CMP_CMD &cmd);
	C1_T_ERROR stm8_num_cmp_op(const B1_CMP_CMD &cmd);
	C1_T_ERROR stm8_str_cmp_op(const B1_CMP_CMD &cmd);
	C1_T_ERROR stm8_load_ptr(const B1_CMP_ARG &first, const B1_CMP_ARG &count);
	C1_T_ERROR stm8_write_ioctl_fn(const B1_CMP_ARG &arg);
	C1_T_ERROR stm8_write_ioctl(std::list<B1_CMP_CMD>::iterator &cmd_it);

	C1_T_ERROR write_data_sec(bool code_init) override;
	C1_T_ERROR write_code_sec(bool code_init) override;

	std::wstring correct_SP_offset(const std::wstring &arg, int32_t op_size, bool &no_SP_off, int32_t *offset = nullptr) const;
	bool is_arithm_op(const B1_ASM_OP_STM8 &ao, int32_t &size, bool *uses_SP = nullptr) const;
	bool is_reg_used(const B1_ASM_OP_STM8 &ao, const std::wstring &reg_name, bool &reg_write_op) const;
	bool is_reg_used_after(B1_ASM_OPS::const_iterator start, B1_ASM_OPS::const_iterator end, const std::wstring &reg_name, bool branch = false) const;


public:
	C1STM8Compiler() = delete;
	C1STM8Compiler(bool out_src_lines, bool opt_nocheck);
	virtual ~C1STM8Compiler();

	C1_T_ERROR WriteCodeInitBegin();
	C1_T_ERROR WriteCodeInitDAT();
	C1_T_ERROR WriteCodeInitEnd();

	C1_T_ERROR Optimize1(bool &changed);
	C1_T_ERROR Optimize2(bool &changed);
	C1_T_ERROR Optimize3(bool &changed);

	 C1_T_ERROR Save(const std::string &file_name, bool overwrite_existing = true) override;
};
