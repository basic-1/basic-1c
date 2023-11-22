/*
 BASIC1 compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
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
	B1T_COMMON, // used when choosing type of IIF pseudo-function
};


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

	static size_t str_split(const std::wstring &str, const std::wstring &del, std::vector<std::wstring> &out_strs);

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
		bool predef_only;
		std::map<std::wstring, std::wstring> values;

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
			mask = 0;
			accepts_data = false;
			data_type = B1Types::B1T_UNKNOWN;
			predef_only = true;
			values.clear();
		}
	};

protected:
	std::string _target_name;
	std::string _MCU_name;

	std::vector<std::string> _lib_dirs;

	std::map<std::wstring, std::wstring> _settings;

	std::map<std::wstring, std::map<std::wstring, IoCmd>> _io_settings;

	std::map<std::string, int32_t> _int_names;


	bool _print_warnings;
	bool _print_warning_desc;
	bool _print_error_desc;

	bool _mem_model_small;

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
	Settings(	int32_t RAM_start,
				int32_t RAM_size,
				int32_t ROM_start,
				int32_t ROM_size,
				int32_t stack_size,
				int32_t heap_size)
	: _print_warnings(true)
	, _print_warning_desc(true)
	, _print_error_desc(true)

	, _mem_model_small(true)
	, _fix_addresses(false)
	, _fix_ret_stk_ptr(false)

	, _RAM_start(RAM_start)
	, _RAM_size(RAM_size)
	, _ROM_start(ROM_start)
	, _ROM_size(ROM_size)
	, _stack_size(stack_size)
	, _heap_size(heap_size)
	{
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

	bool GetMemModelSmall() { return _mem_model_small; }
	bool GetMemModelLarge() { return !_mem_model_small; }

	void SetFixAddresses() { _fix_addresses = true; }
	bool GetFixAddresses() { return _fix_addresses; }

	void SetFixRetStackPtr() { _fix_ret_stk_ptr = true; }
	bool GetFixRetStackPtr() { return _fix_ret_stk_ptr; }

	bool GetPrintWarnings() const { return _print_warnings; }
	bool GetPrintWarningDesc() const { return _print_warning_desc; }

	B1_T_ERROR Read(const std::string &file_name);
	bool GetValue(const std::wstring &key, std::wstring &value) const;

	void SetTargetName(const std::string &target_name) { _target_name = target_name;  }
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
	std::wstring GetCommonDeviceName(const std::wstring &real_dev_name) const;
	std::vector<std::wstring> GetDevCmdsList(const std::wstring &dev_name) const;
};
