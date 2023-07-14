/*
 STM8 assembler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 a1stm8.cpp: STM8 assembler
*/


#include <cstdint>
#include <cstdio>
#include <clocale>
#include <cstring>
#include <cwctype>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <memory>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <set>

#include "../../common/source/version.h"
#include "../../common/source/stm8.h"
#include "../../common/source/gitrev.h"
#include "../../common/source/Utils.h"
#include "../../common/source/moresym.h"

#include "errors.h"


#define A1STM8_MAX_INST_ARGS_NUM 2


static const char *version = B1_CMP_VERSION;


// default values: 2 kB of RAM, 16 kB of FLASH
Settings _global_settings = { 0x0, 0x0800, 0x8000, 0x4000, 0x0, 0x0 };


static void b1_print_version(FILE *fstr)
{
	std::fputs("STM8 assembler\n", fstr);
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

static std::wstring get_size_kB(int64_t size)
{
	size *= 1000;
	size /= 1024;

	auto size_int = size / 1000;
	size %= 1000;

	if (size % 10 >= 5) size = size - (size % 10) + 10;
	if (size % 100 >= 50) size = size - (size % 100) + 100;

	if (size >= 1000)
	{
		size_int++;
		size = 0;
	}
	else
	{
		size /= 100;
	}

	return std::to_wstring(size_int) + (size == 0 ? L"" : (L"." + std::to_wstring(size)));
}


class IhxWriter
{
private:
	std::string _file_name;
	std::FILE *_file;

	int32_t _max_data_len;

	uint32_t _base_addr;
	uint32_t _offset;

	int32_t _data_len;
	uint8_t _data[32];

	A1STM8_T_ERROR WriteDataRecord(int32_t first_pos, int32_t last_pos)
	{
		static const wchar_t *fmt = L":%02x%04x00%ls%02x\n";

		uint8_t chksum;
		int32_t len;

		len = last_pos - first_pos + 1;

		chksum = (uint8_t)len;
		chksum += (uint8_t)(((uint16_t)_offset) >> 8);
		chksum += (uint8_t)_offset;

		std::wostringstream str;

		for(auto i = first_pos; i <= last_pos; i++)
		{
			str << std::hex << std::setw(2) << std::setfill(L'0') << _data[i];
			chksum += _data[i];
		}

		if(len > 0)
		{
			if(std::fwprintf(_file, fmt, (int)len, (int)_offset, str.str().c_str(), (int)(uint8_t)(0 - chksum)) <= 0)
			{
				return A1STM8_T_ERROR::A1STM8_RES_EFWRITE;
			}
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR WriteExtLinearAddress(uint32_t address)
	{
		uint8_t chksum = 06;
		uint16_t addr16 = (uint16_t)((address) >> 16);

		chksum += (uint8_t)(addr16 >> 8);
		chksum += (uint8_t)addr16;
		if(std::fwprintf(_file, L":02000004%04x%02x\n", (int)addr16, (int)(uint8_t)(0 - chksum)) <= 0)
		{
			return A1STM8_T_ERROR::A1STM8_RES_EFWRITE;
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR WriteEndOfFile()
	{
		if(std::fwprintf(_file, L":00000001ff\n") <= 0)
		{
			return A1STM8_T_ERROR::A1STM8_RES_EFWRITE;
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR Flush()
	{
		int32_t first_pos, last_pos;

		if(_data_len <= 0)
		{
			return A1STM8_T_ERROR::A1STM8_RES_OK;
		}

		first_pos = 0;
		last_pos = _data_len - 1;

		if(_offset + _data_len > 0x10000)
		{
			auto write1 = 0x10000 - _offset;

			auto err = WriteDataRecord(first_pos, write1 - 1);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}

			first_pos = write1;
			_data_len -= write1;

			_base_addr += 0x10000;
			_offset = 0;
			err = WriteExtLinearAddress(_base_addr);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}
		}

		auto err = WriteDataRecord(first_pos, last_pos);
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		_offset += _data_len;
		_data_len = 0;

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

public:
	IhxWriter(const std::string &file_name)
	: _file_name(file_name)
	, _file(nullptr)
	, _base_addr(0)
	, _offset(0)
	, _max_data_len(16)
	, _data_len(0)
	, _data{ 0 }
	{
	}

	virtual ~IhxWriter()
	{
		Close();
	}

	A1STM8_T_ERROR Open()
	{
		auto err = Close();
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		_file = std::fopen(_file_name.c_str(), "w+");
		if(_file == nullptr)
		{
			return A1STM8_T_ERROR::A1STM8_RES_EFOPEN;
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR Open(const std::string &file_name)
	{
		auto err = Close();
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		_file_name = file_name;
		return Open();
	}

	A1STM8_T_ERROR Write(const void *data, int32_t size)
	{
		auto write1 = _max_data_len - _data_len;
		const uint8_t *data_ptr = static_cast<const uint8_t *>(data);

		while(size > 0)
		{
			if(size < write1)
			{
				write1 = size;
			}

			std::memcpy(_data + _data_len, data_ptr, write1);
			_data_len += write1;
			data_ptr += write1;
			size -= write1;

			if(_data_len == _max_data_len)
			{
				auto err = Flush();
				if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
				{
					return err;
				}
			}

			write1 = _max_data_len;
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR SetAddress(uint32_t address)
	{
		auto err = Flush();
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		if(address < _base_addr + _offset)
		{
			return A1STM8_T_ERROR::A1STM8_RES_EWADDR;
		}

		if((address & 0xFFFF0000) != _base_addr)
		{
			auto err = WriteExtLinearAddress(address);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}
		}

		_base_addr = address & 0xFFFF0000;
		_offset = (uint16_t)address;

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR Close()
	{
		if(_file != nullptr)
		{
			auto err = Flush();
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}

			err = WriteEndOfFile();
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}

			if(std::fclose(_file) != 0)
			{
				_file = nullptr;
				return A1STM8_T_ERROR::A1STM8_RES_EFCLOSE;
			}
		}

		_file = nullptr;
		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}
};


enum class TokType
{
	TT_INVALID,
	TT_DIR,
	TT_LABEL,
	TT_NUMBER,
	TT_QSTRING,
	TT_STRING,
	TT_OPER,
	TT_EOL,
	TT_EOF,
};

class Token
{
private:
	TokType _toktype;
	std::wstring _token;
	int _line_num;

	void MakeUpper()
	{
		if(IsDir() || IsLabel() || IsString() || _toktype == TokType::TT_NUMBER)
		{
			_token = Utils::str_toupper(_token);
		}
	}

public:
	Token()
	: _toktype(TokType::TT_INVALID)
	, _line_num(0)
	{
	}

	Token(TokType tt, const std::wstring &token, int line_num)
	: _toktype(tt)
	, _token(token)
	, _line_num(line_num)
	{
		MakeUpper();
	}

	~Token()
	{
	}

	bool IsEOF() const
	{
		return _toktype == TokType::TT_EOF;
	}

	bool IsEOL() const
	{
		return _toktype == TokType::TT_EOL;
	}

	bool IsDir() const
	{
		return _toktype == TokType::TT_DIR;
	}

	bool IsLabel() const
	{
		return _toktype == TokType::TT_LABEL;
	}

	bool IsString() const
	{
		return _toktype == TokType::TT_STRING;
	}

	bool IsNumber() const
	{
		return _toktype == TokType::TT_NUMBER;
	}

	bool operator==(const Token &token) const
	{
		return (_toktype == token._toktype) && (_token == token._token);
	}

	bool operator!=(const Token &token) const
	{
		return (_toktype != token._toktype) || (_token != token._token);
	}

	TokType GetType() const
	{
		return _toktype;
	}

	std::wstring GetToken() const
	{
		return _token;
	}

	int32_t GetLineNum() const
	{
		return _line_num;
	}
};

class SrcFile
{
private:
	std::string _file_name;
	std::FILE *_file;

	wchar_t _saved_chr;
	bool _skip_comment;
	wchar_t _nl_chr;
	int _line_num;

	A1STM8_T_ERROR ReadChar(wchar_t &chr)
	{
		A1STM8_T_ERROR err;
		wint_t c;

		err = A1STM8_T_ERROR::A1STM8_RES_OK;

		c = std::fgetwc(_file);
		if(c == WEOF)
		{
			if(std::feof(_file))
			{
				err = A1STM8_T_ERROR::A1STM8_RES_EEOF;
			}
			else
			{
				err = A1STM8_T_ERROR::A1STM8_RES_EFREAD;
			}
		}

		chr = c;

		return err;
	}

public:
	SrcFile() = delete;

	SrcFile(const std::string &file_name)
	: _file_name(file_name)
	, _file(nullptr)
	, _line_num(0)
	, _saved_chr(L'\0')
	, _skip_comment(false)
	, _nl_chr(L'\0')
	{
	}

	~SrcFile()
	{
		Close();
	}

	A1STM8_T_ERROR Open()
	{
		Close();

		_file = std::fopen(_file_name.c_str(), "rt");
		if(_file == nullptr)
		{
			return A1STM8_T_ERROR::A1STM8_RES_EFOPEN;
		}

		_line_num = 1;
		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	void Close()
	{
		if(_file != nullptr)
		{
			std::fclose(_file);
			_file = nullptr;
			_line_num = 0;
			_saved_chr = L'\0';
			_skip_comment = false;
			_nl_chr = L'\0';
		}
	}

	// tokens:
	// directive, a string starting from point (.CODE, .DATA, etc.)
	// label, a string starting from colon (:__label_1)
	// number, a string starting from digit (10, 010, 0x10)
	// quoted string ("hello", "a quote "" inside")
	// character ('a')
	// string (LD, __label_1)
	// operator: +, -, *, /, %, (, ), [, ], >>, <<, >, <, ==, !=, >=, <=, !, &, |, ^
	// end of line
	// enf of file
	A1STM8_T_ERROR GetNextToken(Token &token)
	{
		A1STM8_T_ERROR err = A1STM8_T_ERROR::A1STM8_RES_OK;
		wchar_t c;
		bool begin = true;
		bool qstr = false;
		TokType tt = TokType::TT_INVALID;
		std::wstring tok;

		while(true)
		{
			if(_saved_chr != L'\0')
			{
				c = _saved_chr;
				_saved_chr = L'\0';
			}
			else
			{
				err = ReadChar(c);
				if(err == A1STM8_T_ERROR::A1STM8_RES_EEOF)
				{
					break;
				}
				if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
				{
					return err;
				}
			}

			if(c == L'\n')
			{
				if(tt == TokType::TT_QSTRING && qstr)
				{
					return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
				}

				if(!tok.empty())
				{
					_saved_chr = c;
					break;
				}

				token = Token(TokType::TT_EOL, L"", _line_num);
				_line_num++;
				_skip_comment = false;
				return A1STM8_T_ERROR::A1STM8_RES_OK;
			}

			if(_skip_comment)
			{
				continue;
			}

			if(c == L';' && !(tt == TokType::TT_QSTRING && qstr))
			{
				_skip_comment = true;
				continue;
			}

			// skip initial spaces
			if(begin)
			{
				if(std::iswspace(c))
				{
					continue;
				}
				begin = false;
			}

			// token end
			if(std::iswspace(c))
			{
				if(tok.empty())
				{
					return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
				}

				if(!qstr)
				{
					break;
				}
			}

			// identify the token
			if(tok.empty())
			{
				switch(c)
				{
					case L'.':
						tt = TokType::TT_DIR;
						break;
					case L':':
						tt = TokType::TT_LABEL;
						break;
					case L'\"':
						tt = TokType::TT_QSTRING;
						qstr = true;
						break;
					case L'+':
					case L'-':
					case L'*':
					case L'/':
					case L'%':
					case L'(':
					case L')':
					case L'[':
					case L']':
					case L',':
					case L'>':
					case L'<':
					case L'=':
					case L'!':
					case L'&':
					case L'|':
					case L'^':
						tt = TokType::TT_OPER;
						break;
					default:
						tt = std::iswdigit(c) ? TokType::TT_NUMBER : (std::iswalpha(c) || (c == L'_')) ? TokType::TT_STRING : TokType::TT_INVALID;
				}

				if(tt == TokType::TT_INVALID)
				{
					return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
				}
			}
			else
			{
				if(tt == TokType::TT_QSTRING)
				{
					if(c == L'\"')
					{
						qstr = !qstr;
					}
					else
					if(!qstr)
					{
						return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
					}
				}
				else
				{
					if(	c == L'+' || c == L'-' || c == L'*' || c == L'/' || c == L'%' || c == L'(' || c == L')' || c == L'[' || c == L']' ||
						c == L',' || c == L'>' || c == L'<' || c == L'=' || c == L'!' || c == L'&' || c == L'|' || c == L'^')
					{
						_saved_chr = c;
						break;
					}
				}
			}

			tok += c;

			if(tt == TokType::TT_OPER)
			{
				// read shift, NOT and comparison operators
				if(c == L'>' || c == L'<' || c == L'=' || c == L'!')
				{
					wchar_t c1;

					err = ReadChar(c1);
					if(err == A1STM8_T_ERROR::A1STM8_RES_EEOF)
					{
						return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
					}
					if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
					{
						return err;
					}

					// NOT and not equal operators
					if(c == L'!')
					{
						if(c1 == L'=')
						{
							tok += c1;
						}
						else
						{
							_saved_chr = c1;
						}
					}
					else
					if(c == L'=')
					{
						if(c1 != L'=')
						{
							return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
						}

						tok += c1;
					}
					else
					if(c == L'>' || c == L'<')
					{
						if(c == c1 || c1 == L'=')
						{
							tok += c1;
						}
						else
						{
							_saved_chr = c1;
						}
					}
				}

				break;
			}
		}

		if(err == A1STM8_T_ERROR::A1STM8_RES_EEOF)
		{
			err = A1STM8_T_ERROR::A1STM8_RES_OK;
			if(tok.empty())
			{
				token = Token(TokType::TT_EOF, L"", _line_num);
				return A1STM8_T_ERROR::A1STM8_RES_OK;
			}
		}

		token = Token(tt, tok, _line_num);

		return err;
	}

	int GetLineNum()
	{
		return _line_num;
	}
};

enum class SectType
{
	ST_NONE,
	ST_PAGE0,
	ST_DATA,
	ST_CONST,
	ST_CODE,
	ST_INIT,
	ST_STACK,
	ST_HEAP,
};

class GenStmt;

class Section : public std::list<GenStmt *>
{
private:
	int32_t _sect_line_num;
	mutable int32_t _curr_line_num;
	std::string _file_name;

	SectType _type;
	int32_t _address;

public:
	Section() = delete;

	Section(const Section &sect) = delete;

	Section(Section &&sect);

	Section(const std::string &file_name, int32_t sect_line_num, SectType st, int32_t address = -1);

	virtual ~Section();

	Section &operator=(const Section &) = delete;

	Section &operator=(Section &&) = delete;

	SectType GetType() const;

	int32_t GetAddress() const;

	void SetAddress(int32_t address);

	A1STM8_T_ERROR GetSize(int32_t &size) const;

	int32_t GetSectLineNum() const
	{
		return _sect_line_num;
	}

	int32_t GetCurrLineNum() const
	{
		return _curr_line_num;
	}

	std::string GetFileName() const
	{
		return _file_name;
	}
};

class MemRef
{
private:
	std::wstring _name;
	int32_t _address;
	int _line_num;

public:
	MemRef()
	: _address(-1)
	, _line_num(-1)
	{
	}

	A1STM8_T_ERROR Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end)
	{
		if(start == end)
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		if(start->GetType() != TokType::TT_LABEL)
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		auto tok = start->GetToken();
		auto line_num = start->GetLineNum();

		start++;
		if(start != end && start->GetType() != TokType::TT_EOL && start->GetType() != TokType::TT_EOF)
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		_name.clear();
		bool init = true;

		for(auto c: tok)
		{
			if(c == L':')
			{
				if(init)
				{
					init = false;
					continue;
				}
			}

			_name += c;
		}

		if(_name.empty())
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		_line_num = line_num;

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	int32_t GetAddress() const
	{
		return _address;
	}

	void SetAddress(int32_t address)
	{
		_address = address;
	}

	std::wstring GetName() const
	{
		return _name;
	}

	void SetName(const std::wstring &name)
	{
		_name = name;
	}
};

class GenStmt
{
protected:
	int _line_num;
	std::vector<A1STM8_T_WARNING> _warnings;

	int32_t _size;
	int32_t _address;

public:
	GenStmt()
	: _size(-1)
	, _address(-1)
	, _line_num(-1)
	{
	}

	virtual ~GenStmt()
	{
	}

	virtual A1STM8_T_ERROR Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, const std::map<std::wstring, MemRef> &memrefs, const std::string &file_name) = 0;

	virtual A1STM8_T_ERROR Write(IhxWriter *writer, const std::map<std::wstring, MemRef> &memrefs) = 0;

	int32_t GetSize() const
	{
		return _size;
	}

	int32_t GetAddress() const
	{
		return _address;
	}

	void SetAddress(int32_t address)
	{
		_address = address;
	}

	int GetLineNum() const
	{
		return _line_num;
	}

	const std::vector<A1STM8_T_WARNING> &GetWarnings() const
	{
		return _warnings;
	}
};

Section::Section(const std::string &file_name, int32_t sect_line_num, SectType st, int32_t address /*= -1*/)
: _type(st)
, _address(address)
, _sect_line_num(sect_line_num)
, _curr_line_num(0)
, _file_name(file_name)
{
}

Section::Section(Section &&sect)
: std::list<GenStmt *>(std::move(sect))
, _type(std::move(sect._type))
, _address(std::move(sect._address))
, _sect_line_num(sect._sect_line_num)
, _curr_line_num(std::move(sect._curr_line_num))
, _file_name(std::move(sect._file_name))
{
}

Section::~Section()
{
	for(auto &i: *this)
	{
		delete i;
	}

	clear();
}

SectType Section::GetType() const
{
	return _type;
}

int32_t Section::GetAddress() const
{
	return _address;
}

void Section::SetAddress(int32_t address)
{
	_address = address;
}

A1STM8_T_ERROR Section::GetSize(int32_t &size) const
{
	int32_t osize, size1;

	_curr_line_num = 0;

	osize = 0;
	for(auto &i: *this)
	{
		_curr_line_num = i->GetLineNum();

		size1 = i->GetSize();
		if(size1 <= 0)
		{
			return A1STM8_T_ERROR::A1STM8_RES_EWSTMTSIZE;
		}

		osize += size1;
	}

	size = osize;

	_curr_line_num = 0;
	return A1STM8_T_ERROR::A1STM8_RES_OK;
}

class EVal
{
public:
	enum class USGN
	{
		US_NONE,
		US_MINUS,
		US_NOT,
	};


protected:
	USGN _usgn;
	bool _resolved;
	int32_t _val;
	std::wstring _symbol;

public:
	EVal() = delete;

	EVal(int32_t val)
	: _usgn(USGN::US_NONE)
	, _resolved(true)
	, _val(val)
	, _symbol(L"<no symbol>")
	{
	}

	EVal(const std::wstring &name, USGN usgn)
	: _usgn(usgn)
	, _resolved(false)
	, _val(-1)
	, _symbol(name)
	{
	}

	EVal(int32_t val, const std::wstring &name, USGN usgn)
	: _usgn(usgn)
	, _resolved(true)
	, _val(val)
	, _symbol(name)
	{
	}

	bool IsResolved() const
	{
		return _resolved;
	}

	static int32_t ConvertValue(int32_t val, USGN usgn, B1Types type)
	{
		val = (usgn == EVal::USGN::US_MINUS) ? -val : (usgn == EVal::USGN::US_NOT) ? ~val : val;

		switch (type)
		{
			case B1Types::B1T_BYTE:
				val &= 0xFF;
				break;
			case B1Types::B1T_INT:
			case B1Types::B1T_WORD:
				val &= 0xFFFF;
				break;
		}

		return val;
	}

	A1STM8_T_ERROR Resolve(const std::map<std::wstring, MemRef> &symbols)
	{
		wchar_t c = L'\0', c1 = L'\0';

		auto s = GetSymbol();
		if(s.size() > 2 && s[s.size() - 2] == L'.')
		{
			c = s[s.size() - 1];
			s.erase(s.size() - 2);
		}
		else
		if(s.size() > 3 && s[s.size() - 3] == L'.')
		{
			c = s[s.size() - 2];
			c1 = s[s.size() - 1];
			s.erase(s.size() - 3);
		}

		const auto &ref = symbols.find(s);
		if(ref == symbols.end())
		{
			return A1STM8_T_ERROR::A1STM8_RES_EUNRESSYMB;
		}

		B1Types type = B1Types::B1T_LONG;
		int32_t val = ref->second.GetAddress();

		if(c != L'\0')
		{
			type = B1Types::B1T_BYTE;

			if(c == L'l' || c == L'L')
			{
				val = (uint16_t)val;
			}
			else
			if(c == L'h' || c == L'H')
			{
				val = (uint16_t)(val >> 16);
			}
			else
			{
				return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
			}

			if(c1 != L'\0')
			{
				type = B1Types::B1T_WORD;

				if(c1 == L'l' || c1 == L'L')
				{
					val = (uint8_t)val;
				}
				else
				if(c1 == L'h' || c1 == L'H')
				{
					val = (uint8_t)(val >> 8);
				}
				else
				{
					return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
				}
			}
		}

		_val = ConvertValue(val, _usgn, type);

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	int32_t GetValue() const
	{
		return _val;
	}

	std::wstring GetSymbol() const
	{
		return _symbol;
	}

	bool operator==(const std::wstring &str) const
	{
		return operator std::wstring() == str;
	}

	operator std::wstring() const
	{
		return (_usgn == USGN::US_MINUS) ? (L"-" + _symbol) : (_usgn == USGN::US_NOT) ? (L"!" + _symbol) : _symbol;
	}
};

class Exp
{
protected:
	std::vector<std::wstring> _ops;
	std::vector<EVal> _vals;

public:
	static A1STM8_T_ERROR BuildExp(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, Exp &exp, const std::vector<Token> &terms = std::vector<Token>())
	{
		bool is_val = true;
		EVal::USGN usgn = EVal::USGN::US_NONE;

		for(; start != end; start++)
		{
			if(std::find(terms.begin(), terms.end(), *start) != terms.end())
			{
				break;
			}

			if(is_val)
			{
				usgn = EVal::USGN::US_NONE;

				if(start->GetType() == TokType::TT_OPER)
				{
					if(start->GetToken() == L"-")
					{
						usgn = EVal::USGN::US_MINUS;
					}
					else
					if(start->GetToken() == L"!")
					{
						usgn = EVal::USGN::US_NOT;
					}
					else
					{
						return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
					}

					start++;
				}

				if(start != end && (start->GetType() == TokType::TT_NUMBER || start->GetType() == TokType::TT_STRING))
				{
					int32_t n = 0;

					if(start->GetType() == TokType::TT_NUMBER)
					{
						auto tok = start->GetToken();
						
						B1Types type = B1Types::B1T_UNKNOWN;
						auto err = Utils::str2int32(tok, n, &type);
						if(err != B1_RES_OK)
						{
							return static_cast<A1STM8_T_ERROR>(err);
						}

						if(usgn == EVal::USGN::US_MINUS && n == INT32_MIN)
						{
							return A1STM8_T_ERROR::A1STM8_RES_ENUMOVF;
						}

						n = EVal::ConvertValue(n, usgn, type);
						exp.AddVal(EVal(n, tok, usgn));
					}
					else
					{
						// resolve global constants
						auto tok = start->GetToken();
						std::wstring value;

						if(_global_settings.GetValue(tok, value))
						{
							B1Types type = B1Types::B1T_UNKNOWN;
							auto err = Utils::str2int32(value, n, &type);
							if(err != B1_RES_OK)
							{
								exp.AddVal(EVal(value, usgn));
							}
							else
							{
								if(usgn == EVal::USGN::US_MINUS && n == INT32_MIN)
								{
									return A1STM8_T_ERROR::A1STM8_RES_ENUMOVF;
								}

								n = EVal::ConvertValue(n, usgn, type);
								exp.AddVal(EVal(n, value, usgn));
							}
						}
						else
						if(_RTE_errors.find(tok) != _RTE_errors.end())
						{
							n = static_cast<std::underlying_type_t<B1C_T_RTERROR>>(_RTE_errors[tok]);
							n = EVal::ConvertValue(n, usgn, RTE_ERROR_TYPE);
							exp.AddVal(EVal(n, value, usgn));
						}
						else
						if(_B1C_consts.find(tok) != _B1C_consts.end())
						{
							const auto &c = _B1C_consts[tok];
							n = c.first;
							n = EVal::ConvertValue(n, usgn, c.second);
							exp.AddVal(EVal(n, value, usgn));
						}
						else
						{
							exp.AddVal(EVal(tok, usgn));
						}
					}
				}
				else
				{
					return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
				}

				is_val = false;
			}
			else
			{
				if(start->GetType() != TokType::TT_OPER)
				{
					return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
				}

				auto tok = start->GetToken();

				if(tok != L"+" && tok != L"-" && tok != L"*" && tok != L"/" && tok != L"%" && tok != L">>" && tok != L"<<" && tok != L"&" && tok != L"^" && tok != L"|")
				{
					return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
				}

				exp.AddOp(tok);

				is_val = true;
			}
		}

		if(exp._vals.size() - 1 != exp._ops.size())
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	static A1STM8_T_ERROR CalcSimpleExp(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, int32_t &res, const std::vector<Token> &terms = std::vector<Token>())
	{
		Exp exp;

		auto err = BuildExp(start, end, exp, terms);
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		err = exp.Eval(res);

		return err;
	}

	void AddVal(const EVal &val)
	{
		_vals.push_back(val);
	}

	void AddOp(const std::wstring &op)
	{
		_ops.push_back(op);
	}

	A1STM8_T_ERROR Eval(int32_t &res, const std::map<std::wstring, MemRef> &symbols = std::map<std::wstring, MemRef>()) const
	{
		if(_vals.size() - 1 != _ops.size())
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		std::vector<std::wstring> ops = _ops;
		std::vector<EVal> vals = _vals;

		// resolve symbols
		for(auto &v: vals)
		{
			if(!v.IsResolved())
			{
				auto err = v.Resolve(symbols);
				if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
				{
					return err;
				}
			}
		}

		// multiplicative operations
		bool stop = false;
		while(!stop && ops.size() > 0)
		{
			stop = true;
			for(auto o = ops.begin(); o != ops.end(); o++)
			{
				if(*o == L"*" || *o == L"/" || *o == L"%")
				{
					auto i = o - ops.begin();

					switch((*o)[0])
					{
						case L'*':
							vals[i] = vals[i].GetValue() * vals[i + 1].GetValue();
							break;
						case L'/':
							vals[i] = vals[i].GetValue() / vals[i + 1].GetValue();
							break;
						case L'%':
							vals[i] = vals[i].GetValue() % vals[i + 1].GetValue();
							break;
					}

					vals.erase(vals.begin() + i + 1);
					ops.erase(o);

					stop = false;
					break;
				}
			}
		}

		// additive operations
		stop = false;
		while(!stop && ops.size() > 0)
		{
			stop = true;
			for(auto o = ops.begin(); o != ops.end(); o++)
			{
				if(*o == L"+" || *o == L"-")
				{
					auto i = o - ops.begin();

					switch ((*o)[0])
					{
						case L'+':
							vals[i] = vals[i].GetValue() + vals[i + 1].GetValue();
							break;
						case L'-':
							vals[i] = vals[i].GetValue() - vals[i + 1].GetValue();
							break;
					}

					vals.erase(vals.begin() + i + 1);
					ops.erase(o);

					stop = false;
					break;
				}
			}
		}

		// shift operations
		stop = false;
		while(!stop && ops.size() > 0)
		{
			stop = true;
			for(auto o = ops.begin(); o != ops.end(); o++)
			{
				if(*o == L">>" || *o == L"<<")
				{
					auto i = o - ops.begin();

					switch((*o)[0])
					{
						case L'>':
							vals[i] = vals[i].GetValue() >> vals[i + 1].GetValue();
							break;
						case L'<':
							vals[i] = vals[i].GetValue() << vals[i + 1].GetValue();
							break;
					}

					vals.erase(vals.begin() + i + 1);
					ops.erase(o);

					stop = false;
					break;
				}
			}
		}

		// bitwise AND
		stop = false;
		while(!stop && ops.size() > 0)
		{
			stop = true;
			for(auto o = ops.begin(); o != ops.end(); o++)
			{
				if(*o == L"&")
				{
					auto i = o - ops.begin();
					vals[i] = vals[i].GetValue() & vals[i + 1].GetValue();
					vals.erase(vals.begin() + i + 1);
					ops.erase(o);

					stop = false;
					break;
				}
			}
		}

		// bitwise XOR
		stop = false;
		while(!stop && ops.size() > 0)
		{
			stop = true;
			for(auto o = ops.begin(); o != ops.end(); o++)
			{
				if(*o == L"^")
				{
					auto i = o - ops.begin();
					vals[i] = vals[i].GetValue() ^ vals[i + 1].GetValue();
					vals.erase(vals.begin() + i + 1);
					ops.erase(o);

					stop = false;
					break;
				}
			}
		}

		// bitwise OR
		stop = false;
		while(!stop && ops.size() > 0)
		{
			stop = true;
			for(auto o = ops.begin(); o != ops.end(); o++)
			{
				if(*o == L"|")
				{
					auto i = o - ops.begin();
					vals[i] = vals[i].GetValue() | vals[i + 1].GetValue();
					vals.erase(vals.begin() + i + 1);
					ops.erase(o);

					stop = false;
					break;
				}
			}
		}

		if(ops.size() != 0)
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		res = vals[0].GetValue();

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	bool operator==(const std::wstring &str) const
	{
		return operator std::wstring() == str;
	}

	operator std::wstring() const
	{
		return (_ops.empty() && _vals.size() == 1) ? (std::wstring)_vals[0] : L"";
	}
};

class DataStmt: public GenStmt
{
protected:
	int32_t _size1;
	bool _size_specified;

public:
	DataStmt()
	: GenStmt()
	, _size1(-1)
	, _size_specified(false)
	{
	}

	virtual ~DataStmt()
	{
	}

	A1STM8_T_ERROR Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, const std::map<std::wstring, MemRef> &memrefs, const std::string &file_name) override
	{
		if(start == end)
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		if(start->GetType() != TokType::TT_STRING)
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		auto tok = start->GetToken();
		int32_t size1;

		if(tok == L"DB")
		{
			size1 = 1;
		}
		else
		if(tok == L"DW")
		{
			size1 = 2;
		}
		else
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		_line_num = start->GetLineNum();

		_size_specified = false;

		start++;
		
		if(start != end && start->GetType() == TokType::TT_OPER && start->GetToken() == L"(")
		{
			start++;

			int32_t rep = -1;
			auto err = Exp::CalcSimpleExp(start, end, rep, { Token(TokType::TT_OPER, L")", -1) });
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}
			if(start == end)
			{
				return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
			}
			if(rep <= 0)
			{
				return A1STM8_T_ERROR::A1STM8_RES_EWBLKSIZE;
			}

			_size1 = size1;
			_size = size1 * rep;
			_size_specified = true;

			start++;
		}
		else
		{
			_size1 = size1;
			_size = size1;
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR Write(IhxWriter *writer, const std::map<std::wstring, MemRef> &memrefs) override
	{
		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}
};

class Page0Stmt: public DataStmt
{
public:
	Page0Stmt()
	: DataStmt()
	{
	}

	virtual ~Page0Stmt()
	{
	}
};

class HeapStmt: public DataStmt
{
public:
	HeapStmt()
	: DataStmt()
	{
	}

	virtual ~HeapStmt()
	{
	}
};

class StackStmt: public DataStmt
{
public:
	StackStmt()
	: DataStmt()
	{
	}

	virtual ~StackStmt()
	{
	}
};

class ConstStmt: public DataStmt
{
protected:
	std::vector<uint8_t> _data;
	std::vector<std::pair<int32_t, Exp>> _exps;
	bool _truncated;

public:
	ConstStmt()
	: DataStmt()
	, _truncated(false)
	{
	}

	virtual ~ConstStmt()
	{
	}

	A1STM8_T_ERROR Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, const std::map<std::wstring, MemRef> &memrefs, const std::string &file_name) override
	{
		auto err = DataStmt::Read(start, end, memrefs, file_name);
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		std::vector<Token> terms;

		terms.push_back(Token(TokType::TT_OPER, L",", -1));
		terms.push_back(Token(TokType::TT_EOL, L"", -1));
		terms.push_back(Token(TokType::TT_EOF, L"", -1));

		// read data
		while(start != end && !start->IsEOL() && !start->IsEOF())
		{
			if(start->GetType() == TokType::TT_QSTRING)
			{
				// put string
				auto tok = start->GetToken();

				for(auto ci = tok.begin() + 1; ci != tok.end() - 1; ci++)
				{
					auto c = *ci;

					if(c == L'\"')
					{
						ci++;
					}
					else
					if(c == L'\\')
					{
						ci++;
						c = *ci;

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
							return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
						}
					}

					if(_size1 == 2)
					{
						_data.push_back(((uint16_t)c) >> 8);
					}

					_data.push_back((uint8_t)c);
				}

				start++;
			}
			else
			{
				int32_t num = 0;
				Exp exp;

				auto err = Exp::BuildExp(start, end, exp, terms);
				if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
				{
					return err;
				}

				err = exp.Eval(num);
				if(err != A1STM8_T_ERROR::A1STM8_RES_OK && err != A1STM8_T_ERROR::A1STM8_RES_EUNRESSYMB)
				{
					return err;
				}

				if(err == A1STM8_T_ERROR::A1STM8_RES_EUNRESSYMB)
				{
					// save expression to evaluate later
					_exps.push_back({ (int32_t)_data.size(), exp });
				}

				if(_size1 == 2)
				{
					_data.push_back(((uint16_t)num) >> 8);
				}

				_data.push_back((uint8_t)num);
			}

			if(start != end && *start == Token(TokType::TT_OPER, L",", -1))
			{
				start++;
				continue;
			}
		}

		if(_size_specified)
		{
			if(_size < _data.size())
			{
				// truncate data
				_truncated = true;
			}
			else
			{
				_data.resize(_size, 0);
			}
		}
		else
		{
			if(_size <= _data.size())
			{
				_size = _data.size();
			}
			else
			{
				_data.resize(_size, 0);
			}
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR Write(IhxWriter *writer, const std::map<std::wstring, MemRef> &memrefs) override
	{
		// evaluate expressions if any
		for(auto &exp: _exps)
		{
			int32_t val = 0;
			auto err = exp.second.Eval(val, memrefs);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}

			auto i = exp.first;

			if(_size1 == 2)
			{
				_data[i++] = (((uint16_t)val) >> 8);
			}

			_data[i] = ((uint8_t)val);
		}

		if(_truncated)
		{
			// data truncated
			_warnings.push_back(A1STM8_T_WARNING::A1STM8_WRN_WDATATRUNC);
		}

		auto err = writer->Write(_data.data(), _size);
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}
};

enum class ArgType
{
	AT_NONE,
	AT_1BYTE_ADDR, // 0..FF
	AT_2BYTE_ADDR, // 0..FFFF
	AT_3BYTE_ADDR, // 0..FFFFFF
	AT_1BYTE_OFF, // -128..+127 (offset for JRx, CALLR instructions)
	AT_1BYTE_VAL, // -128..255
	AT_2BYTE_VAL, // -32768..65535
};

class Inst
{
public:
	// instruction code size
	int _size;
	// code
	const unsigned char *_code;
	// arguments count
	int _argnum;
	// argument sizes
	ArgType _argtypes[A1STM8_MAX_INST_ARGS_NUM];
	// reversed argument order
	bool _revorder;

	Inst(int size, const unsigned char *code, ArgType arg1type = ArgType::AT_NONE, ArgType arg2type = ArgType::AT_NONE, bool revorder = false)
	: _size(size)
	, _code(code)
	, _argnum(0)
	, _argtypes { arg1type, arg2type }
	, _revorder(revorder)
	{
		for(_argnum = 0; _argnum < A1STM8_MAX_INST_ARGS_NUM; _argnum++)
		{
			if(_argtypes[_argnum] == ArgType::AT_NONE)
			{
				break;
			}
		}
	}
};


static std::multimap<std::wstring, Inst> _instructions;

#define ADD_INST(SIGN, OPCODE, ...) _instructions.emplace(SIGN, Inst((sizeof(OPCODE) / sizeof(OPCODE[0])) - 1, (const unsigned char *)(OPCODE), ##__VA_ARGS__))

static void load_all_instructions()
{
	// ADC
	ADD_INST(L"ADCA,V",			"\xA9", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"ADCA,(V)",		"\xB9", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADCA,(V)",		"\xC9", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADCA,(X)",		"\xF9");
	ADD_INST(L"ADCA,(V,X)",		"\xE9", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADCA,(V,X)",		"\xD9", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADCA,(Y)",		"\x90\xF9");
	ADD_INST(L"ADCA,(V,Y)",		"\x90\xE9", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADCA,(V,Y)",		"\x90\xD9", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADCA,(V,SP)",	"\x19", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADCA,[V]",		"\x92\xC9", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADCA,[V]",		"\x72\xC9", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADCA,([V],X)",	"\x92\xD9", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADCA,([V],X)",	"\x72\xD9", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADCA,([V],Y)",	"\x91\xD9", ArgType::AT_1BYTE_ADDR);

	// ADD
	ADD_INST(L"ADDA,V",			"\xAB", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"ADDA,(V)",		"\xBB", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDA,(V)",		"\xCB", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDA,(X)",		"\xFB");
	ADD_INST(L"ADDA,(V,X)",		"\xEB", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDA,(V,X)",		"\xDB", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDA,(Y)",		"\x90\xFB");
	ADD_INST(L"ADDA,(V,Y)",		"\x90\xEB", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDA,(V,Y)",		"\x90\xDB", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDA,(V,SP)",	"\x1B", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDA,[V]",		"\x92\xCB", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDA,[V]",		"\x72\xCB", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDA,([V],X)",	"\x92\xDB", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDA,([V],X)",	"\x72\xDB", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDA,([V],Y)",	"\x91\xDB", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDSP,V",		"\x5B", ArgType::AT_1BYTE_ADDR);

	// ADDW
	ADD_INST(L"ADDWX,V",		"\x1C", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"ADDWX,(V)",		"\x72\xBB", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDWX,(V,SP)",	"\x72\xFB", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDWY,V",		"\x72\xA9", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"ADDWY,(V)",		"\x72\xB9", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDWY,(V,SP)",	"\x72\xF9", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDWSP,V",		"\x5B", ArgType::AT_1BYTE_ADDR);

	// AND
	ADD_INST(L"ANDA,V",			"\xA4", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"ANDA,(V)",		"\xB4", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ANDA,(V)",		"\xC4", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ANDA,(X)",		"\xF4");
	ADD_INST(L"ANDA,(V,X)",		"\xE4", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ANDA,(V,X)",		"\xD4", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ANDA,(Y)",		"\x90\xF4");
	ADD_INST(L"ANDA,(V,Y)",		"\x90\xE4", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ANDA,(V,Y)",		"\x90\xD4", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ANDA,(V,SP)",	"\x14", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ANDA,[V]",		"\x92\xC4", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ANDA,[V]",		"\x72\xC4", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ANDA,([V],X)",	"\x92\xD4", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ANDA,([V],X)",	"\x72\xD4", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ANDA,([V],Y)",	"\x91\xD4", ArgType::AT_1BYTE_ADDR);

	// BCCM
	// \x90\x1n, where n = 1 + 2 * pos
	ADD_INST(L"BCCM(V),0",		"\x90\x11", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),1",		"\x90\x13", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),2",		"\x90\x15", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),3",		"\x90\x17", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),4",		"\x90\x19", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),5",		"\x90\x1B", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),6",		"\x90\x1D", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),7",		"\x90\x1F", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0x0",	"\x90\x11", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0x1",	"\x90\x13", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0x2",	"\x90\x15", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0x3",	"\x90\x17", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0x4",	"\x90\x19", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0x5",	"\x90\x1B", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0x6",	"\x90\x1D", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0x7",	"\x90\x1F", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0X0",	"\x90\x11", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0X1",	"\x90\x13", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0X2",	"\x90\x15", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0X3",	"\x90\x17", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0X4",	"\x90\x19", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0X5",	"\x90\x1B", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0X6",	"\x90\x1D", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCCM(V),0X7",	"\x90\x1F", ArgType::AT_2BYTE_ADDR);

	// BCP
	ADD_INST(L"BCPA,V",			"\xA5", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"BCPA,(V)",		"\xB5", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"BCPA,(V)",		"\xC5", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPA,(X)",		"\xF5");
	ADD_INST(L"BCPA,(V,X)",		"\xE5", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"BCPA,(V,X)",		"\xD5", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPA,(Y)",		"\x90\xF5");
	ADD_INST(L"BCPA,(V,Y)",		"\x90\xE5", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"BCPA,(V,Y)",		"\x90\xD5", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPA,(V,SP)",	"\x15", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"BCPA,[V]",		"\x92\xC5", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"BCPA,[V]",		"\x72\xC5", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPA,([V],X)",	"\x92\xD5", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"BCPA,([V],X)",	"\x72\xD5", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPA,([V],Y)",	"\x91\xD5", ArgType::AT_1BYTE_ADDR);

	// BCPL
	// \x90\x1n, where n = 2 * pos
	ADD_INST(L"BCPL(V),0",		"\x90\x10", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),1",		"\x90\x12", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),2",		"\x90\x14", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),3",		"\x90\x16", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),4",		"\x90\x18", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),5",		"\x90\x1A", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),6",		"\x90\x1C", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),7",		"\x90\x1E", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0x0",	"\x90\x10", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0x1",	"\x90\x12", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0x2",	"\x90\x14", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0x3",	"\x90\x16", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0x4",	"\x90\x18", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0x5",	"\x90\x1A", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0x6",	"\x90\x1C", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0x7",	"\x90\x1E", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0X0",	"\x90\x10", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0X1",	"\x90\x12", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0X2",	"\x90\x14", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0X3",	"\x90\x16", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0X4",	"\x90\x18", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0X5",	"\x90\x1A", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0X6",	"\x90\x1C", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPL(V),0X7",	"\x90\x1E", ArgType::AT_2BYTE_ADDR);

	// BREAK
	ADD_INST(L"BREAK", "\x8B");

	// BRES
	// \x72\x1n, where n = 1 + 2 * pos
	ADD_INST(L"BRES(V),0",		"\x72\x11", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),1",		"\x72\x13", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),2",		"\x72\x15", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),3",		"\x72\x17", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),4",		"\x72\x19", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),5",		"\x72\x1B", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),6",		"\x72\x1D", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),7",		"\x72\x1F", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0x0",	"\x72\x11", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0x1",	"\x72\x13", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0x2",	"\x72\x15", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0x3",	"\x72\x17", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0x4",	"\x72\x19", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0x5",	"\x72\x1B", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0x6",	"\x72\x1D", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0x7",	"\x72\x1F", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0X0",	"\x72\x11", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0X1",	"\x72\x13", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0X2",	"\x72\x15", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0X3",	"\x72\x17", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0X4",	"\x72\x19", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0X5",	"\x72\x1B", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0X6",	"\x72\x1D", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BRES(V),0X7",	"\x72\x1F", ArgType::AT_2BYTE_ADDR);

	// BSET
	// \x72\x1n, where n = 2 * pos
	ADD_INST(L"BSET(V),0",		"\x72\x10", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),1",		"\x72\x12", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),2",		"\x72\x14", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),3",		"\x72\x16", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),4",		"\x72\x18", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),5",		"\x72\x1A", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),6",		"\x72\x1C", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),7",		"\x72\x1E", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0x0",	"\x72\x10", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0x1",	"\x72\x12", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0x2",	"\x72\x14", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0x3",	"\x72\x16", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0x4",	"\x72\x18", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0x5",	"\x72\x1A", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0x6",	"\x72\x1C", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0x7",	"\x72\x1E", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0X0",	"\x72\x10", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0X1",	"\x72\x12", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0X2",	"\x72\x14", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0X3",	"\x72\x16", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0X4",	"\x72\x18", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0X5",	"\x72\x1A", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0X6",	"\x72\x1C", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BSET(V),0X7",	"\x72\x1E", ArgType::AT_2BYTE_ADDR);

	// BTJF
	// \x72\x0n, where n = 1 + 2 * pos
	ADD_INST(L"BTJF(V),0,V",	"\x72\x01", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),1,V",	"\x72\x03", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),2,V",	"\x72\x05", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),3,V",	"\x72\x07", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),4,V",	"\x72\x09", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),5,V",	"\x72\x0B", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),6,V",	"\x72\x0D", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),7,V",	"\x72\x0F", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0x0,V",	"\x72\x01", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0x1,V",	"\x72\x03", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0x2,V",	"\x72\x05", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0x3,V",	"\x72\x07", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0x4,V",	"\x72\x09", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0x5,V",	"\x72\x0B", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0x6,V",	"\x72\x0D", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0x7,V",	"\x72\x0F", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0X0,V",	"\x72\x01", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0X1,V",	"\x72\x03", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0X2,V",	"\x72\x05", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0X3,V",	"\x72\x07", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0X4,V",	"\x72\x09", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0X5,V",	"\x72\x0B", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0X6,V",	"\x72\x0D", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJF(V),0X7,V",	"\x72\x0F", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);

	// BTJT
	// \x72\x0n, where n = 2 * pos
	ADD_INST(L"BTJT(V),0,V",	"\x72\x00", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),1,V",	"\x72\x02", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),2,V",	"\x72\x04", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),3,V",	"\x72\x06", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),4,V",	"\x72\x08", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),5,V",	"\x72\x0A", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),6,V",	"\x72\x0C", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),7,V",	"\x72\x0E", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0x0,V",	"\x72\x00", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0x1,V",	"\x72\x02", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0x2,V",	"\x72\x04", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0x3,V",	"\x72\x06", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0x4,V",	"\x72\x08", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0x5,V",	"\x72\x0A", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0x6,V",	"\x72\x0C", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0x7,V",	"\x72\x0E", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0X0,V",	"\x72\x00", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0X1,V",	"\x72\x02", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0X2,V",	"\x72\x04", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0X3,V",	"\x72\x06", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0X4,V",	"\x72\x08", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0X5,V",	"\x72\x0A", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0X6,V",	"\x72\x0C", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);
	ADD_INST(L"BTJT(V),0X7,V",	"\x72\x0E", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_OFF);

	// CALL
	ADD_INST(L"CALLV",			"\xCD", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CALL(V)",		"\xCD", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CALL(X)",		"\xFD");
	ADD_INST(L"CALL(V,X)",		"\xED", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CALL(V,X)",		"\xDD", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CALL(Y)",		"\x90\xFD");
	ADD_INST(L"CALL(V,Y)",		"\x90\xED", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CALL(V,Y)",		"\x90\xDD", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CALL[V]",		"\x92\xCD", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CALL[V]",		"\x72\xCD", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CALL([V],X)",	"\x92\xDD", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CALL([V],X)",	"\x72\xDD", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CALL([V],Y)",	"\x91\xDD", ArgType::AT_1BYTE_ADDR);

	// CALLF
	ADD_INST(L"CALLFV",			"\x8D", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"CALLF(V)",		"\x8D", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"CALLF[V]",		"\x92\x8D", ArgType::AT_2BYTE_ADDR);

	// CALLR
	ADD_INST(L"CALLRV",			"\xAD", ArgType::AT_1BYTE_OFF);

	// CCF
	ADD_INST(L"CCF",			"\x8C");

	// CLR
	ADD_INST(L"CLRA",			"\x4F");
	ADD_INST(L"CLR(V)",			"\x3F", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CLR(V)",			"\x72\x5F", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CLR(X)",			"\x7F");
	ADD_INST(L"CLR(V,X)",		"\x6F", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CLR(V,X)",		"\x72\x4F", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CLR(Y)",			"\x90\x7F");
	ADD_INST(L"CLR(V,Y)",		"\x90\x6F", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CLR(V,Y)",		"\x90\x4F", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CLR(V,SP)",		"\x0F", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CLR[V]",			"\x92\x3F", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CLR[V]",			"\x72\x3F", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CLR([V],X)",		"\x92\x6F", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CLR([V],X]",		"\x72\x6F", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CLR([V],Y)",		"\x91\x6F", ArgType::AT_1BYTE_ADDR);

	// CLRW
	ADD_INST(L"CLRWX",			"\x5F");
	ADD_INST(L"CLRWY",			"\x90\x5F");

	// CP
	ADD_INST(L"CPA,V",			"\xA1", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"CPA,(V)",		"\xB1", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPA,(V)",		"\xC1", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPA,(X)",		"\xF1");
	ADD_INST(L"CPA,(V,X)",		"\xE1", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPA,(V,X)",		"\xD1", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPA,(Y)",		"\x90\xF1");
	ADD_INST(L"CPA,(V,Y)",		"\x90\xE1", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPA,(V,Y)",		"\x90\xD1", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPA,(V,SP)",		"\x11", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPA,[V]",		"\x92\xC1", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPA,[V]",		"\x72\xC1", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPA,([V],X)",	"\x92\xD1", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPA,([V],X)",	"\x72\xD1", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPA,([V],Y)",	"\x91\xD1", ArgType::AT_1BYTE_ADDR);

	// CPW
	ADD_INST(L"CPWX,V",			"\xA3", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"CPWX,(V)",		"\xB3", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWX,(V)",		"\xC3", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPWX,(Y)",		"\x90\xF3");
	ADD_INST(L"CPWX,(V,Y)",		"\x90\xE3", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWX,(V,Y)",		"\x90\xD3", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPWX,(V,SP)",	"\x13", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWX,[V]",		"\x92\xC3", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWX,[V]",		"\x72\xC3", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPWX,([V],Y)",	"\x91\xD3", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWY,V",			"\x90\xA3", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPWY,(V)",		"\x90\xB3", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWY,(V)",		"\x90\xC3", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPWY,(X)",		"\xF3");
	ADD_INST(L"CPWY,(V,X)",		"\xE3", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWY,(V,X)",		"\xD3", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPWY,[V]",		"\x91\xC3", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWY,[V],X",		"\x92\xD3", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWY,[V],X",		"\x72\xD3", ArgType::AT_2BYTE_ADDR);

	// CPL
	ADD_INST(L"CPLA",			"\x43");
	ADD_INST(L"CPL(V)",			"\x33", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPL(V)",			"\x72\x53", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPL(X)",			"\x73");
	ADD_INST(L"CPL(V,X)",		"\x63", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPL(V,X)",		"\x72\x43", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPL(Y)",			"\x90\x73");
	ADD_INST(L"CPL(V,Y)",		"\x90\x63", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPL(V,Y)",		"\x90\x43", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPL(V,SP)",		"\x03", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPL[V]",			"\x92\x33", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPL[V]",			"\x72\x33", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPL([V],X)",		"\x92\x63", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPL([V],X]",		"\x72\x63", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPL([V],Y)",		"\x91\x63", ArgType::AT_1BYTE_ADDR);

	// CPLW
	ADD_INST(L"CPLWX",			"\x53");
	ADD_INST(L"CPLWY",			"\x90\x53");

	// DEC
	ADD_INST(L"DECA",			"\x4A");
	ADD_INST(L"DEC(V)",			"\x3A", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"DEC(V)",			"\x72\x5A", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"DEC(X)",			"\x7A");
	ADD_INST(L"DEC(V,X)",		"\x6A", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"DEC(V,X)",		"\x72\x4A", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"DEC(Y)",			"\x90\x7A");
	ADD_INST(L"DEC(V,Y)",		"\x90\x6A", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"DEC(V,Y)",		"\x90\x4A", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"DEC(V,SP)",		"\x0A", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"DEC[V]",			"\x92\x3A", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"DEC[V]",			"\x72\x3A", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"DEC([V],X)",		"\x92\x6A", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"DEC([V],X]",		"\x72\x6A", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"DEC([V],Y)",		"\x91\x6A", ArgType::AT_1BYTE_ADDR);

	// DECW
	ADD_INST(L"DECWX",			"\x5A");
	ADD_INST(L"DECWY",			"\x90\x5A");

	// DIV
	ADD_INST(L"DIVX,A",			"\x62");
	ADD_INST(L"DIVY,A",			"\x90\x62");

	// DIVW
	ADD_INST(L"DIVWX,Y",		"\x65");

	// EXG
	ADD_INST(L"EXGA,XL",		"\x41");
	ADD_INST(L"EXGA,YL",		"\x61");
	ADD_INST(L"EXGA,(V)",		"\x31", ArgType::AT_2BYTE_ADDR);

	// EXGW
	ADD_INST(L"EXGWX,Y",		"\x51");

	// HALT
	ADD_INST(L"HALT",			"\x8E");

	// INC
	ADD_INST(L"INCA",			"\x4C");
	ADD_INST(L"INC(V)",			"\x3c", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"INC(V)",			"\x72\x5C", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"INC(X)",			"\x7C");
	ADD_INST(L"INC(V,X)",		"\x6C", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"INC(V,X)",		"\x72\x4C", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"INC(Y)",			"\x90\x7C");
	ADD_INST(L"INC(V,Y)",		"\x90\x6C", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"INC(V,Y)",		"\x90\x4C", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"INC(V,SP)",		"\x0C", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"INC[V]",			"\x92\x3C", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"INC[V]",			"\x72\x3C", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"INC([V],X)",		"\x92\x6C", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"INC([V],X]",		"\x72\x6C", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"INC([V],Y)",		"\x91\x6C", ArgType::AT_1BYTE_ADDR);

	// INCW
	ADD_INST(L"INCWX",			"\x5C");
	ADD_INST(L"INCWY",			"\x90\x5C");

	// INT
	ADD_INST(L"INTV",			"\x82", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"INT(V)",			"\x82", ArgType::AT_3BYTE_ADDR);

	// IRET
	ADD_INST(L"IRET",			"\x80");

	// JP
	ADD_INST(L"JPV",			"\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"JP(V)",			"\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"JP(X)",			"\xFC");
	ADD_INST(L"JP(V,X)",		"\xEC", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"JP(V,X)",		"\xDC", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"JP(Y)",			"\x90\xFC");
	ADD_INST(L"JP(V,Y)",		"\x90\xEC", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"JP(V,Y)",		"\x90\xDC", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"JP[V]",			"\x92\xCC", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"JP[V]",			"\x72\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"JP([V],X)",		"\x92\xDC", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"JP([V],X)",		"\x72\xDC", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"JP([V],Y)",		"\x91\xDC", ArgType::AT_1BYTE_ADDR);

	// JPF
	ADD_INST(L"JPFV",			"\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"JPF(V)",			"\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"JPF[V]",			"\x92\xAC", ArgType::AT_2BYTE_ADDR);

	// JRX
	ADD_INST(L"JRAV",			"\x20", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRTV",			"\x20", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRCV",			"\x25", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRULTV",			"\x25", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JREQV",			"\x27", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRFV",			"\x21", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRHV",			"\x90\x29", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRIHV",			"\x90\x2F", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRILV",			"\x90\x2E", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRMV",			"\x90\x2D", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRMIV",			"\x2B", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRNCV",			"\x24", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRUGEV",			"\x24", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRNEV",			"\x26", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRNHV",			"\x90\x28", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRNMV",			"\x90\x2C", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRNVV",			"\x28", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRPLV",			"\x2A", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRSGEV",			"\x2E", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRSGTV",			"\x2C", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRSLEV",			"\x2D", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRSLTV",			"\x2F", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRUGTV",			"\x22", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRULEV",			"\x23", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRVV",			"\x29", ArgType::AT_1BYTE_OFF);

	// LD
	ADD_INST(L"LDA,V",			"\xA6", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"LDA,(V)",		"\xB6", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDA,(V)",		"\xC6", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDA,(X)",		"\xF6");
	ADD_INST(L"LDA,(V,X)",		"\xE6", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDA,(V,X)",		"\xD6", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDA,(Y)",		"\x90\xF6");
	ADD_INST(L"LDA,(V,Y)",		"\x90\xE6", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDA,(V,Y)",		"\x90\xD6", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDA,(V,SP)",		"\x7B", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDA,[V]",		"\x92\xC6", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDA,[V]",		"\x72\xC6", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDA,([V],X)",	"\x92\xD6", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDA,([V],X)",	"\x72\xD6", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDA,([V],Y)",	"\x91\xD6", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD(V),A",		"\xB7", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD(V),A",		"\xC7", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LD(X),A",		"\xF7");
	ADD_INST(L"LD(V,X),A",		"\xE7", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD(V,X),A",		"\xD7", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LD(Y),A",		"\x90\xF7");
	ADD_INST(L"LD(V,Y),A",		"\x90\xE7", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD(V,Y),A",		"\x90\xD7", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LD(V,SP),A",		"\x6B", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD[V],A",		"\x92\xC7", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD[V],A",		"\x72\xC7", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LD([V],X),A",	"\x92\xD7", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD([V],X),A",	"\x72\xD7", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LD([V],Y),A",	"\x91\xD7", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDXL,A",			"\x97");
	ADD_INST(L"LDA,XL",			"\x9F");
	ADD_INST(L"LDYL,A",			"\x90\x97");
	ADD_INST(L"LDA,YL",			"\x90\x9F");
	ADD_INST(L"LDXH,A",			"\x95");
	ADD_INST(L"LDA,XH",			"\x9E");
	ADD_INST(L"LDYH,A",			"\x90\x95");
	ADD_INST(L"LDA,YH",			"\x90\x9E");

	// LDF
	ADD_INST(L"LDFA,(V)",		"\xBC", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"LDFA,(V,X)",		"\xAF", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"LDFA,(V,Y)",		"\x90\xAF", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"LDFA,[V]",		"\x92\xBC", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDFA,([V],X)",	"\x92\xAF", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDFA,([V],Y)",	"\x91\xAF", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDF(V),A",		"\xBD", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"LDF(V,X),A",		"\xA7", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"LDF(V,Y),A",		"\x90\xA7", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"LDF[V],A",		"\x92\xBD", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDF([V],X),A",	"\x92\xA7", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDF([V],Y),A",	"\x91\xA7", ArgType::AT_2BYTE_ADDR);

	// LDW
	ADD_INST(L"LDWX,V",			"\xAE", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"LDWX,(V)",		"\xBE", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWX,(V)",		"\xCE", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDWX,(X)",		"\xFE");
	ADD_INST(L"LDWX,(V,X)",		"\xEE", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWX,(V,X)",		"\xDE", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDWX,(V,SP)",	"\x1E", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWX,[V]",		"\x92\xCE", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWX,[V]",		"\x72\xCE", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDWX,([V],X)",	"\x92\xDE", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWX,([V],X)",	"\x92\xDE", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDW(V),X",		"\xBF", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW(V),X",		"\xCF", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDW(X),Y",		"\xFF");
	ADD_INST(L"LDW(V,X),Y",		"\xEF", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW(V,X),Y",		"\xDF", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDW(V,SP),X",	"\x1F", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW[V],X",		"\x92\xCF", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW[V],X",		"\x72\xCF", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDW([V],X),Y",	"\x92\xDF", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW([V],X),Y",	"\x72\xDF", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDWY,V",			"\x90\xAE", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"LDWY,(V)",		"\x90\xBE", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWY,(V)",		"\x90\xCE", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDWY,(Y)",		"\x90\xFE");
	ADD_INST(L"LDWY,(V,Y)",		"\x90\xEE", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWY,(V,Y)",		"\x90\xDE", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDWY,(V,SP)",	"\x16", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWY,[V]",		"\x91\xCE", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWY,([V],Y)",	"\x91\xDE", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW(V),Y",		"\x90\xBF", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW(V),Y",		"\x90\xCF", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDW(Y),X",		"\x90\xFF");
	ADD_INST(L"LDW(V,Y),X",		"\x90\xEF", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW(V,Y),X",		"\x90\xDF", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDW(V,SP),Y",	"\x17", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW[V],Y",		"\x91\xCF", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW([V],Y),X",	"\x91\xDF", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWY,X",			"\x90\x93");
	ADD_INST(L"LDWX,Y",			"\x93");
	ADD_INST(L"LDWX,SP",		"\x96");
	ADD_INST(L"LDWSP,X",		"\x94");
	ADD_INST(L"LDWY,SP",		"\x90\x96");
	ADD_INST(L"LDWSP,Y",		"\x90\x94");

	// MOV
	ADD_INST(L"MOV(V),V",		"\x35", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_VAL, true);
	ADD_INST(L"MOV(V),(V)",		"\x45", ArgType::AT_1BYTE_ADDR, ArgType::AT_1BYTE_ADDR, true);
	ADD_INST(L"MOV(V),(V)",		"\x55", ArgType::AT_2BYTE_ADDR, ArgType::AT_2BYTE_ADDR, true);

	// MUL
	ADD_INST(L"MULX,A",			"\x42");
	ADD_INST(L"MULY,A",			"\x90\x42");

	// NEG
	ADD_INST(L"NEGA",			"\x40");
	ADD_INST(L"NEG(V)",			"\x30", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"NEG(V)",			"\x72\x50", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"NEG(X)",			"\x70");
	ADD_INST(L"NEG(V,X)",		"\x60", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"NEG(V,X)",		"\x72\x40", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"NEG(Y)",			"\x90\x70");
	ADD_INST(L"NEG(V,Y)",		"\x90\x60", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"NEG(V,Y)",		"\x90\x40", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"NEG(V,SP)",		"\x00", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"NEG[V]",			"\x92\x30", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"NEG[V]",			"\x72\x30", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"NEG([V],X)",		"\x92\x60", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"NEG([V],X]",		"\x72\x60", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"NEG([V],Y)",		"\x91\x60", ArgType::AT_1BYTE_ADDR);

	// NEGW
	ADD_INST(L"NEGWX",			"\x50");
	ADD_INST(L"NEGWY",			"\x90\x50");

	// NOP
	ADD_INST(L"NOP",			"\x9D");

	// OR
	ADD_INST(L"ORA,V",			"\xAA", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"ORA,(V)",		"\xBA", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ORA,(V)",		"\xCA", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ORA,(X)",		"\xFA");
	ADD_INST(L"ORA,(V,X)",		"\xEA", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ORA,(V,X)",		"\xDA", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ORA,(Y)",		"\x90\xFA");
	ADD_INST(L"ORA,(V,Y)",		"\x90\xEA", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ORA,(V,Y)",		"\x90\xDA", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ORA,(V,SP)",		"\x1A", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ORA,[V]",		"\x92\xCA", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ORA,[V]",		"\x72\xCA", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ORA,([V],X)",	"\x92\xDA", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ORA,([V],X)",	"\x72\xDA", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ORA,([V],Y)",	"\x91\xDA", ArgType::AT_1BYTE_ADDR);

	// POP
	ADD_INST(L"POPA",			"\x84");
	ADD_INST(L"POPCC",			"\x86");
	ADD_INST(L"POP(V)",			"\x32", ArgType::AT_2BYTE_ADDR);

	// POPW
	ADD_INST(L"POPWX",			"\x85");
	ADD_INST(L"POPWY",			"\x90\x85");

	// PUSH
	ADD_INST(L"PUSHA",			"\x88");
	ADD_INST(L"PUSHCC",			"\x8A");
	ADD_INST(L"PUSHV",			"\x4B", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"PUSH(V)",		"\x3B", ArgType::AT_2BYTE_ADDR);

	// PUSHW
	ADD_INST(L"PUSHWX",			"\x89");
	ADD_INST(L"PUSHWY",			"\x90\x89");

	// RCF
	ADD_INST(L"RCF",			"\x98");

	// RET
	ADD_INST(L"RET",			"\x81");

	// RETF
	ADD_INST(L"RETF",			"\x87");

	// RIM
	ADD_INST(L"RIM",			"\x9A");

	// RLC
	ADD_INST(L"RLCA",			"\x49");
	ADD_INST(L"RLC(V)",			"\x39", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RLC(V)",			"\x72\x59", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RLC(X)",			"\x79");
	ADD_INST(L"RLC(V,X)",		"\x69", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RLC(V,X)",		"\x72\x49", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RLC(Y)",			"\x90\x79");
	ADD_INST(L"RLC(V,Y)",		"\x90\x69", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RLC(V,Y)",		"\x90\x49", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RLC(V,SP)",		"\x09", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RLC[V]",			"\x92\x39", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RLC[V]",			"\x72\x39", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RLC([V],X)",		"\x92\x69", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RLC([V],X]",		"\x72\x69", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RLC([V],Y)",		"\x91\x69", ArgType::AT_1BYTE_ADDR);

	// RLCW
	ADD_INST(L"RLCWX",			"\x59");
	ADD_INST(L"RLCWY",			"\x90\x59");

	// RLWA
	ADD_INST(L"RLWAX",			"\x02");
	ADD_INST(L"RLWAY",			"\x90\x02");

	// RRC
	ADD_INST(L"RRCA",			"\x46");
	ADD_INST(L"RRC(V)",			"\x36", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RRC(V)",			"\x72\x56", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RRC(X)",			"\x76");
	ADD_INST(L"RRC(V,X)",		"\x66", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RRC(V,X)",		"\x72\x46", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RRC(Y)",			"\x90\x76");
	ADD_INST(L"RRC(V,Y)",		"\x90\x66", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RRC(V,Y)",		"\x90\x46", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RRC(V,SP)",		"\x06", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RRC[V]",			"\x92\x36", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RRC[V]",			"\x72\x36", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RRC([V],X)",		"\x92\x66", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RRC([V],X]",		"\x72\x66", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RRC([V],Y)",		"\x91\x66", ArgType::AT_1BYTE_ADDR);
	
	// RRCW
	ADD_INST(L"RRCWX",			"\x56");
	ADD_INST(L"RRCWY",			"\x90\x56");

	// RRWA
	ADD_INST(L"RRWAX",			"\x01");
	ADD_INST(L"RRWAY",			"\x90\x01");

	// RVF
	ADD_INST(L"RVF",			"\x9C");

	// SBC
	ADD_INST(L"SBCA,V",			"\xA2", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"SBCA,(V)",		"\xB2", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SBCA,(V)",		"\xC2", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SBCA,(X)",		"\xF2");
	ADD_INST(L"SBCA,(V,X)",		"\xE2", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SBCA,(V,X)",		"\xD2", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SBCA,(Y)",		"\x90\xF2");
	ADD_INST(L"SBCA,(V,Y)",		"\x90\xE2", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SBCA,(V,Y)",		"\x90\xD2", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SBCA,(V,SP)",	"\x12", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SBCA,[V]",		"\x92\xC2", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SBCA,[V]",		"\x72\xC2", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SBCA,([V],X)",	"\x92\xD2", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SBCA,([V],X)",	"\x72\xD2", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SBCA,([V],Y)",	"\x91\xD2", ArgType::AT_1BYTE_ADDR);

	// SCF
	ADD_INST(L"SCF",			"\x99");

	// SIM
	ADD_INST(L"SIM",			"\x9B");

	// SLA
	ADD_INST(L"SLAA",			"\x48");
	ADD_INST(L"SLA(V)",			"\x38", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLA(V)",			"\x72\x58", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLA(X)",			"\x78");
	ADD_INST(L"SLA(V,X)",		"\x68", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLA(V,X)",		"\x72\x48", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLA(Y)",			"\x90\x78");
	ADD_INST(L"SLA(V,Y)",		"\x90\x68", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLA(V,Y)",		"\x90\x48", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLA(V,SP)",		"\x08", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLA[V]",			"\x92\x38", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLA[V]",			"\x72\x38", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLA([V],X)",		"\x92\x68", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLA([V],X]",		"\x72\x68", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLA([V],Y)",		"\x91\x68", ArgType::AT_1BYTE_ADDR);

	// SLAW
	ADD_INST(L"SLAWX",			"\x58");
	ADD_INST(L"SLAWY",			"\x90\x58");

	// SLL
	ADD_INST(L"SLLA",			"\x48");
	ADD_INST(L"SLL(V)",			"\x38", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLL(V)",			"\x72\x58", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLL(X)",			"\x78");
	ADD_INST(L"SLL(V,X)",		"\x68", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLL(V,X)",		"\x72\x48", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLL(Y)",			"\x90\x78");
	ADD_INST(L"SLL(V,Y)",		"\x90\x68", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLL(V,Y)",		"\x90\x48", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLL(V,SP)",		"\x08", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLL[V]",			"\x92\x38", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLL[V]",			"\x72\x38", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLL([V],X)",		"\x92\x68", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLL([V],X]",		"\x72\x68", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLL([V],Y)",		"\x91\x68", ArgType::AT_1BYTE_ADDR);

	// SLLW
	ADD_INST(L"SLLWX",			"\x58");
	ADD_INST(L"SLLWY",			"\x90\x58");

	// SRA
	ADD_INST(L"SRAA",			"\x47");
	ADD_INST(L"SRA(V)",			"\x37", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRA(V)",			"\x72\x57", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRA(X)",			"\x77");
	ADD_INST(L"SRA(V,X)",		"\x67", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRA(V,X)",		"\x72\x47", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRA(Y)",			"\x90\x77");
	ADD_INST(L"SRA(V,Y)",		"\x90\x67", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRA(V,Y)",		"\x90\x47", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRA(V,SP)",		"\x07", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRA[V]",			"\x92\x37", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRA[V]",			"\x72\x37", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRA([V],X)",		"\x92\x67", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRA([V],X]",		"\x72\x67", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRA([V],Y)",		"\x91\x67", ArgType::AT_1BYTE_ADDR);

	// SRAW
	ADD_INST(L"SRAWX",			"\x57");
	ADD_INST(L"SRAWY",			"\x90\x57");

	// SRL
	ADD_INST(L"SRLA",			"\x44");
	ADD_INST(L"SRL(V)",			"\x34", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRL(V)",			"\x72\x54", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRL(X)",			"\x74");
	ADD_INST(L"SRL(V,X)",		"\x64", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRL(V,X)",		"\x72\x44", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRL(Y)",			"\x90\x74");
	ADD_INST(L"SRL(V,Y)",		"\x90\x64", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRL(V,Y)",		"\x90\x44", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRL(V,SP)",		"\x04", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRL[V]",			"\x92\x34", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRL[V]",			"\x72\x34", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRL([V],X)",		"\x92\x64", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRL([V],X]",		"\x72\x64", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRL([V],Y)",		"\x91\x64", ArgType::AT_1BYTE_ADDR);

	// SRLW
	ADD_INST(L"SRLWX",			"\x54");
	ADD_INST(L"SRLWY",			"\x90\x54");

	// SUB
	ADD_INST(L"SUBA,V",			"\xA0", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"SUBA,(V)",		"\xB0", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBA,(V)",		"\xC0", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBA,(X)",		"\xF0");
	ADD_INST(L"SUBA,(V,X)",		"\xE0", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBA,(V,X)",		"\xD0", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBA,(Y)",		"\x90\xF0");
	ADD_INST(L"SUBA,(V,Y)",		"\x90\xE0", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBA,(V,Y)",		"\x90\xD0", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBA,(V,SP)",	"\x10", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBA,[V]",		"\x92\xC0", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBA,[V]",		"\x72\xC0", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBA,([V],X)",	"\x92\xD0", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBA,([V],X)",	"\x72\xD0", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBA,([V],Y)",	"\x91\xD0", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBSP,V",		"\x52", ArgType::AT_1BYTE_ADDR);

	// SUBW
	ADD_INST(L"SUBWX,V",		"\x1D", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"SUBWX,(V)",		"\x72\xB0", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBWX,(V,SP)",	"\x72\xF0", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBWY,V",		"\x72\xA2", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"SUBWY,(V)",		"\x72\xB2", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBWY,(V,SP)",	"\x72\xF2", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBWSP,V",		"\x52", ArgType::AT_1BYTE_ADDR);

	// SWAP
	ADD_INST(L"SWAPA",			"\x4E");
	ADD_INST(L"SWAP(V)",		"\x3E", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SWAP(V)",		"\x72\x5E", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SWAP(X)",		"\x7E");
	ADD_INST(L"SWAP(V,X)",		"\x6E", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SWAP(V,X)",		"\x72\x4E", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SWAP(Y)",		"\x90\x7E");
	ADD_INST(L"SWAP(V,Y)",		"\x90\x6E", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SWAP(V,Y)",		"\x90\x4E", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SWAP(V,SP)",		"\x0E", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SWAP[V]",		"\x92\x3E", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SWAP[V]",		"\x72\x3E", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SWAP([V],X)",	"\x92\x6E", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SWAP([V],X]",	"\x72\x6E", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SWAP([V],Y)",	"\x91\x6E", ArgType::AT_1BYTE_ADDR);

	// SWAPW
	ADD_INST(L"SWAPWX",			"\x5E");
	ADD_INST(L"SWAPWY",			"\x90\x5E");

	// TNZ
	ADD_INST(L"TNZA",			"\x4D");
	ADD_INST(L"TNZ(V)",			"\x3D", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"TNZ(V)",			"\x72\x5D", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"TNZ(X)",			"\x7D");
	ADD_INST(L"TNZ(V,X)",		"\x6D", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"TNZ(V,X)",		"\x72\x4D", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"TNZ(Y)",			"\x90\x7D");
	ADD_INST(L"TNZ(V,Y)",		"\x90\x6D", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"TNZ(V,Y)",		"\x90\x4D", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"TNZ(V,SP)",		"\x0D", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"TNZ[V]",			"\x92\x3D", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"TNZ[V]",			"\x72\x3D", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"TNZ([V],X)",		"\x92\x6D", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"TNZ([V],X]",		"\x72\x6D", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"TNZ([V],Y)",		"\x91\x6D", ArgType::AT_1BYTE_ADDR);

	// TNZW
	ADD_INST(L"TNZWX",			"\x5D");
	ADD_INST(L"TNZWY",			"\x90\x5D");

	// TRAP
	ADD_INST(L"TRAP",			"\x83");

	// WFE
	ADD_INST(L"WFE",			"\x72\x8F");

	// WFI
	ADD_INST(L"WFI", "\x8F");

	// XOR
	ADD_INST(L"XORA,V",			"\xA8", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"XORA,(V)",		"\xB8", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"XORA,(V)",		"\xC8", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"XORA,(X)",		"\xF8");
	ADD_INST(L"XORA,(V,X)",		"\xE8", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"XORA,(V,X)",		"\xD8", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"XORA,(Y)",		"\x90\xF8");
	ADD_INST(L"XORA,(V,Y)",		"\x90\xE8", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"XORA,(V,Y)",		"\x90\xD8", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"XORA,(V,SP)",	"\x18", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"XORA,[V]",		"\x92\xC8", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"XORA,[V]",		"\x72\xC8", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"XORA,([V],X)",	"\x92\xD8", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"XORA,([V],X)",	"\x72\xD8", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"XORA,([V],Y)",	"\x91\xD8", ArgType::AT_1BYTE_ADDR);
}

static std::set<std::pair<int32_t, std::string>> _instructions_to_replace;

static std::multimap<std::wstring, Inst> _instructions_ex;

#define ADD_INST_EX(SIGN, OPCODE, ...) _instructions_ex.emplace(SIGN, Inst((sizeof(OPCODE) / sizeof(OPCODE[0])) - 1, (const unsigned char *)(OPCODE), ##__VA_ARGS__))

// CALLR -> CALL (if necessary), JRX -> JP (if necessary)
static void load_extra_instructions_small()
{
	// CALLR
	ADD_INST_EX(L"CALLRV",			"\xCD", ArgType::AT_2BYTE_ADDR);

	// JRX
	ADD_INST_EX(L"JRAV",			"\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRTV",			"\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRCV",			"\x24\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRULTV",			"\x24\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JREQV",			"\x26\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRHV",			"\x90\x28\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRIHV",			"\x90\x2E\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRILV",			"\x90\x2F\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRMV",			"\x90\x2C\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRMIV",			"\x2A\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRNCV",			"\x25\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRUGEV",			"\x25\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRNEV",			"\x27\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRNHV",			"\x90\x29\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRNMV",			"\x90\x2D\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRNVV",			"\x29\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRPLV",			"\x2B\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRSGEV",			"\x2F\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRSGTV",			"\x2D\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRSLEV",			"\x2C\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRSLTV",			"\x2E\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRUGTV",			"\x23\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRULEV",			"\x22\x03\xCC", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRVV",			"\x28\x03\xCC", ArgType::AT_2BYTE_ADDR);
}

// JRX -> JPF (if necessary), JP -> JPF, CALL and CALLR -> CALLF, RET -> RETF
static void load_extra_instructions_large()
{
	// CALLR
	ADD_INST_EX(L"CALLRV",			"\x8D", ArgType::AT_3BYTE_ADDR);

	// CALL
	ADD_INST_EX(L"CALLV",			"\x8D", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"CALL(V)",			"\x8D", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"CALL[V]",			"\x92\x8D", ArgType::AT_2BYTE_ADDR);

	// JP
	ADD_INST_EX(L"JPV",				"\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JP(V)",			"\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JP[V]",			"\x92\xAC", ArgType::AT_2BYTE_ADDR);

	// JRX
	ADD_INST_EX(L"JRAV",			"\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRTV",			"\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRCV",			"\x24\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRULTV",			"\x24\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JREQV",			"\x26\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRHV",			"\x90\x28\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRIHV",			"\x90\x2E\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRILV",			"\x90\x2F\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRMV",			"\x90\x2C\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRMIV",			"\x2A\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRNCV",			"\x25\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRUGEV",			"\x25\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRNEV",			"\x27\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRNHV",			"\x90\x29\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRNMV",			"\x90\x2D\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRNVV",			"\x29\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRPLV",			"\x2B\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRSGEV",			"\x2F\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRSGTV",			"\x2D\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRSLEV",			"\x2C\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRSLTV",			"\x2E\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRUGTV",			"\x23\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRULEV",			"\x22\x03\xAC", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRVV",			"\x28\x03\xAC", ArgType::AT_3BYTE_ADDR);

	// RET
	ADD_INST_EX(L"RET",				"\x87");
}

class CodeStmt: public ConstStmt
{
protected:
	bool _revorder;
	std::vector<std::pair<ArgType, Exp>> _refs;

	A1STM8_T_ERROR WriteRef(IhxWriter *writer, const std::pair<ArgType, Exp> &ref, const std::map<std::wstring, MemRef> &memrefs)
	{
		int32_t addr = 0;
		uint8_t data[3];
		int size = 0;

		auto err = ref.second.Eval(addr, memrefs);
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		if(ref.first == ArgType::AT_1BYTE_VAL)
		{
			size = 1;

			if(addr < -128 || addr > 255)
			{
				_warnings.push_back(A1STM8_T_WARNING::A1STM8_WRN_WINTOUTRANGE);
			}

			data[0] = (uint8_t)addr;
		}
		else
		if(ref.first == ArgType::AT_2BYTE_VAL)
		{
			size = 2;

			if(addr < -32768 || addr > 65535)
			{
				_warnings.push_back(A1STM8_T_WARNING::A1STM8_WRN_WINTOUTRANGE);
			}

			data[0] = (uint8_t)(((uint16_t)addr) >> 8);
			data[1] = (uint8_t)addr;
		}
		else
		if(ref.first == ArgType::AT_1BYTE_ADDR)
		{
			size = 1;

			if(addr < 0 || addr > 0xFF)
			{
				_warnings.push_back(A1STM8_T_WARNING::A1STM8_WRN_WADDROUTRANGE);
			}

			data[0] = (uint8_t)addr;
		}
		else
		if(ref.first == ArgType::AT_2BYTE_ADDR)
		{
			size = 2;

			if(addr < 0 || addr > 0xFFFF)
			{
				_warnings.push_back(A1STM8_T_WARNING::A1STM8_WRN_WADDROUTRANGE);
			}

			data[0] = (uint8_t)(((uint16_t)addr) >> 8);
			data[1] = (uint8_t)addr;
		}
		else
		if(ref.first == ArgType::AT_3BYTE_ADDR)
		{
			size = 3;

			if(addr < 0 || addr > 0xFFFFFF)
			{
				_warnings.push_back(A1STM8_T_WARNING::A1STM8_WRN_WADDROUTRANGE);
			}

			data[0] = (uint8_t)(addr >> 16);
			data[1] = (uint8_t)(((uint16_t)addr) >> 8);
			data[2] = (uint8_t)addr;
		}
		else
		if(ref.first == ArgType::AT_1BYTE_OFF)
		{
			addr = addr - _address - _size;
			size = 1;

			if(addr < -128 || addr > 127)
			{
				return A1STM8_T_ERROR::A1STM8_RES_ERELOUTRANGE;
			}

			data[0] = (int8_t)addr;
		}
		else
		{
			return A1STM8_T_ERROR::A1STM8_RES_EINVREFTYPE;
		}

		err = writer->Write(data, size);
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR ReadInstArg(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, std::wstring &argsign, bool bit_arg = false)
	{
		std::wstring tok;
		std::vector<std::wstring> brackets;

		if(start == end)
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		while(start != end && !start->IsEOL() && !start->IsEOF() && !(*start == Token(TokType::TT_OPER, L",", -1) && brackets.empty()))
		{
			if(*start == Token(TokType::TT_OPER, L"(", -1) || *start == Token(TokType::TT_OPER, L"[", -1))
			{
				brackets.push_back(start->GetToken());
			}
			else
			if(*start == Token(TokType::TT_OPER, L")", -1) || *start == Token(TokType::TT_OPER, L"]", -1))
			{
				if(brackets.empty())
				{
					return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
				}

				brackets.pop_back();
			}
			else
			if(*start != Token(TokType::TT_OPER, L",", -1))
			{
				// read next argument expression item
				std::vector<Token> terms;

				if(!brackets.empty())
				{
					terms.push_back(Token(TokType::TT_OPER, L"]", -1));
					terms.push_back(Token(TokType::TT_OPER, L")", -1));
				}

				terms.push_back(Token(TokType::TT_OPER, L",", -1));
				terms.push_back(Token(TokType::TT_EOL, L"", -1));
				terms.push_back(Token(TokType::TT_EOF, L"", -1));

				Exp exp;

				auto err = Exp::BuildExp(start, end, exp, terms);
				if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
				{
					return err;
				}

				if(bit_arg || exp == L"A" || exp == L"X" || exp == L"XL" || exp == L"XH" || exp == L"Y" || exp == L"YL" || exp == L"YH" || exp == L"SP" || exp == L"CC")
				{
					// register or bit argument
					argsign += exp;
				}
				else
				{
					// expression
					_refs.push_back(std::pair<ArgType, Exp>(ArgType::AT_NONE, exp));
					argsign += L"V";
				}

				continue;
			}

			argsign += start->GetToken();
			start++;
		}

		if(!brackets.empty())
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

public:
	CodeStmt()
	: ConstStmt()
	, _revorder(false)
	{
	}

	virtual ~CodeStmt()
	{
	}

	A1STM8_T_ERROR Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, const std::map<std::wstring, MemRef> &memrefs, const std::string &file_name) override
	{
		if(start == end)
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		if(start->GetType() != TokType::TT_STRING)
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		_refs.clear();

		const auto op_name = start->GetToken();

		if(op_name == L"DB" || op_name == L"DW")
		{
			return ConstStmt::Read(start, end, memrefs, file_name);
		}

		auto line_num = start->GetLineNum();

		// read instruction
		int arg_num = 0;
		int bit_arg_pos = -1;

		if (arg_num == 0 &&
			(op_name == L"BCCM" || op_name == L"BCPL" || op_name == L"BRES" || op_name == L"BSET" || op_name == L"BTJF" || op_name == L"BTJT")
			)
		{
			bit_arg_pos = 1;
		}

		auto signature = op_name;

		for(; arg_num < 3; arg_num++)
		{
			start++;

			if(start != end && start->GetType() != TokType::TT_EOL && start->GetType() != TokType::TT_EOF)
			{
				auto err = ReadInstArg(start, end, signature, arg_num == bit_arg_pos);
				if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
				{
					return err;
				}

				if(start != end && start->GetType() != TokType::TT_EOL && start->GetType() != TokType::TT_EOF)
				{
					if(start->GetType() == TokType::TT_OPER && start->GetToken() == L",")
					{
						signature += L",";
						continue;
					}
					else
					{
						return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
					}
				}
			}

			break;
		}

		if(start != end && start->GetType() != TokType::TT_EOL && start->GetType() != TokType::TT_EOF)
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		bool use_ex_opcodes = false;

		// replace JP -> JPF, CALL and CALLR -> CALLF, RET -> RETF
		if(_global_settings.GetFixAddresses() && _global_settings.GetMemModelLarge())
		{
			if(op_name == L"JP" || op_name == L"CALL" || op_name == L"CALLR" || op_name == L"RET")
			{
				use_ex_opcodes = true;
			}
		}

		// replace instructions with relative addressing if their addresses are out of range
		if(!use_ex_opcodes && (_global_settings.GetFixAddresses() && _instructions_to_replace.find(std::make_pair(line_num, file_name)) != _instructions_to_replace.end()))
		{
			use_ex_opcodes = true;
		}

		const auto &ginsts = use_ex_opcodes ? _instructions_ex : _instructions;
		
		auto inst_num = ginsts.count(signature);
		
		if(inst_num == 0)
		{
			return A1STM8_T_ERROR::A1STM8_RES_EINVINST;
		}

		auto insts = ginsts.find(signature);
		const Inst *inst = &insts->second;

		// the code below processes STM8 short/long addresses (selects proper instruction)
		if(inst_num > 1)
		{
			// more than one instruction found (two at most now)
			// try to evaluate arguments
			bool eval = true;
			std::vector<int32_t> vals;
			for(auto &r: _refs)
			{
				int32_t val = -1;
				auto err = r.second.Eval(val, memrefs);
				if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
				{
					eval = false;
					break;
				}
				vals.push_back(val);
			}

			bool page0addresses = false;

			if(eval)
			{
				if(vals.size() == 1 && vals[0] >= 0 && vals[0] <= 255)
				{
					page0addresses = true;
				}
				else
				if(vals.size() == 2 && vals[0] >= 0 && vals[0] <= 255 && vals[1] >= 0 && vals[1] <= 255)
				{
					page0addresses = true;
				}
			}

			if(page0addresses)
			{
				if(_refs.size() == 1 && inst->_argtypes[0] != ArgType::AT_1BYTE_ADDR)
				{
					insts++;
					inst = &insts->second;
				}
				else
				if(_refs.size() == 2 && (inst->_argtypes[0] != ArgType::AT_1BYTE_ADDR || inst->_argtypes[1] != ArgType::AT_1BYTE_ADDR))
				{
					insts++;
					inst = &insts->second;
				}
			}
			else
			{
				// all non-resolved references should be in code sections soo they cannot be short addresses
				if(_refs.size() == 1 && inst->_argtypes[0] == ArgType::AT_1BYTE_ADDR)
				{
					insts++;
					inst = &insts->second;
				}
				else
				if(_refs.size() == 2 && (inst->_argtypes[0] == ArgType::AT_1BYTE_ADDR || inst->_argtypes[1] == ArgType::AT_1BYTE_ADDR))
				{
					insts++;
					inst = &insts->second;
				}
			}
		}

		_data.resize(inst->_size);
		std::copy(inst->_code, inst->_code + inst->_size, _data.begin());

		_size = _data.size();
		for(auto i = 0; i < inst->_argnum; i++)
		{
			_refs[i].first = inst->_argtypes[i];

			switch(inst->_argtypes[i])
			{
				case ArgType::AT_1BYTE_VAL:
					_size += 1;
					break;
				case ArgType::AT_2BYTE_VAL:
					_size += 2;
					break;
				case ArgType::AT_1BYTE_ADDR:
					_size += 1;
					break;
				case ArgType::AT_2BYTE_ADDR:
					_size += 2;
					break;
				case ArgType::AT_3BYTE_ADDR:
					_size += 3;
					break;
				case ArgType::AT_1BYTE_OFF:
					_size += 1;
					break;
			}
		}

		_revorder = inst->_revorder;

		_line_num = line_num;

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR Write(IhxWriter *writer, const std::map<std::wstring, MemRef> &memrefs) override
	{
		auto err = writer->Write(_data.data(), _data.size());
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		int ref_num = _refs.size();
		for(int i = 0; i < ref_num; i++)
		{
			auto err = WriteRef(writer, _refs[_revorder ? ref_num - i - 1: i], memrefs);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}
};

class CodeInitStmt: public CodeStmt
{
public:
	CodeInitStmt()
	: CodeStmt()
	{
	}

	virtual ~CodeInitStmt()
	{
	}
};

class Sections: std::vector<Section>
{
private:
	int32_t _curr_line_num;
	std::string _curr_file_name;
	std::vector<std::tuple<int32_t, std::string, A1STM8_T_WARNING>> _warnings;

	std::vector<std::string> _src_files;
	std::vector<std::vector<Token>> _token_files;

	std::map<std::wstring, MemRef> _memrefs;

	int32_t _data_size;
	int32_t _init_size;
	int32_t _const_size;
	int32_t _code_size;


	A1STM8_T_ERROR ReadStmt(int32_t file_num, std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end)
	{
		auto stype = back().GetType();

		if(start->IsLabel())
		{
			MemRef mr;

			auto err = mr.Read(start, end);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}

			int32_t ssize = 0;
			err = back().GetSize(ssize);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}

			mr.SetAddress(back().GetAddress() + ssize);

			auto refn = mr.GetName();

			if(_memrefs.find(refn) != _memrefs.end())
			{
				return A1STM8_T_ERROR::A1STM8_RES_EDUPSYM;
			}

			// do not use labels in stack and heap sections
			if(!(stype == SectType::ST_STACK || stype == SectType::ST_HEAP))
			{
				_memrefs[refn] = mr;
			}
		}
		else
		if(start->IsString())
		{
			std::unique_ptr<GenStmt> stmt;

			switch(stype)
			{
				case SectType::ST_PAGE0:
					stmt.reset(new Page0Stmt());
					break;
				case SectType::ST_DATA:
					stmt.reset(new DataStmt());
					break;
				case SectType::ST_HEAP:
					stmt.reset(new HeapStmt());
					break;
				case SectType::ST_STACK:
					stmt.reset(new StackStmt());
					break;
				case SectType::ST_CONST:
					stmt.reset(new ConstStmt());
					break;
				case SectType::ST_CODE:
					stmt.reset(new CodeStmt());
					break;
				case SectType::ST_INIT:
					stmt.reset(new CodeInitStmt());
					break;
				default:
					return A1STM8_T_ERROR::A1STM8_RES_ENOSEC;
			}

			auto err = stmt->Read(start, end, _memrefs, _src_files[file_num]);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}

			int32_t ssize = 0;
			err = back().GetSize(ssize);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}

			stmt->SetAddress(back().GetAddress() + ssize);

			back().push_back(stmt.release());
		}
		else
		{
			return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR CheckIFDir(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, bool &res)
	{
		int32_t resl = 0, resr = 0;

		Exp expl, expr;
		std::vector<Token> terms;

		terms.push_back(Token(TokType::TT_OPER, L"==", -1));
		terms.push_back(Token(TokType::TT_OPER, L"!=", -1));
		terms.push_back(Token(TokType::TT_OPER, L">", -1));
		terms.push_back(Token(TokType::TT_OPER, L"<", -1));
		terms.push_back(Token(TokType::TT_OPER, L">=", -1));
		terms.push_back(Token(TokType::TT_OPER, L"<=", -1));

		auto err = Exp::BuildExp(start, end, expl, terms);
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		err = expl.Eval(resl, _memrefs);
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		const auto &cmp_op = *start;
		start++;

		terms.clear();
		terms.push_back(Token(TokType::TT_EOL, L"", -1));
		
		err = Exp::BuildExp(start, end, expr, terms);
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		err = expr.Eval(resr, _memrefs);
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			return err;
		}

		if(cmp_op.GetToken() == L"==")
		{
			res = (resl == resr);
		}
		else
		if(cmp_op.GetToken() == L"!=")
		{
			res = (resl != resr);
		}
		else
		if (cmp_op.GetToken() == L">")
		{
			res = (resl > resr);
		}
		else
		if (cmp_op.GetToken() == L"<")
		{
			res = (resl < resr);
		}
		else
		if (cmp_op.GetToken() == L">=")
		{
			res = (resl >= resr);
		}
		else
		if (cmp_op.GetToken() == L"<=")
		{
			res = (resl <= resr);
		}

		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR ReadSections(int32_t file_num, SectType sec_type, int32_t sec_base, int32_t &over_size, int32_t max_size)
	{
		Section *psec = nullptr;
		bool skip = false;

		std::vector<std::tuple<bool, bool, bool, bool>> if_state;
		bool if_blck = false;	// processing .IF block
		bool if_skip = false;	// skip the current sub-block (.IF, .ELIF or .ELSE)
		bool if_chck;			// check the next .IF or .ELIF
		bool if_else;			// .ELSE sub-block

		over_size = 0;

		_curr_file_name = _src_files[file_num];
		_curr_line_num = 0;

		const auto &tok_file = _token_files[file_num];

		for(auto ti = tok_file.begin(); ti != tok_file.end(); ti++)
		{
			if(ti->IsEOL())
			{
				continue;
			}

			_curr_line_num = ti->GetLineNum();

			if(ti->IsDir())
			{
				auto token = ti->GetToken();
				bool dir_proc = false;

				if(token == L".IF")
				{
					dir_proc = true;

					if_state.push_back(std::make_tuple(if_blck, if_skip, if_chck, if_else));

					if_blck = true;
					if_else = false;

					// higher level skip
					if(skip || if_skip)
					{
						if_chck = false;
					}
					else
					{
						ti++;
						auto err = CheckIFDir(ti, tok_file.end(), if_skip);
						if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
						{
							return err;
						}

						if_skip = !if_skip;
						if_chck = if_skip;
					}
				}
				else
				if(token == L".ELIF")
				{
					dir_proc = true;

					if(!if_blck || if_else)
					{
						return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
					}

					if_skip = true;

					// check higher level skip
					if(skip || !std::get<1>(if_state.back()))
					{
						if(if_chck)
						{
							ti++;
							auto err = CheckIFDir(ti, tok_file.end(), if_skip);
							if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
							{
								return err;
							}

							if_skip = !if_skip;
							if_chck = if_skip;
						}
					}
				}
				else
				if(token == L".ELSE")
				{
					dir_proc = true;

					if(!if_blck)
					{
						return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
					}

					if_skip = true;
					if_else = true;

					// check higher level skip
					if(skip || !std::get<1>(if_state.back()))
					{
						if_skip = !if_chck;
					}
				}
				else
				if(token == L".ENDIF")
				{
					dir_proc = true;

					if(!if_blck)
					{
						return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
					}

					const auto &st = if_state.back();
					if_blck = std::get<0>(st);
					if_skip = std::get<1>(st);
					if_chck = std::get<2>(st);
					if_else = std::get<3>(st);
					if_state.pop_back();
				}

				// new section declaration
				if(!dir_proc)
				{
					SectType st =	token == L".DATA"	?	SectType::ST_DATA :
									token == L".CONST"	?	SectType::ST_CONST :
									token == L".CODE"	?	SectType::ST_CODE :
									token == L".STACK"	?	SectType::ST_STACK :
									token == L".HEAP"	?	SectType::ST_HEAP : SectType::ST_NONE;

					if(st == SectType::ST_NONE)
					{
						return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
					}

					ti++;

					if(!(ti == tok_file.end() || ti->IsEOL()))
					{
						auto sec_mod = ti->GetToken();

						ti++;

						if(st == SectType::ST_CODE && sec_mod == L"INIT")
						{
							st = SectType::ST_INIT;
						}
						else
						if(st == SectType::ST_DATA && sec_mod == L"PAGE0")
						{
							st = SectType::ST_PAGE0;
						}
						else
						{
							return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
						}
					}

					if(psec != nullptr && !(psec->GetType() == SectType::ST_STACK || psec->GetType() == SectType::ST_HEAP))
					{
						int32_t size = 0;
						auto err = psec->GetSize(size);
						if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
						{
							return err;
						}

						over_size += size;
						if(over_size > max_size)
						{
							return A1STM8_T_ERROR::A1STM8_RES_EWSECSIZE;
						}
					}

					skip = true;
					psec = nullptr;

					if(st == sec_type)
					{
						skip = false;
						push_back(Section(_curr_file_name, _curr_line_num, st, sec_base + over_size));
						psec = &back();
					}

					if(!(ti == tok_file.end() || ti->IsEOL()))
					{
						return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
					}
				}

				if(ti == tok_file.end())
				{
					break;
				}
				continue;
			}

			if(psec == nullptr && !skip)
			{
				return A1STM8_T_ERROR::A1STM8_RES_ESYNTAX;
			}

			if(skip)
			{
				if(ti == tok_file.end())
				{
					break;
				}
				continue;
			}

			if(if_skip)
			{
				for(; ti != tok_file.end() && !ti->IsEOL(); ti++);
			}
			else
			{
				auto err = ReadStmt(file_num, ti, tok_file.end());
				if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
				{
					return err;
				}
			}

			if(ti == tok_file.end())
			{
				break;
			}
		}

		if(!skip && psec != nullptr && !(psec->GetType() == SectType::ST_STACK || psec->GetType() == SectType::ST_HEAP))
		{
			int32_t size = 0;
			auto err = psec->GetSize(size);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}

			over_size += size;
			if(over_size > max_size)
			{
				return A1STM8_T_ERROR::A1STM8_RES_EWSECSIZE;
			}
		}

		_curr_file_name.clear();
		_curr_line_num = 0;
		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

public:
	Sections()
	: _curr_line_num(0)
	, _data_size(0)
	, _init_size(0)
	, _const_size(0)
	, _code_size(0)
	{
	}

	virtual ~Sections()
	{

	}

	A1STM8_T_ERROR ReadSourceFiles(const std::vector<std::string> &src_files)
	{
		_curr_line_num = 0;
		_curr_file_name.clear();

		_src_files.clear();
		_token_files.clear();

		for(const auto &f: src_files)
		{
			_curr_file_name = f;
			_curr_line_num = 0;

			SrcFile file(f);

			auto err = file.Open();
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}

			_token_files.push_back(std::vector<Token>());
			Token tok;

			while(true)
			{
				err = file.GetNextToken(tok);
				if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
				{
					_curr_line_num = file.GetLineNum();
					return err;
				}

				if(tok.GetType() == TokType::TT_EOF)
				{
					break;
				}

				_token_files.back().push_back(tok);
			}

			_src_files.push_back(f);
		}

		_curr_line_num = 0;
		_curr_file_name.clear();
		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR ReadSections()
	{
		clear();

		_memrefs.clear();
		_warnings.clear();

		_data_size = 0;
		_init_size = 0;
		_const_size = 0;
		_code_size = 0;

		// add some special symbols
		MemRef mr;

		mr.SetName(L"__RET_ADDR_SIZE");
		mr.SetAddress(_global_settings.GetMemModelSmall() ? 2 : 3);
		_memrefs[L"__RET_ADDR_SIZE"] = mr;

		// read .HEAP section
		auto first_sec_num = size();

		for(int32_t i = 0; i < _token_files.size(); i++)
		{
			int32_t hs = 0;
			auto err = ReadSections(i, SectType::ST_HEAP, 0, hs, 0);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}
		}

		if(size() == first_sec_num + 1)
		{
			int32_t hs = 0;
			auto err = at(first_sec_num).GetSize(hs);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				_curr_file_name = at(first_sec_num).GetFileName();
				return err;
			}

			if(hs > _global_settings.GetRAMSize())
			{
				_curr_file_name = at(first_sec_num).GetFileName();
				return A1STM8_T_ERROR::A1STM8_RES_EWSECSIZE;
			}

			_global_settings.SetHeapSize(hs);
		}
		else
		if(size() > first_sec_num + 1)
		{
			int32_t hs = 0;

			for(auto i = first_sec_num; i < size(); i++)
			{
				int32_t hs1;
				auto err = at(i).GetSize(hs1);
				if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
				{
					_curr_file_name = at(i).GetFileName();
					return err;
				}

				hs = hs1 > hs ? hs1 : hs;

				_warnings.push_back(std::make_tuple(at(i).GetSectLineNum(), at(i).GetFileName(), A1STM8_T_WARNING::A1STM8_WRN_WMANYHPSECT));
				if(hs > _global_settings.GetRAMSize())
				{
					_curr_file_name = at(i).GetFileName();
					return A1STM8_T_ERROR::A1STM8_RES_EWSECSIZE;
				}
			}

			_global_settings.SetHeapSize(hs);
		}


		// read .STACK section
		first_sec_num = size();

		for(int32_t i = 0; i < _token_files.size(); i++)
		{
			int32_t ss = 0;
			auto err = ReadSections(i, SectType::ST_STACK, 0, ss, 0);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}
		}

		if(size() == first_sec_num + 1)
		{
			int32_t ss = 0;
			auto err = at(first_sec_num).GetSize(ss);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}

			if(_global_settings.GetHeapSize() + ss > _global_settings.GetRAMSize())
			{
				_curr_file_name = at(first_sec_num).GetFileName();
				return A1STM8_T_ERROR::A1STM8_RES_EWSECSIZE;
			}

			_global_settings.SetStackSize(ss);
		}
		else
		if(size() > first_sec_num + 1)
		{
			int32_t ss = 0;

			for(auto i = first_sec_num; i < size(); i++)
			{
				int32_t ss1;
				auto err = at(i).GetSize(ss1);
				if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
				{
					return err;
				}

				ss = ss1 > ss ? ss1 : ss;

				_warnings.push_back(std::make_tuple(at(i).GetSectLineNum(), at(i).GetFileName(), A1STM8_T_WARNING::A1STM8_WRN_WMANYSTKSECT));
				if(_global_settings.GetHeapSize() + ss > _global_settings.GetRAMSize())
				{
					_curr_file_name = at(i).GetFileName();
					return A1STM8_T_ERROR::A1STM8_RES_EWSECSIZE;
				}
			}

			_global_settings.SetStackSize(ss);
		}

		// .STACK section size
		mr.SetName(L"__STACK_START");
		mr.SetAddress(_global_settings.GetRAMStart() + (_global_settings.GetRAMSize() - _global_settings.GetStackSize()));
		_memrefs[L"__STACK_START"] = mr;
		mr.SetName(L"__STACK_SIZE");
		mr.SetAddress(_global_settings.GetStackSize());
		_memrefs[L"__STACK_SIZE"] = mr;


		// read PAGE0 sections
		for(int32_t i = 0; i < _token_files.size(); i++)
		{
			int32_t size = 0;
			auto err = ReadSections(i, SectType::ST_PAGE0, _global_settings.GetRAMStart() + _data_size, size, _global_settings.GetRAMSize() - _data_size - _global_settings.GetHeapSize());
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}
			_data_size += size;

			if(_data_size > STM8_PAGE0_SIZE)
			{
				_curr_file_name = _src_files[i];
				return A1STM8_T_ERROR::A1STM8_RES_EWSECSIZE;
			}

			if(_data_size + _global_settings.GetHeapSize() + _global_settings.GetStackSize() > _global_settings.GetRAMSize())
			{
				_warnings.push_back(std::make_tuple(-1, _src_files[i], A1STM8_T_WARNING::A1STM8_WRN_EWNORAM));
			}
		}

		// read DATA sections
		for(int32_t i = 0; i < _token_files.size(); i++)
		{
			int32_t size = 0;
			auto err = ReadSections(i, SectType::ST_DATA, _global_settings.GetRAMStart() + _data_size, size, _global_settings.GetRAMSize() - _data_size - _global_settings.GetHeapSize());
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}
			_data_size += size;

			if(_data_size + _global_settings.GetHeapSize() > _global_settings.GetRAMSize())
			{
				_curr_file_name = _src_files[i];
				return A1STM8_T_ERROR::A1STM8_RES_EWSECSIZE;
			}

			if(_data_size + _global_settings.GetHeapSize() + _global_settings.GetStackSize() > _global_settings.GetRAMSize())
			{
				_warnings.push_back(std::make_tuple(-1, _src_files[i], A1STM8_T_WARNING::A1STM8_WRN_EWNORAM));
			}
		}

		// .HEAP section size
		mr.SetName(L"__HEAP_START");
		mr.SetAddress(_global_settings.GetRAMStart() + _data_size);
		_memrefs[L"__HEAP_START"] = mr;
		mr.SetName(L"__HEAP_SIZE");
		mr.SetAddress(_global_settings.GetHeapSize());
		_memrefs[L"__HEAP_SIZE"] = mr;

		// .DATA sections size
		mr.SetName(L"__DATA_START");
		mr.SetAddress(_global_settings.GetRAMStart());
		_memrefs[L"__DATA_START"] = mr;
		mr.SetName(L"__DATA_SIZE");
		mr.SetAddress(_data_size);
		_memrefs[L"__DATA_SIZE"] = mr;
		mr.SetName(L"__DATA_TOTAL_SIZE");
		mr.SetAddress(_global_settings.GetRAMSize());
		_memrefs[L"__DATA_TOTAL_SIZE"] = mr;


		// read CODE INIT sections
		first_sec_num = size();

		for(int32_t i = 0; i < _token_files.size(); i++)
		{
			int32_t size = 0;
			auto err = ReadSections(i, SectType::ST_INIT, _global_settings.GetROMStart() + _init_size, size, _global_settings.GetROMSize());
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}
			_init_size += size;

			if(_init_size > _global_settings.GetROMSize())
			{
				_curr_file_name = _src_files[i];
				return A1STM8_T_ERROR::A1STM8_RES_EWSECSIZE;
			}
		}

		if(size() > first_sec_num + 1)
		{
			for(auto i = first_sec_num; i < size(); i++)
			{
				_warnings.push_back(std::make_tuple(at(i).GetSectLineNum(), at(i).GetFileName(), A1STM8_T_WARNING::A1STM8_WRN_WMANYCODINIT));
			}
		}

		// .CODE INIT section size
		mr.SetName(L"__INIT_START");
		mr.SetAddress(_global_settings.GetROMStart());
		_memrefs[L"__INIT_START"] = mr;
		mr.SetName(L"__INIT_SIZE");
		mr.SetAddress(_init_size);
		_memrefs[L"__INIT_SIZE"] = mr;


		// read CONST sections
		for(int32_t i = 0; i < _token_files.size(); i++)
		{
			int32_t size = 0;
			auto err = ReadSections(i, SectType::ST_CONST, _global_settings.GetROMStart() + _init_size + _const_size, size, _global_settings.GetROMSize() - _init_size);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}
			_const_size += size;

			if(_const_size + _init_size > _global_settings.GetROMSize())
			{
				_curr_file_name = _src_files[i];
				return A1STM8_T_ERROR::A1STM8_RES_EWSECSIZE;
			}
		}

		// .CONST sections size
		mr.SetName(L"__CONST_START");
		mr.SetAddress(_global_settings.GetROMStart() + _init_size);
		_memrefs[L"__CONST_START"] = mr;
		mr.SetName(L"__CONST_SIZE");
		mr.SetAddress(_const_size);
		_memrefs[L"__CONST_SIZE"] = mr;


		// read CODE sections
		for(int32_t i = 0; i < _token_files.size(); i++)
		{
			int32_t size = 0;
			auto err = ReadSections(i, SectType::ST_CODE, _global_settings.GetROMStart() + _init_size + _const_size + _code_size, size, _global_settings.GetROMSize() - _init_size - _const_size);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				return err;
			}
			_code_size += size;

			if(_code_size + _init_size + _const_size > _global_settings.GetROMSize())
			{
				_curr_file_name = _src_files[i];
				return A1STM8_T_ERROR::A1STM8_RES_EWSECSIZE;
			}
		}

		// .CODE sections size
		mr.SetName(L"__CODE_START");
		mr.SetAddress(_global_settings.GetROMStart() + _init_size + _const_size);
		_memrefs[L"__CODE_START"] = mr;
		mr.SetName(L"__CODE_SIZE");
		mr.SetAddress(_code_size);
		_memrefs[L"__CODE_SIZE"] = mr;
		mr.SetName(L"__CODE_TOTAL_SIZE");
		mr.SetAddress(_global_settings.GetROMSize());
		_memrefs[L"__CODE_TOTAL_SIZE"] = mr;


		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	A1STM8_T_ERROR Write(const std::string &file_name)
	{
		bool rel_out_range = false;
		int ror_line_num = 0;
		std::string ror_file_name;

		_curr_line_num = 0;
		_curr_file_name.clear();

		IhxWriter writer(file_name);

		auto err = writer.Open();
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			writer.Close();
			std::remove(file_name.c_str());
			return err;
		}

		err = writer.SetAddress(_global_settings.GetROMStart());
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			writer.Close();
			std::remove(file_name.c_str());
			return err;
		}

		for(const auto &s: *this)
		{
			_curr_file_name = s.GetFileName();

			int32_t size = 0;
			err = s.GetSize(size);
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				writer.Close();
				std::remove(file_name.c_str());
				_curr_line_num = s.GetCurrLineNum();
				return err;
			}

			if(s.GetType() == SectType::ST_INIT || s.GetType() == SectType::ST_CONST || s.GetType() == SectType::ST_CODE)
			{
				for(const auto &i: s)
				{
					err = i->Write(&writer, _memrefs);
					auto &ws = i->GetWarnings();
					for(auto &w: ws)
					{
						_warnings.insert(_warnings.end(), std::make_tuple(i->GetLineNum(), _curr_file_name, w));
					}

					if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
					{
						if(_global_settings.GetFixAddresses() && err == A1STM8_T_ERROR::A1STM8_RES_ERELOUTRANGE)
						{
							rel_out_range = true;
							ror_line_num = i->GetLineNum();
							ror_file_name = _curr_file_name;
							_instructions_to_replace.insert(std::make_pair(ror_line_num, _curr_file_name));
						}
						else
						{
							writer.Close();
							std::remove(file_name.c_str());
							_curr_line_num = i->GetLineNum();
							return err;
						}
					}
				}
			}
		}

		err = writer.Close();
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			std::remove(file_name.c_str());
			return err;
		}

		if(rel_out_range)
		{
			std::remove(file_name.c_str());
			_curr_line_num = ror_line_num;
			_curr_file_name = ror_file_name;
			return A1STM8_T_ERROR::A1STM8_RES_ERELOUTRANGE;
		}

		_curr_line_num = 0;
		_curr_file_name.clear();
		return A1STM8_T_ERROR::A1STM8_RES_OK;
	}

	int32_t GetCurrLineNum()
	{
		return _curr_line_num;
	}

	std::string GetCurrFileName()
	{
		return _curr_file_name;
	}

	const std::vector<std::tuple<int32_t, std::string, A1STM8_T_WARNING>> &GetWarnings() const
	{
		return _warnings;
	}

	int32_t GetVariablesSize() const
	{
		return _data_size;
	}

	int32_t GetStackSize() const
	{
		return _global_settings.GetStackSize();
	}

	int32_t GetHeapSize() const
	{
		return _global_settings.GetHeapSize();
	}

	int32_t GetConstSize() const
	{
		return _const_size;
	}

	int32_t GetCodeSize() const
	{
		return _code_size + _init_size;
	}
};


int main(int argc, char **argv)
{
	int i;
	bool print_err_desc = false;
	std::string ofn;
	bool print_version = false;
	std::string lib_dir;
	std::string MCU_name;
	const std::string target_name = "STM8";
	bool print_mem_use = false;
	std::vector<std::string> files;
	bool args_error = false;
	std::string args_error_txt;
	A1STM8_T_ERROR err;


	// set numeric properties from C locale for sprintf to use dot as decimal delimiter
	std::setlocale(LC_NUMERIC, "C");


	// read options and input file names
	for(i = 1; i < argc; i++)
	{
		if(files.empty())
		{
			// print error description
			if((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'D' || argv[i][1] == 'd') &&
				argv[i][2] == 0)
			{
				print_err_desc = true;
				continue;
			}

			// try to fix addresses (use JP instead of JRA/JRXX, CALL instead of CALLR)
			if ((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'F' || argv[i][1] == 'f') &&
				argv[i][2] == 0)
			{
				_global_settings.SetFixAddresses();
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
					lib_dir = argv[i];
				}

				continue;
			}

			// read MCU settings
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
					MCU_name = argv[i];
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

				continue;
			}

			// print memory usage
			if ((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'M' || argv[i][1] == 'm') &&
				(argv[i][2] == 'U' || argv[i][2] == 'u') &&
				argv[i][3] == 0)
			{
				print_mem_use = true;
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
					_global_settings.SetRAMSize(n);
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
					_global_settings.SetRAMStart(n);
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
					_global_settings.SetROMSize(n);
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
					_global_settings.SetROMStart(n);
				}

				continue;
			}

			// check target
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
					if(Utils::str_toupper(argv[i]) != "STM8")
					{
						args_error = true;
						args_error_txt = "invalid target";
					}
				}

				continue;
			}

			// print version
			if((argv[i][0] == '-' || argv[i][0] == '/') &&
				(argv[i][1] == 'V' || argv[i][1] == 'v') &&
				argv[i][2] == 0)
			{
				print_version = true;
				break;
			}
		}

		files.push_back(argv[i]);
	}

	if(args_error || files.empty())
	{
		b1_print_version(stderr);

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
		std::fputs(" [options] filename [filename1 filename2 ... filenameN]\n", stderr);
		std::fputs("options:\n", stderr);
		std::fputs("-d or /d - print error description\n", stderr);
		std::fputs("-l or /l - libraries directory, e.g. -l \"../lib\"\n", stderr);
		std::fputs("-m or /m - specify MCU name, e.g. -m STM8S103F3\n", stderr);
		std::fputs("-mu or /mu - print memory usage\n", stderr);
		std::fputs("-o or /o - specify output file name, e.g.: -o out.ihx\n", stderr);
		std::fputs("-ram_size or /ram_size - specify RAM size, e.g.: -ram_size 0x400\n", stderr);
		std::fputs("-ram_start or /ram_start - specify RAM starting address, e.g.: -ram_start 0\n", stderr);
		std::fputs("-rom_size or /rom_size - specify ROM size, e.g.: -rom_size 0x2000\n", stderr);
		std::fputs("-rom_start or /rom_start - specify ROM starting address, e.g.: -rom_start 0x8000\n", stderr);
		std::fputs("-t or /t - set target (default STM8), e.g.: -t STM8\n", stderr);
		std::fputs("-v or /v - show assembler version\n", stderr);
		return 1;
	}


	if(print_version)
	{
		// just print version and stop executing
		b1_print_version(stdout);
		return 0;
	}


	// read settings
	_global_settings.SetTargetName(target_name);
	_global_settings.SetMCUName(MCU_name);
	_global_settings.SetLibDir(lib_dir);

	// read settings file if specified
	if(!MCU_name.empty())
	{
		auto file_name = _global_settings.GetLibFileName(MCU_name, ".cfg");
		if(!file_name.empty())
		{
			auto err = static_cast<A1STM8_T_ERROR>(_global_settings.Read(file_name));
			if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
			{
				a1stm8_print_error(err, -1, file_name, print_err_desc);
				return 2;
			}
		}
		else
		{
			a1stm8_print_warning(A1STM8_T_WARNING::A1STM8_WRN_WUNKNMCU, -1, MCU_name, _global_settings.GetPrintWarningDesc());
		}
	}


	// prepare output file name
	if(ofn.empty())
	{
		ofn = files.front();
		auto delpos = ofn.find_last_of("\\/");
		auto pntpos = ofn.find_last_of('.');
		if(pntpos != std::string::npos && (delpos == std::string::npos || pntpos > delpos))
		{
			ofn.erase(pntpos, std::string::npos);
		}
		ofn += ".ihx";
	}

	// initialize instructions map
	load_all_instructions();

	if(_global_settings.GetFixAddresses())
	{
		if(_global_settings.GetMemModelSmall())
		{
			load_extra_instructions_small();
		}
		else
		{
			load_extra_instructions_large();
		}
	}

	Sections secs;

	err = secs.ReadSourceFiles(files);
	if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
	{
		if(_global_settings.GetPrintWarnings())
		{
			auto &ws = secs.GetWarnings();
			for(auto &w: ws)
			{
				a1stm8_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
			}
		}

		a1stm8_print_error(err, secs.GetCurrLineNum(), secs.GetCurrFileName(), print_err_desc);
		return 3;
	}

	while(true)
	{
		err = secs.ReadSections();
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			if(_global_settings.GetPrintWarnings())
			{
				auto &ws = secs.GetWarnings();
				for(auto &w: ws)
				{
					a1stm8_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
				}
			}

			a1stm8_print_error(err, secs.GetCurrLineNum(), secs.GetCurrFileName(), print_err_desc);
			return 4;
		}

		err = secs.Write(ofn);
		if(err == A1STM8_T_ERROR::A1STM8_RES_ERELOUTRANGE && _global_settings.GetFixAddresses())
		{
			continue;
		}
		else
		if(err != A1STM8_T_ERROR::A1STM8_RES_OK)
		{
			if(_global_settings.GetPrintWarnings())
			{
				auto &ws = secs.GetWarnings();
				for(auto &w: ws)
				{
					a1stm8_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
				}
			}

			a1stm8_print_error(err, secs.GetCurrLineNum(), secs.GetCurrFileName(), print_err_desc);
			return 5;
		}

		break;
	}

	if(_global_settings.GetPrintWarnings())
	{
		auto &ws = secs.GetWarnings();
		for(auto &w: ws)
		{
			a1stm8_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
		}
	}

	if(print_mem_use)
	{
		std::fwprintf(stdout, L"Memory usage:\n");
		std::fwprintf(stdout, L"Variables: %d (%ls kB)\n", (int)secs.GetVariablesSize(), get_size_kB(secs.GetVariablesSize()).c_str());
		std::fwprintf(stdout, L"Heap: %d (%ls kB)\n", (int)secs.GetHeapSize(), get_size_kB(secs.GetHeapSize()).c_str());
		std::fwprintf(stdout, L"Stack: %d (%ls kB)\n", (int)secs.GetStackSize(), get_size_kB(secs.GetStackSize()).c_str());
		std::fwprintf(stdout, L"Total RAM: %d (%ls kB)\n", (int)secs.GetVariablesSize() + secs.GetHeapSize() + secs.GetStackSize(), get_size_kB(secs.GetVariablesSize() + secs.GetHeapSize() + secs.GetStackSize()).c_str());
		std::fwprintf(stdout, L"Constants: %d (%ls kB)\n", (int)secs.GetConstSize(), get_size_kB(secs.GetConstSize()).c_str());
		std::fwprintf(stdout, L"Code: %d (%ls kB)\n", (int)secs.GetCodeSize(), get_size_kB(secs.GetCodeSize()).c_str());
		std::fwprintf(stdout, L"Total ROM: %d (%ls kB)\n", (int)(secs.GetConstSize() + secs.GetCodeSize()), get_size_kB(secs.GetConstSize() + secs.GetCodeSize()).c_str());
	}

	return 0;
}
