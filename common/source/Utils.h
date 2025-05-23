/*
 BASIC1 compiler
 Copyright (c) 2021-2025 Nikolay Pletnev
 MIT license

 Utils.h: BASIC1 compiler utility classes declaration
*/


#pragma once

extern "C"
{
#include "b1err.h"
}

#include <string>
#include <vector>
#include <map>
#include <set>
#include <any>


enum class B1Types
{
	B1T_INVALID = -1,
	B1T_UNKNOWN = 0,
	B1T_BYTE,
	B1T_INT,
	B1T_WORD,
	B1T_LONG,
	B1T_STRING,

	// special types
	B1T_LABEL = 1000, // used with IOCTL
	B1T_VARREF, // used with IOCTL
	B1T_TEXT, // used with IOCTL
	B1T_COMMON, // used when choosing type of IIF pseudo-function
};


// loading value types
enum class LVT
{
	LVT_NONE = 0,
	LVT_REG = 1,		// value is loaded into register (e.g. A, X, or X+Y pair for STM8, depending on data type)
	LVT_IMMVAL = 2,		// immediate value (for numeric types only)
	LVT_MEMREF = 4,		// memory address (e.g. __VAR_A, __STR_S$, __VAR_B + 0x10)
	LVT_STKREF = 8,		// value in stack (local or function argument, returns offset relative to SP)
	LVT_REGARG = 16,	// function argument passed in register
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


class Utils
{
public:
	static B1_T_ERROR str2int32(const std::wstring &str, int32_t &num);
	[[nodiscard]]
	static std::string wstr2str(const std::wstring &str);
	[[nodiscard]]
	static std::wstring str2wstr(const std::string &str);

	static B1_T_ERROR read_line(std::FILE *fp, std::wstring &str);

	[[nodiscard]]
	static std::wstring str_trim(const std::wstring &str);
	[[nodiscard]]
	static std::wstring str_ltrim(const std::wstring &str, const std::wstring &del);
	[[nodiscard]]
	static std::wstring str_rtrim(const std::wstring &str, const std::wstring &del);
	[[nodiscard]]
	static std::wstring str_delspaces(const std::wstring &str);

	[[nodiscard]]
	static std::string str_trim(const std::string &str);
	[[nodiscard]]
	static std::string str_ltrim(const std::string &str, const std::string &del);
	[[nodiscard]]
	static std::string str_rtrim(const std::string &str, const std::string &del);

	[[nodiscard]]
	static std::wstring str_toupper(const std::wstring &s);
	[[nodiscard]]
	static std::string str_toupper(const std::string &s);

	[[nodiscard]]
	static std::wstring str_tohex16(uint32_t n);
	[[nodiscard]]
	static std::wstring str_tohex32(int32_t n);

	static size_t str_split(const std::wstring &str, const std::wstring &del, std::vector<std::wstring> &out_strs, bool include_dels = false);
	static size_t str_split(const std::wstring &str, const std::vector<wchar_t> &dels, std::vector<std::wstring> &out_strs, bool include_dels = false);

	[[nodiscard]]
	static size_t find_first_of(const std::wstring &str, const std::vector<std::wstring> &search_for, int32_t &val_index);

	[[nodiscard]]
	static std::wstring get_type_name(B1Types type);

	[[nodiscard]]
	static B1Types get_type_by_name(const std::wstring &type_name);
	[[nodiscard]]
	static B1Types get_type_by_type_spec(const std::wstring &name, B1Types expl_type);

	[[nodiscard]]
	static bool check_const_name(const std::wstring &const_name);
	[[nodiscard]]
	static B1Types get_const_type(const std::wstring &const_name);

	static void correct_int_value(int32_t &n, const B1Types type);


	static int32_t int32power(int32_t base, uint32_t exp);

	[[nodiscard]]
	static std::wstring any2wstr(const std::any &any_val);
};


class Settings
{
public:
	class IoCmd
	{
	public:
		enum class IoCmdCallType
		{
			CT_CALL,
			CT_INL,
		};

		enum class IoCmdCodePlacement
		{
			CP_CURR_POS,
			CP_END,
		};

		int32_t id;
		IoCmdCallType call_type;
		IoCmdCodePlacement code_place;
		std::wstring file_name;
		int32_t mask;
		bool accepts_data;
		B1Types data_type;
		std::wstring extra_data;
		bool predef_only;
		std::map<std::wstring, std::wstring> values;
		std::wstring def_val;
		std::vector<std::pair<B1Types, int32_t>> more_masks;

		IoCmd()
		: id(-1)
		, call_type(IoCmdCallType::CT_CALL)
		, code_place(IoCmdCodePlacement::CP_CURR_POS)
		, mask(0)
		, accepts_data(false)
		, data_type(B1Types::B1T_UNKNOWN)
		, predef_only(true)
		{
		}

		void clear()
		{
			id = -1;
			call_type = IoCmdCallType::CT_CALL;
			code_place = IoCmdCodePlacement::CP_CURR_POS;
			file_name.clear();
			mask = 0;
			accepts_data = false;
			data_type = B1Types::B1T_UNKNOWN;
			extra_data.clear();
			predef_only = true;
			values.clear();
			def_val.clear();
			more_masks.clear();
		}
	};

protected:
	std::string _target_name;
	std::string _MCU_name;
	bool _embedded;
	bool _compressed;

	std::vector<std::string> _lib_dirs;

	std::map<std::wstring, std::wstring> _settings;

