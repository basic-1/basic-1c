/*
 BASIC1 compiler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 Utils.cpp: BASIC1 compiler utility classes
*/


#include "Utils.h"

#include <cwctype>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <linux/limits.h>
#endif

#include "moresym.h"


B1_T_ERROR Utils::str2int32(const std::wstring &str, int32_t &num, B1Types *type /*= nullptr*/)
{
	bool neg = false;
	int base = 10;
	const wchar_t *strptr;

	strptr = str.c_str();

	if(*strptr == L'-')
	{
		neg = true;
		strptr++;
	}

	if(*strptr == L'0' && (*(strptr + 1) == L'x' || *(strptr + 1) == L'X'))
	{
		base = 16;
		strptr += 2;
	}

	wchar_t c, c1;
	int64_t n = 0;
	bool start = true;
	bool lead_zero = false;

	if(type != nullptr)
	{
		*type = B1Types::B1T_UNKNOWN;
	}

	while(true)
	{
		c = *strptr;

		// the only numeric data type specifier is % at the moment
		if(c == L'\0' || c == L'.' || c == L'%')
		{
			break;
		}

		// skip leading zeroes
		if(start && c == L'0')
		{
			strptr++;
			lead_zero = true;
			continue;
		}

		start = false;

		switch(base)
		{
			case 10:
				if(!(c >= L'0' && c <= L'9'))
				{
					return B1_RES_EINVNUM;
				}

				n *= 10;
				n += (c - L'0');
				break;
			case 16:
				if(c >= L'a' && c <= L'f')
				{
					c -= (L'a' - L'A');
				}

				if(!((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F')))
				{
					return B1_RES_EINVNUM;
				}

				n *= 16;
				n += (c >= L'A') ? (c - L'A' + 10) : (c - L'0');
				break;
		}

		if(n > UINT32_MAX)
		{
			return B1_RES_ENUMOVF;
		}

		strptr++;
	}

	if(n == 0 && !lead_zero)
	{
		return B1_RES_EINVNUM;
	}

	int32_t nn;

	if(neg)
	{
		if(base == 10)
		{
			if(n > ((int64_t)INT32_MAX) + 1)
			{
				return B1_RES_ENUMOVF;
			}
		}
		else
		{
			if(n == ((int64_t)INT32_MAX) + 1)
			{
				return B1_RES_ENUMOVF;
			}
		}

		nn = -(int32_t)n;
	}
	else
	{
		if(base == 10)
		{
			if(n > INT32_MAX)
			{
				return B1_RES_ENUMOVF;
			}
		}

		nn = (int32_t)n;
	}

	if(c == L'\0')
	{
		if(type != nullptr)
		{
			*type = B1Types::B1T_LONG;
		}

		num = nn;
	}
	else
	if(c == L'.')
	{
		c = *(++strptr);
		if(c == L'\0')
		{
			return B1_RES_EINVNUM;
		}
		c1 = *(++strptr);

		if(c != L'\0')
		{
			if(type != nullptr)
			{
				*type = neg ? B1Types::B1T_INT : B1Types::B1T_WORD;
			}

			if(c == L'l' || c == L'L')
			{
				num = (uint16_t)nn;
			}
			else
			if(c == L'h' || c == L'H')
			{
				num = (uint16_t)(nn >> 16);
			}
			else
			{
				return B1_RES_EINVNUM;
			}

			if(c1 != L'\0')
			{
				if(type != nullptr)
				{
					*type = B1Types::B1T_BYTE;
				}

				if(*(++strptr) != L'\0')
				{
					return B1_RES_EINVNUM;
				}

				if(c1 == L'l' || c1 == L'L')
				{
					num = (uint8_t)num;
				}
				else
				if(c1 == L'h' || c1 == L'H')
				{
					num = (uint8_t)(num >> 8);
				}
				else
				{
					return B1_RES_EINVNUM;
				}
			}
		}
	}
	else
	if(c == L'%')
	{
		if(*(++strptr) != L'\0')
		{
			return B1_RES_EINVNUM;
		}

		if(type != nullptr)
		{
			*type = B1Types::B1T_INT;
		}

		num = (uint16_t)nn;
	}
	else
	{
		return B1_RES_EINVNUM;
	}

	return B1_RES_OK;
}

std::string Utils::wstr2str(const std::wstring &str)
{
	std::string s;

	std::transform(str.cbegin(), str.cend(), std::back_inserter(s), [](wchar_t c) { return (char)c; });
	return s;
}

std::wstring Utils::str2wstr(const std::string &str)
{
	std::wstring s;

	std::transform(str.cbegin(), str.cend(), std::back_inserter(s), [](char c) { return (wchar_t)c; });
	return s;
}

B1_T_ERROR Utils::read_line(std::FILE *fp, std::wstring &str)
{
	static char buffer[256];
	bool newline;

	if(std::feof(fp))
	{
		return B1_RES_EEOF;
	}

	str.clear();

	while(std::fgets(buffer, sizeof(buffer), fp) != nullptr)
	{
		newline = false;

		for(auto c: buffer)
		{
			if(c == 0)
			{
				if(newline || std::feof(fp))
				{
					return B1_RES_OK;
				}

				break;
			}

			if(c == '\r' || c == '\n')
			{
				newline = true;
			}

			str += (wchar_t)c;
		}
	}

	if(std::feof(fp))
	{
		return B1_RES_EEOF;
	}

	return B1_RES_EENVFAT;
}

std::wstring Utils::str_trim(const std::wstring &str)
{
	auto b = str.begin();
	auto e = str.end();

	// skip leading and trailing spaces
	while(b != e && std::iswspace(*b)) b++;
	while(b != e && std::iswspace(*(e - 1))) e--;

	return std::wstring(b, e);
}

std::wstring Utils::str_ltrim(const std::wstring &str, const std::wstring &del)
{
	auto b = str.begin();
	auto e = str.end();

	// skip leading characters
	while(b != e && del.find(*b) != std::wstring::npos) b++;

	return std::wstring(b, e);
}

std::wstring Utils::str_rtrim(const std::wstring &str, const std::wstring &del)
{
	auto b = str.begin();
	auto e = str.end();

	// skip trailing characters
	while (b != e && del.find(*(e - 1)) != std::wstring::npos) e--;

	return std::wstring(b, e);
}

std::wstring Utils::str_toupper(const std::wstring &s)
{
	std::wstring ostr;

	for(auto c: s)
	{
		ostr += (wchar_t)std::toupper((unsigned char)c);
	}

	return ostr;
}

std::string Utils::str_toupper(const std::string &s)
{
	std::string ostr;

	for(auto c: s)
	{
		ostr += std::toupper((unsigned char)c);
	}

	return ostr;
}

std::wstring Utils::str_tohex16(uint32_t n)
{
	std::wostringstream ss;
	ss << L"0x" << std::hex << (uint16_t)n;
	return ss.str();
}

size_t Utils::str_split(const std::wstring &str, const std::wstring &del, std::vector<std::wstring> &out_strs)
{
	size_t prev = 0;
	auto pos = str.find(del);
	while(pos != str.npos)
	{
		out_strs.push_back(str.substr(prev, pos - prev));
		prev = pos + del.length();
		pos = str.find(del, prev);
	}
	out_strs.push_back(str.substr(prev));
	return out_strs.size();
}

std::wstring Utils::get_type_name(B1Types type)
{
	switch(type)
	{
		case B1Types::B1T_BYTE:
			return L"BYTE";
		case B1Types::B1T_INT:
			return L"INT";
		case B1Types::B1T_WORD:
			return L"WORD";
		case B1Types::B1T_LONG:
			return L"LONG";
		case B1Types::B1T_STRING:
			return L"STRING";
	}

	return std::wstring();
}

bool Utils::check_const_name(const std::wstring& const_name)
{
	auto cn = Utils::str_toupper(const_name);

	if(_RTE_errors.find(cn) != _RTE_errors.cend())
	{
		return true;
	}

	if(_B1C_consts.find(cn) != _B1C_consts.cend())
	{
		return true;
	}

	if(_B1AT_consts.find(cn) != _B1AT_consts.cend())
	{
		return true;
	}

	return false;
}

std::wstring Utils::get_const_type(const std::wstring& const_name)
{
	auto cn = Utils::str_toupper(const_name);
	auto type = B1Types::B1T_UNKNOWN;

	if(_RTE_errors.find(cn) != _RTE_errors.cend())
	{
		type = RTE_ERROR_TYPE;
	}
	else
	{
		const auto bci = _B1C_consts.find(cn);
		if(bci != _B1C_consts.cend())
		{
			type = bci->second.second;
		}
		else
		{
			const auto bati = _B1AT_consts.find(cn);
			if(bati != _B1AT_consts.cend())
			{
				type = bati->second;
			}
		}
	}

	return get_type_name(type);
}


B1_T_ERROR Settings::Read(const std::string &file_name)
{
	std::FILE *fp = std::fopen(file_name.c_str(), "rt");
	if(fp == nullptr)
	{
		return B1_RES_EENVFAT;
	}

	_settings.clear();

	while(true)
	{
		std::wstring line;

		auto err = Utils::read_line(fp, line);
		if(err == B1_RES_EEOF)
		{
			err = B1_RES_OK;
			break;
		}

		if(err != B1_RES_OK)
		{
			std::fclose(fp);
			return B1_RES_EENVFAT;
		}

		line = Utils::str_trim(line);
		if(line.empty() || line.front() == L';' || line.front() == L'\'' || line.front() == L'!' || line.front() == L'#')
		{
			continue;
		}

		auto pos = line.find(L'=');
		if(pos == std::wstring::npos)
		{
			continue;
		}

		const auto name = Utils::str_toupper(Utils::str_rtrim(line.substr(0, pos), L" \t\r\n"));
		const auto value = Utils::str_ltrim(line.substr(pos + 1), L" \t\r\n");

		// read interrupt names
		int32_t int_num = -1;

		if(name.substr(0, 3) == L"INT" && name.substr(name.length() - 5) == L"_NAME" && Utils::str2int32(name.substr(3, name.length() - 3 - 5), int_num) == B1_RES_OK)
		{
			std::vector<std::wstring> ins;
			Utils::str_split(value, L",", ins);
			for(const auto &in: ins)
			{
				_int_names[Utils::wstr2str(Utils::str_trim(in))] = int_num;
			}
		}
		else
		{
			_settings[name] = value;
		}
	}

	std::fclose(fp);

#define SETTINGS_READ_NUM_PROPERTY(NAME, VAR) \
{ \
	const auto s = _settings.find(L"" #NAME); \
	if(s != _settings.end()) \
	{ \
		int32_t n; \
		auto err = Utils::str2int32(s->second, n); \
		if(err != B1_RES_OK) \
		{ \
			return err; \
		} \
		VAR = n; \
	} \
}

	SETTINGS_READ_NUM_PROPERTY(RAM_START, _RAM_start);
	SETTINGS_READ_NUM_PROPERTY(RAM_SIZE, _RAM_size);
	SETTINGS_READ_NUM_PROPERTY(ROM_START, _ROM_start);
	SETTINGS_READ_NUM_PROPERTY(ROM_SIZE, _ROM_size);

	return B1_RES_OK;
}

bool Settings::GetValue(const std::wstring &key, std::wstring &value) const
{
	const auto v = _settings.find(key);

	if(v != _settings.end())
	{
		value = v->second;
		return true;
	}

	return false;
}

// get libraries directory
void Settings::SetLibDir(const std::string &lib_dir)
{
	std::string lib_dir1 = lib_dir;

	if(lib_dir.empty())
	{
#ifdef _WIN32
		char path[MAX_PATH];

		path[0] = '\0';
		::GetModuleFileNameA(NULL, path, MAX_PATH);
#else
		char path[PATH_MAX];

		path[0] = '\0';
		auto count = ::readlink("/proc/self/exe", path, PATH_MAX);
		if(count > 0 && count < PATH_MAX)
		{
			path[count] = '\0';
		}
#endif

		lib_dir1 = path;
	}

	// remove double quotes
	if(!lib_dir1.empty() && lib_dir1.front() == '\"')
	{
		lib_dir1.erase(lib_dir1.begin());
	}
	if(!lib_dir1.empty() && lib_dir1.back() == '\"')
	{
		lib_dir1.erase(lib_dir1.end() - 1);
	}

	// remove executable name
	if(lib_dir.empty())
	{
		auto delpos = lib_dir1.find_last_of("\\/");
		if(delpos != std::string::npos)
		{
			lib_dir1.erase(delpos, std::string::npos);
		}
		else
		{
			lib_dir1.clear();
		}

#ifndef _WIN32
		// remove /bin and add /share/<project_name>
		if(lib_dir1.rfind("/bin") == lib_dir1.length() - 4)
		{
			lib_dir1.erase(lib_dir1.length() - 4, std::string::npos);
			lib_dir1 += "/share";
			lib_dir1 += "/b1c";
		}
#endif
	}

	if(lib_dir1.length() > 1 && (lib_dir1.back() == '\\' || lib_dir1.back() == '/'))
	{
		lib_dir1.erase(lib_dir1.end() - 1);
	}

	if(lib_dir1.empty())
	{
		lib_dir1 = ".";
	}

	// build library directories list
	if(!_target_name.empty())
	{
		lib_dir1 += "/lib/";
		_lib_dirs.push_back(lib_dir1 + _target_name + "/");
		_lib_dirs.push_back(lib_dir1 + _target_name + "/" + (_mem_model_small ? "small" : "large") + "/");
		if(!_MCU_name.empty())
		{
			_lib_dirs.push_back(lib_dir1 + _target_name + "/" + _MCU_name + "/");
			_lib_dirs.push_back(lib_dir1 + _target_name + "/" + _MCU_name + "/" + (_mem_model_small ? "small" : "large") + "/");
		}
	}
}

std::string Settings::GetLibFileName(const std::string &file_name, const std::string &ext) const
{
	for(auto dir = _lib_dirs.rbegin(); dir != _lib_dirs.rend(); dir++)
	{
		FILE *fp = std::fopen((*dir + file_name + ext).c_str(), "r");
		if(fp != nullptr)
		{
			std::fclose(fp);
			return *dir + file_name + ext;
		}
	}

	return std::string();
}

bool Settings::get_field(std::wstring &line, bool optional, std::wstring &value)
{
	value.clear();
	if(line.empty())
	{
		return optional;
	}

	auto pos = line.find(L',');
	value = Utils::str_toupper(Utils::str_trim(line.substr(0, pos)));
	if(!optional && value.empty())
	{
		return false;
	}

	if(pos != std::wstring::npos)
	{
		pos++;
	}
	line.erase(0, pos);

	return true;
}

B1_T_ERROR Settings::ReadIoSettings(const std::string &file_name)
{
	std::FILE *fp = std::fopen(file_name.c_str(), "rt");
	if(fp == nullptr)
	{
		return B1_RES_EENVFAT;
	}

	_io_settings.clear();

	std::vector<std::wstring> dev_names;
	std::map<std::wstring, IoCmd> cmds;

	while(true)
	{
		std::wstring line;

		auto err = Utils::read_line(fp, line);
		if(err == B1_RES_EEOF)
		{
			err = B1_RES_OK;
			break;
		}

		if(err != B1_RES_OK)
		{
			std::fclose(fp);
			return B1_RES_EENVFAT;
		}

		line = Utils::str_trim(line);
		if(line.empty() || line.front() == L';' || line.front() == L'\'' || line.front() == L'!' || line.front() == L'#')
		{
			continue;
		}

		// read new device definition
		if(line.front() == L'[')
		{
			auto pos = line.find(L']');
			if(pos == std::wstring::npos)
			{
				std::fclose(fp);
				return B1_RES_ESYNTAX;
			}

			if(!dev_names.empty())
			{
				if(cmds.empty())
				{
					std::fclose(fp);
					return B1_RES_ESYNTAX;
				}

				for(const auto &dn: dev_names)
				{
					_io_settings.emplace(std::make_pair(Utils::str_toupper(Utils::str_trim(dn)), cmds));
				}
			}
			
			//dev_name = Utils::str_toupper(Utils::str_trim(line.substr(1, pos - 1)));
			dev_names.clear();
			Utils::str_split(line.substr(1, pos - 1), L",", dev_names);

			if(dev_names.empty())
			{
				std::fclose(fp);
				return B1_RES_ESYNTAX;
			}

			cmds.clear();

			continue;
		}

		if(dev_names.empty())
		{
			std::fclose(fp);
			return B1_RES_ESYNTAX;
		}

		std::wstring cmd_name, name, value;
		IoCmd cmd;
		
		// read command
		// name
		if(!get_field(line, false, cmd_name))
		{
			return B1_RES_ESYNTAX;
		}

		// id
		if(!get_field(line, false, value))
		{
			return B1_RES_ESYNTAX;
		}
		err = Utils::str2int32(value, cmd.id);
		if(err != B1_RES_OK)
		{
			return err;
		}

		// call type
		if(!get_field(line, false, value))
		{
			return B1_RES_ESYNTAX;
		}
		if(Utils::str_toupper(value) == L"CALL")
		{
			cmd.call_type = IoCmd::IoCmdCallType::CT_CALL;
		}
		else
		if(Utils::str_toupper(value) == L"INL")
		{
			cmd.call_type = IoCmd::IoCmdCallType::CT_INL;
		}
		else
		{
			return B1_RES_ESYNTAX;
		}

		// file name
		if(!get_field(line, true, value))
		{
			return B1_RES_ESYNTAX;
		}
		cmd.file_name = value;

		// mask
		if(!get_field(line, true, value))
		{
			return B1_RES_ESYNTAX;
		}
		if(!value.empty())
		{
			err = Utils::str2int32(value, cmd.mask);
			if(err != B1_RES_OK)
			{
				return err;
			}
		}

		// accepts data
		if(!get_field(line, true, value))
		{
			return B1_RES_ESYNTAX;
		}
		if(!value.empty())
		{
			if(value == L"TRUE")
			{
				cmd.accepts_data = true;
			}
			else
			if(value == L"FALSE")
			{
				cmd.accepts_data = false;
			}
			else
			{
				return B1_RES_ESYNTAX;
			}
		}

		// data type
		if(!get_field(line, true, value))
		{
			return B1_RES_ESYNTAX;
		}
		cmd.data_type = value;

		// predefined values only
		if(!get_field(line, true, value))
		{
			return B1_RES_ESYNTAX;
		}
		if(!value.empty())
		{
			if(value == L"TRUE")
			{
				cmd.predef_only = true;
			}
			else
			if(value == L"FALSE")
			{
				cmd.predef_only = false;
			}
			else
			{
				return B1_RES_ESYNTAX;
			}
		}

		// values
		while(!line.empty())
		{
			if(!get_field(line, false, name) || !get_field(line, false, value))
			{
				return B1_RES_ESYNTAX;
			}

			cmd.values.emplace(std::make_pair(name, value));
		}

		cmds.emplace(std::make_pair(cmd_name, cmd));
	}

	std::fclose(fp);

	if(!dev_names.empty())
	{
		if(cmds.empty())
		{
			return B1_RES_ESYNTAX;
		}

		for(const auto &dn: dev_names)
		{
			_io_settings.emplace(std::make_pair(Utils::str_toupper(Utils::str_trim(dn)), cmds));
		}
	}

	return B1_RES_OK;
}

bool Settings::GetIoCmd(const std::wstring &dev_name, const std::wstring &cmd_name, IoCmd &cmd) const
{
	auto dc = _io_settings.find(dev_name);
	if(dc == _io_settings.end())
	{
		return false;
	}

	auto c = dc->second.find(cmd_name);
	if(c != dc->second.end())
	{
		cmd = c->second;
		return true;
	}

	return false;
}

std::wstring Settings::GetIoDeviceName(const std::wstring &spec_dev_name) const
{
	std::wstring def_dev_name = spec_dev_name;

	if(spec_dev_name.empty())
	{
		GetValue(L"DEFAULT_IO_DEVICE", def_dev_name);
	}

	std::wstring dev_name;
	int dev_num = 0;

	while(GetValue(L"DEVICE_NAME" + std::to_wstring(dev_num), dev_name))
	{
		if(def_dev_name == dev_name)
		{
			GetValue(L"REAL_DEVICE_NAME" + std::to_wstring(dev_num), dev_name);
			return dev_name;
		}
		dev_num++;
	}

	return def_dev_name;
}

// returns interrupt index by name or -1 if not an interrupt name was specified
int Settings::GetInterruptIndex(const std::string &int_name) const
{
	auto ii = _int_names.find(int_name);
	return (ii == _int_names.end()) ? -1 : ii->second;
}

// splits source file name to interrupt name and file name itself
std::string Settings::GetInterruptName(const std::string &file_name, std::string &real_file_name) const
{
	std::string int_name;

	// check for <path>/<int_name>:<file_name> format
	auto delpos = file_name.find_last_of("\\/");
	if(delpos != std::string::npos)
	{
		real_file_name = file_name.substr(0, delpos + 1);
		int_name = file_name.substr(delpos + 1);
		auto pos = int_name.find(':');
		if(pos != std::string::npos)
		{
			real_file_name += int_name.substr(pos + 1);
			int_name = Utils::str_toupper(int_name.substr(0, pos));
			if(GetInterruptIndex(int_name) >= 0)
			{
				return int_name;
			}
		}
	}

	// check for <int_name>:<path>/<file_name> format
	auto pos = file_name.find(':');
	if(pos == std::string::npos)
	{
		real_file_name = file_name;
		return "";
	}

	int_name = Utils::str_toupper(file_name.substr(0, pos));
	if(GetInterruptIndex(int_name) < 0)
	{
		real_file_name = file_name;
		return "";
	}

	real_file_name = file_name.substr(pos + 1);
	return int_name;
}

std::vector<std::wstring> Settings::GetDevList() const
{
	std::vector<std::wstring> devs;

	for(const auto &d: _io_settings)
	{
		devs.push_back(d.first);
	}

	return devs;
}

std::wstring Settings::GetCommonDeviceName(const std::wstring &real_dev_name) const
{
	std::wstring dev_name;
	int dev_num = 0;

	while(GetValue(L"REAL_DEVICE_NAME" + std::to_wstring(dev_num), dev_name))
	{
		if(dev_name == real_dev_name)
		{
			GetValue(L"DEVICE_NAME" + std::to_wstring(dev_num), dev_name);
			return dev_name;
		}
		dev_num++;
	}

	return std::wstring();
}

std::vector<std::wstring> Settings::GetDevCmdsList(const std::wstring &dev_name) const
{
	std::vector<std::wstring> cmds;

	auto dc = _io_settings.find(dev_name);
	if(dc != _io_settings.end())
	{
		for(const auto &c: dc->second)
		{
			cmds.push_back(c.first);
		}
	}

	return cmds;
}