	std::map<std::wstring, std::map<std::wstring, IoCmd>> _io_settings;

	mutable std::map<std::wstring, std::set<std::wstring>> _io_dev_options;

	std::map<std::string, int32_t> _int_names;


	bool _print_warnings;
	bool _print_warning_desc;
	bool _print_error_desc;

	bool _mem_model_small;

	int _ret_address_size;

	bool _fix_addresses;

	bool _fix_ret_stk_ptr;

	int32_t _RAM_start;
	int32_t _RAM_size;

	int32_t _ROM_start;
	int32_t _ROM_size;

	int32_t _stack_size;

	int32_t _heap_size;


	static bool get_field(std::wstring &line, bool optional, std::wstring &value);


public:
	Settings()
	: _print_warnings(true)
	, _print_warning_desc(true)
	, _print_error_desc(true)

	, _embedded(false)
	, _compressed(true)

	, _mem_model_small(true)
	, _ret_address_size(-1)
	, _fix_addresses(false)
	, _fix_ret_stk_ptr(false)

	, _RAM_start(-1)
	, _RAM_size(-1)
	, _ROM_start(-1)
	, _ROM_size(-1)
	, _stack_size(-1)
	, _heap_size(-1)
	{
	}

	void Init(	int32_t RAM_start,
			int32_t RAM_size,
			int32_t ROM_start,
			int32_t ROM_size,
			int32_t stack_size,
			int32_t heap_size,
			int ret_addr_size)
	{
		if(_RAM_start == -1) _RAM_start = RAM_start;
		if(_RAM_size == -1) _RAM_size = RAM_size;
		if(_ROM_start == -1) _ROM_start = ROM_start;
		if(_ROM_size == -1) _ROM_size = ROM_size;
		if(_stack_size == -1) _stack_size = stack_size;
		if(_heap_size == -1) _heap_size = heap_size;
		_ret_address_size = ret_addr_size;
	}

	int32_t GetRAMStart() const { return _RAM_start; }
	void SetRAMStart(int32_t RAM_start) { _RAM_start = RAM_start; }

	int32_t GetROMStart() const { return _ROM_start; }
	void SetROMStart(int32_t ROM_start) { _ROM_start = ROM_start; }

	int32_t GetRAMSize() const { return _RAM_size; }
	void SetRAMSize(int32_t RAM_size) { _RAM_size = RAM_size; }

	int32_t GetROMSize() const { return _ROM_size; }
	void SetROMSize(int32_t ROM_size) { _ROM_size = ROM_size; }

	int32_t GetStackSize() const { return _stack_size; }
	void SetStackSize(int32_t stack_size) { _stack_size = stack_size; }

	int32_t GetHeapSize() const { return _heap_size; }
	void SetHeapSize(int32_t heap_size) { _heap_size = heap_size; }

	void SetMemModelSmall() { _mem_model_small = true;  }
	void SetMemModelLarge() { _mem_model_small = false; }

	void SetEmbedded(bool embedded = true) { _embedded = embedded; }
	bool GetEmbedded() const { return _embedded; }

	void SetCompressed(bool compressed = true) { _compressed = compressed; }
	bool GetCompressed() const { return _compressed; }

	bool GetMemModelSmall() const { return _mem_model_small; }
	bool GetMemModelLarge() const { return !_mem_model_small; }

	void SetRetAddressSize(int ret_addr_size) { _ret_address_size = ret_addr_size;  }
	int GetRetAddressSize() const { return _ret_address_size; }

	void SetFixAddresses() { _fix_addresses = true; }
	bool GetFixAddresses() const { return _fix_addresses; }

	void SetFixRetStackPtr() { _fix_ret_stk_ptr = true; }
	bool GetFixRetStackPtr() const { return _fix_ret_stk_ptr; }

	bool GetPrintWarnings() const { return _print_warnings; }
	bool GetPrintWarningDesc() const { return _print_warning_desc; }

	B1_T_ERROR Read(const std::string &file_name);
	bool GetValue(const std::wstring &key, std::wstring &value) const;

	void SetTargetName(const std::string &target_name) { _target_name = target_name;  }
	std::string GetTargetName() const { return _target_name; }

	void SetMCUName(const std::string &MCU_name) { _MCU_name = MCU_name; }
	void SetLibDir(const std::string &lib_dir);
	std::string GetLibFileName(const std::string &file_name, const std::string &ext) const;

	B1_T_ERROR ReadIoSettings(const std::string &file_name);
	bool GetIoCmd(const std::wstring &dev_name, const std::wstring &cmd_name, IoCmd &cmd) const;

	std::wstring GetIoDeviceName(const std::wstring &spec_dev_name) const;

	// returns interrupt index by name or -1 if not an interrupt name was specified
	int GetInterruptIndex(const std::string &int_name) const;
	// splits source file name to interrupt name and file name itself
	std::string GetInterruptName(const std::string &file_name, std::string &real_file_name) const;

	std::vector<std::wstring> GetDevList() const;
	std::wstring GetDefaultDeviceName(const std::wstring &real_dev_name) const;
	std::vector<std::wstring> GetDevCmdsList(const std::wstring &dev_name) const;
	std::vector<std::wstring> GetIoDevList() const;
	const std::set<std::wstring> *GetDeviceOptions(const std::wstring &dev_name) const;

	virtual B1_T_ERROR ProcessNumPostfix(const std::wstring &postfix, int32_t &n) const;
};
