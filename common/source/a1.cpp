/*
 A1 assembler
 Copyright (c) 2021-2024 Nikolay Pletnev
 MIT license

 a1.cpp: basic assembler classes
*/

#include <sstream>
#include <iomanip>
#include <cwctype>
#include <memory>
#include <algorithm>
#include <cstring>

#include "a1.h"
#include "moresym.h"


A1_T_ERROR IhxWriter::WriteDataRecord(int32_t first_pos, int32_t last_pos)
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
			return A1_T_ERROR::A1_RES_EFWRITE;
		}
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR IhxWriter::WriteExtLinearAddress(uint32_t address)
{
	uint8_t chksum = 06;
	uint16_t addr16 = (uint16_t)((address) >> 16);

	chksum += (uint8_t)(addr16 >> 8);
	chksum += (uint8_t)addr16;
	if(std::fwprintf(_file, L":02000004%04x%02x\n", (int)addr16, (int)(uint8_t)(0 - chksum)) <= 0)
	{
		return A1_T_ERROR::A1_RES_EFWRITE;
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR IhxWriter::WriteEndOfFile()
{
	if(std::fwprintf(_file, L":00000001ff\n") <= 0)
	{
		return A1_T_ERROR::A1_RES_EFWRITE;
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR IhxWriter::Flush()
{
	int32_t first_pos, last_pos;

	if(_data_len <= 0)
	{
		return A1_T_ERROR::A1_RES_OK;
	}

	first_pos = 0;
	last_pos = _data_len - 1;

	if(_offset + _data_len > 0x10000)
	{
		auto write1 = 0x10000 - _offset;

		auto err = WriteDataRecord(first_pos, write1 - 1);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		first_pos = write1;
		_data_len -= write1;

		_base_addr += 0x10000;
		_offset = 0;
		err = WriteExtLinearAddress(_base_addr);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
	}

	auto err = WriteDataRecord(first_pos, last_pos);
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	_offset += _data_len;
	_data_len = 0;

	return A1_T_ERROR::A1_RES_OK;
}

IhxWriter::IhxWriter(const std::string &file_name)
: _file_name(file_name)
, _file(nullptr)
, _base_addr(0)
, _offset(0)
, _max_data_len(16)
, _data_len(0)
, _data{ 0 }
{
}

IhxWriter::~IhxWriter()
{
	Close();
}

A1_T_ERROR IhxWriter::Open()
{
	auto err = Close();
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	_file = std::fopen(_file_name.c_str(), "w+");
	if(_file == nullptr)
	{
		return A1_T_ERROR::A1_RES_EFOPEN;
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR IhxWriter::Open(const std::string &file_name)
{
	auto err = Close();
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	_file_name = file_name;
	return Open();
}

A1_T_ERROR IhxWriter::Write(const void *data, int32_t size)
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
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}
		}

		write1 = _max_data_len;
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR IhxWriter::SetAddress(uint32_t address)
{
	auto err = Flush();
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	if(address < _base_addr + _offset)
	{
		return A1_T_ERROR::A1_RES_EWADDR;
	}

	if((address & 0xFFFF0000) != _base_addr)
	{
		auto err = WriteExtLinearAddress(address);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
	}

	_base_addr = address & 0xFFFF0000;
	_offset = (uint16_t)address;

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR IhxWriter::Close()
{
	if(_file != nullptr)
	{
		auto err = Flush();
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		err = WriteEndOfFile();
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		if(std::fclose(_file) != 0)
		{
			_file = nullptr;
			return A1_T_ERROR::A1_RES_EFCLOSE;
		}
	}

	_file = nullptr;
	return A1_T_ERROR::A1_RES_OK;
}


void Token::MakeUpper()
{
	if(IsDir() || IsLabel() || IsString() || _toktype == TokType::TT_NUMBER)
	{
		_token = Utils::str_toupper(_token);
	}
}

A1_T_ERROR Token::QStringToString(const std::wstring &qstr, std::wstring &str)
{
	str.clear();

	for(auto ci = qstr.begin() + 1; ci != qstr.end() - 1; ci++)
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
				return A1_T_ERROR::A1_RES_ESYNTAX;
			}
		}

		str += c;
	}

	return A1_T_ERROR::A1_RES_OK;
}


const Token Token::DATA_DIR(TokType::TT_DIR, L".DATA", -1);
const Token Token::CONST_DIR(TokType::TT_DIR, L".CONST", -1);
const Token Token::CODE_DIR(TokType::TT_DIR, L".CODE", -1);
const Token Token::STACK_DIR(TokType::TT_DIR, L".STACK", -1);
const Token Token::HEAP_DIR(TokType::TT_DIR, L".HEAP", -1);

const Token Token::IF_DIR(TokType::TT_DIR, L".IF", -1);
const Token Token::ELIF_DIR(TokType::TT_DIR, L".ELIF", -1);
const Token Token::ELSE_DIR(TokType::TT_DIR, L".ELSE", -1);
const Token Token::ENDIF_DIR(TokType::TT_DIR, L".ENDIF", -1);

const Token Token::ERROR_DIR(TokType::TT_DIR, L".ERROR", -1);

const Token Token::DEF_DIR(TokType::TT_DIR, L".DEF", -1);


A1_T_ERROR SrcFile::ReadChar(wchar_t &chr)
{
	A1_T_ERROR err;
	wint_t c;

	err = A1_T_ERROR::A1_RES_OK;

	c = std::fgetwc(_file);
	if(c == WEOF)
	{
		if(std::feof(_file))
		{
			err = A1_T_ERROR::A1_RES_EEOF;
		}
		else
		{
			err = A1_T_ERROR::A1_RES_EFREAD;
		}
	}

	chr = c;

	return err;
}

A1_T_ERROR SrcFile::Open()
{
	Close();

	_file = std::fopen(_file_name.c_str(), "rt");
	if(_file == nullptr)
	{
		return A1_T_ERROR::A1_RES_EFOPEN;
	}

	_line_num = 1;
	return A1_T_ERROR::A1_RES_OK;
}

void SrcFile::Close()
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
A1_T_ERROR SrcFile::GetNextToken(Token &token)
{
	A1_T_ERROR err = A1_T_ERROR::A1_RES_OK;
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
			if(err == A1_T_ERROR::A1_RES_EEOF)
			{
				break;
			}
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}
		}

		if(c == L'\n')
		{
			if(tt == TokType::TT_QSTRING && qstr)
			{
				return A1_T_ERROR::A1_RES_ESYNTAX;
			}

			if(!tok.empty())
			{
				_saved_chr = c;
				break;
			}

			token = Token(TokType::TT_EOL, L"", _line_num);
			_line_num++;
			_skip_comment = false;
			return A1_T_ERROR::A1_RES_OK;
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
				return A1_T_ERROR::A1_RES_ESYNTAX;
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
				return A1_T_ERROR::A1_RES_ESYNTAX;
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
					return A1_T_ERROR::A1_RES_ESYNTAX;
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
				if(err == A1_T_ERROR::A1_RES_EEOF)
				{
					return A1_T_ERROR::A1_RES_ESYNTAX;
				}
				if(err != A1_T_ERROR::A1_RES_OK)
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
						return A1_T_ERROR::A1_RES_ESYNTAX;
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

	if(err == A1_T_ERROR::A1_RES_EEOF)
	{
		err = A1_T_ERROR::A1_RES_OK;
		if(tok.empty())
		{
			token = Token(TokType::TT_EOF, L"", _line_num);
			return A1_T_ERROR::A1_RES_OK;
		}
	}

	token = Token(tt, tok, _line_num);

	return err;
}


A1_T_ERROR MemRef::Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end)
{
	if(start == end)
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	if(start->GetType() != TokType::TT_LABEL)
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	auto tok = start->GetToken();
	auto line_num = start->GetLineNum();

	start++;
	if(start != end && start->GetType() != TokType::TT_EOL && start->GetType() != TokType::TT_EOF)
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
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
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	_line_num = line_num;

	return A1_T_ERROR::A1_RES_OK;
}


Section::Section(const std::string &file_name, int32_t sect_line_num, SectType st, const std::wstring &type_mod, int32_t address /*= -1*/)
: _type(st)
, _type_mod(type_mod)
, _address(address)
, _sect_line_num(sect_line_num)
, _curr_line_num(0)
, _file_name(file_name)
{
}

Section::Section(Section &&sect)
: std::list<GenStmt *>(std::move(sect))
, _type(std::move(sect._type))
, _type_mod(std::move(sect._type_mod))
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

const std::wstring &Section::GetTypeMod() const
{
	return _type_mod;
}

int32_t Section::GetAddress() const
{
	return _address;
}

void Section::SetAddress(int32_t address)
{
	_address = address;
}

A1_T_ERROR Section::GetSize(int32_t &size) const
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
			return A1_T_ERROR::A1_RES_EWSTMTSIZE;
		}

		osize += size1;
	}

	size = osize;

	_curr_line_num = 0;
	return A1_T_ERROR::A1_RES_OK;
}


EVal::EVal(int32_t val, USGN usgn /*= USGN::US_NONE*/)
: _resolved(true)
, _usgn(usgn)
{
	if(_usgn == USGN::US_MINUS)
	{
		_val = -val;
	}
	else
	if(_usgn == USGN::US_NOT)
	{
		_val = ~val;
	}
	else
	{
		_val = val;
	}
}

EVal::EVal(const std::wstring &symbol, USGN usgn)
: _val(-1)
, _resolved(false)
, _usgn(usgn)
{
	auto pos = symbol.rfind(L'.');
	if(pos != std::wstring::npos)
	{
		_postfix = symbol.substr(pos + 1);
		_symbol = symbol.substr(0, pos);
	}
	else
	{
		_symbol = symbol;
	}
}

A1_T_ERROR EVal::Resolve(const std::map<std::wstring, MemRef> &symbols /*= std::map<std::wstring, MemRef>()*/)
{
	if(_resolved)
	{
		return A1_T_ERROR::A1_RES_OK;
	}

	int32_t n;

	// process -2147483648 separately, because Utils::str2int32() function returns error on attempt
	// to parse positive "2147483648" string (numeric overflow)
	if(_usgn == EVal::USGN::US_MINUS && (_symbol == L"2147483648" || Utils::str_toupper(_symbol) == L"0X80000000") && _postfix.empty())
	{
		_val = INT32_MIN;
		_resolved = true;
		return A1_T_ERROR::A1_RES_OK;
	}

	if(std::iswdigit(_symbol[0]))
	{
		// numeric value
		auto err = Utils::str2int32(_symbol, n);
		if(err != B1_RES_OK)
		{
			return static_cast<A1_T_ERROR>(err);
		}
	}
	else
	{
		const auto &ref = symbols.find(_symbol);
		if(ref == symbols.end())
		{
			return A1_T_ERROR::A1_RES_EUNRESSYMB;
		}

		n = ref->second.GetAddress();
	}

	if(_usgn == USGN::US_MINUS)
	{
		n = -n;
	}
	else
	if(_usgn == USGN::US_NOT)
	{
		n = ~n;
	}

	if(!_postfix.empty())
	{
		if(_postfix.size() > 2)
		{
			return A1_T_ERROR::A1_RES_ESYNTAX;
		}

		if(_postfix[0] == L'l' || _postfix[0] == L'L')
		{
			n = (uint16_t)n;
		}
		else
		if(_postfix[0] == L'h' || _postfix[0] == L'H')
		{
			n = (uint16_t)(n >> 16);
		}
		else
		{
			return A1_T_ERROR::A1_RES_ESYNTAX;
		}

		if(_postfix.size() > 1)
		{
			if(_postfix[1] == L'l' || _postfix[1] == L'L')
			{
				n = (uint8_t)n;
			}
			else
			if(_postfix[1] == L'h' || _postfix[1] == L'H')
			{
				n = (uint8_t)(n >> 8);
			}
			else
			{
				return A1_T_ERROR::A1_RES_ESYNTAX;
			}
		}
	}
		
	_val = n;
	_resolved = true;

	return A1_T_ERROR::A1_RES_OK;
}


A1_T_ERROR Exp::BuildExp(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, Exp &exp, const std::vector<Token> &terms /*= std::vector<Token>()*/, const A1Settings *settings /*= nullptr*/)
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
					return A1_T_ERROR::A1_RES_ESYNTAX;
				}

				start++;
			}

			if(start != end && (start->GetType() == TokType::TT_NUMBER || start->GetType() == TokType::TT_STRING))
			{
				int32_t n = 0;

				if(start->GetType() == TokType::TT_NUMBER)
				{
					auto tok = start->GetToken();

					EVal val(tok, usgn);
					auto err = val.Resolve();
					if(err != A1_T_ERROR::A1_RES_OK)
					{
						return err;
					}

					exp.AddVal(val);
				}
				else
				{
					// resolve global constants
					auto tok = start->GetToken();
					std::wstring value;

					if(settings != nullptr && settings->GetValue(tok, value))
					{
						auto err = Utils::str2int32(value, n);
						if(err != B1_RES_OK)
						{
							exp.AddVal(EVal(value, usgn));
						}
						else
						{
							if(usgn == EVal::USGN::US_MINUS && n == INT32_MIN)
							{
								return A1_T_ERROR::A1_RES_ENUMOVF;
							}

							exp.AddVal(EVal(n, usgn));
						}
					}
					else
					if(_RTE_errors.find(tok) != _RTE_errors.end())
					{
						n = static_cast<std::underlying_type_t<B1C_T_RTERROR>>(_RTE_errors[tok]);
						exp.AddVal(EVal(n, usgn));
					}
					else
					if(_B1C_consts.find(tok) != _B1C_consts.end())
					{
						const auto &c = _B1C_consts[tok];
						n = c.first;
						exp.AddVal(EVal(n, usgn));
					}
					else
					{
						exp.AddVal(EVal(tok, usgn));
					}
				}
			}
			else
			{
				return A1_T_ERROR::A1_RES_ESYNTAX;
			}

			is_val = false;
		}
		else
		{
			if(start->GetType() != TokType::TT_OPER)
			{
				return A1_T_ERROR::A1_RES_ESYNTAX;
			}

			auto tok = start->GetToken();

			if(tok != L"+" && tok != L"-" && tok != L"*" && tok != L"/" && tok != L"%" && tok != L">>" && tok != L"<<" && tok != L"&" && tok != L"^" && tok != L"|")
			{
				return A1_T_ERROR::A1_RES_ESYNTAX;
			}

			exp.AddOp(tok);

			is_val = true;
		}
	}

	if(exp._vals.size() - 1 != exp._ops.size())
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Exp::CalcSimpleExp(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, int32_t &res, const std::vector<Token> &terms /*= std::vector<Token>()*/)
{
	Exp exp;

	auto err = BuildExp(start, end, exp, terms);
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	err = exp.Eval(res);

	return err;
}

A1_T_ERROR Exp::Eval(int32_t &res, const std::map<std::wstring, MemRef> &symbols /*= std::map<std::wstring, MemRef>()*/) const
{
	if(_vals.size() - 1 != _ops.size())
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	std::vector<std::wstring> ops = _ops;
	std::vector<EVal> vals = _vals;

	// resolve symbols
	for(auto &v: vals)
	{
		if(!v.IsResolved())
		{
			auto err = v.Resolve(symbols);
			if(err != A1_T_ERROR::A1_RES_OK)
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
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	res = vals[0].GetValue();

	return A1_T_ERROR::A1_RES_OK;
}


const ArgType ArgType::AT_NONE(-1, 0, 0);
const ArgType ArgType::AT_1BYTE_ADDR(1, 0, 0xFF); // 0..FF
const ArgType ArgType::AT_2BYTE_ADDR(2, 0, 0xFFFF); // 0..FFFF
const ArgType ArgType::AT_3BYTE_ADDR(3, 0, 0xFFFFFF); // 0..FFFFFF
const ArgType ArgType::AT_1BYTE_OFF(1, -128, 127); // -128..+127 (offset for JRx, CALLR instructions)
const ArgType ArgType::AT_1BYTE_VAL(1, -128, 255); // -128..255
const ArgType ArgType::AT_2BYTE_VAL(2, -32768, 65535); // -32768..65535


uint32_t Inst::get_hex_value(const std::wstring &str, int *len /*= nullptr*/)
{
	uint32_t val = 0;
	int bits = 0;
	wchar_t c;

	for(auto ci = str.cbegin(); ci != str.cend(); ci++)
	{
		c = *ci;

		val *= 16;
		bits += 4;

		if(std::isdigit((int)c))
		{
			val += (c - L'0');
		}
		else
		{
			c = std::towupper(c);
			if(c >= L'A' && c <= L'F')
			{
				val += (c - L'A' + 10);
			}
			else
			{
				throw std::invalid_argument("fatal: incorrect instruction definition (opcode)");
			}
		}
	}

	if(len != nullptr)
	{
		if(bits & 4)
		{
			bits += 4;
		}

		*len = bits;
	}

	return val;
}

uint32_t Inst::get_bit_arg(const std::wstring &bit_arg, int &start_pos, int &len)
{
	uint32_t val = 0;

	auto len_pos = bit_arg.rfind(L':');
	auto start_pos_pos = len_pos;
	if(len_pos != std::wstring::npos)
	{
		start_pos_pos = bit_arg.rfind(L':', len_pos - 1);
	}

	if(start_pos_pos == std::wstring::npos)
	{
		if(len_pos == std::wstring::npos)
		{
			// full value
			start_pos = -1;
			val = get_hex_value(bit_arg, &len);
		}
		else
		{
			// <value>:<length>
			val = get_hex_value(bit_arg.substr(0, len_pos));
			len = get_hex_value(bit_arg.substr(len_pos + 1));
			if(len > 16)
			{
				throw std::invalid_argument("fatal: incorrect instruction definition (length)");
			}
			start_pos = len - 1;
		}
	}
	else
	{
		// <value>:<start>:<length>
		val = get_hex_value(bit_arg.substr(0, start_pos_pos));
		start_pos = get_hex_value(bit_arg.substr(start_pos_pos + 1, len_pos - start_pos_pos - 1));
		len = get_hex_value(bit_arg.substr(len_pos + 1));
		if(len > 16)
		{
			throw std::invalid_argument("fatal: incorrect instruction definition (length)");
		}
	}

	return val;
}

Inst::Inst(const wchar_t *code, const ArgType &arg1type /*= ArgType::AT_NONE*/, const ArgType &arg2type /*= ArgType::AT_NONE*/, const ArgType &arg3type /*= ArgType::AT_NONE*/)
: _size(0)
, _argnum(0)
, _argtypes({ arg1type, arg2type, arg3type })
{
	for(_argnum = 0; _argnum < A1_MAX_INST_ARGS_NUM; _argnum++)
	{
		if(_argtypes[_argnum].get() == ArgType::AT_NONE)
		{
			break;
		}
	}

	std::vector<std::wstring> code_parts;
	Utils::str_split(code, L" ", code_parts);

	uint32_t val;
	int start, len;
	bool is_arg;

	for(const auto &cp: code_parts)
	{
		if(cp.front() == L'{')
		{
			if(cp.back() != L'}')
			{
				throw std::invalid_argument("fatal: incorrect instruction definition (arg)");
			}

			val = get_bit_arg(cp.substr(1, cp.length() - 2), start, len);
			if(val == 0 || val > _argnum)
			{
				throw std::invalid_argument("fatal: incorrect instruction definition (argnum)");
			}

			if(start == -1)
			{
				len = _argtypes[val - 1].get()._size * 8;
			}

			is_arg = true;
		}
		else
		{
			val = get_bit_arg(cp, start, len);
			is_arg = false;
		}

		_code.emplace_back(is_arg, val, start, len);
		_size += len;
	}

	if(_size % 8)
	{
		throw std::invalid_argument("fatal: incorrect instruction definition (size)");
	}

	_size /= 8;

	if(_size < 1)
	{
		throw std::invalid_argument("fatal: incorrect instruction definition (zero size)");
	}
}


bool DataStmt::IsDataStmt(const Token &token, int *data_size /*= nullptr*/)
{
	if(token.IsString())
	{
		const auto ts = token.GetToken();
		int size = -1;

		if(ts == L"DB")
		{
			size = 1;
		}
		else
		if(ts == L"DW")
		{
			size = 2;
		}
		else
		if(ts == L"DD")
		{
			size = 4;
		}

		if(data_size != nullptr)
		{
			*data_size = size;
		}

		return (size > 0);
	}

	return false;
}

A1_T_ERROR DataStmt::Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, const std::map<std::wstring, MemRef> &memrefs, const std::string &file_name, const A1Settings &settings)
{
	if(start == end)
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	if(start->GetType() != TokType::TT_STRING)
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	int32_t size1 = 0;

	if(!IsDataStmt(*start, &size1))
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	_line_num = start->GetLineNum();

	_size_specified = false;

	start++;
		
	if(start != end && start->GetType() == TokType::TT_OPER && start->GetToken() == L"(")
	{
		start++;

		int32_t rep = -1;
		Exp exp;

		auto err = Exp::BuildExp(start, end, exp, { Token(TokType::TT_OPER, L")", -1) }, &settings);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		err = exp.Eval(rep, memrefs);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
		if(start == end)
		{
			return A1_T_ERROR::A1_RES_ESYNTAX;
		}
		if(rep <= 0)
		{
			return A1_T_ERROR::A1_RES_EWBLKSIZE;
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

	return A1_T_ERROR::A1_RES_OK;
}


A1_T_ERROR ConstStmt::Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, const std::map<std::wstring, MemRef> &memrefs, const std::string &file_name, const A1Settings &settings)
{
	auto err = DataStmt::Read(start, end, memrefs, file_name, settings);
	if(err != A1_T_ERROR::A1_RES_OK)
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
			std::wstring str;
			err = Token::QStringToString(start->GetToken(), str);
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}

			char mbc[6];

			for(const auto c: str)
			{
				if(_size1 == 4)
				{
					_data.push_back((uint8_t)(((uint32_t)c) >> 24));
					_data.push_back((uint8_t)(((uint32_t)c) >> 16));
					_data.push_back((uint8_t)(((uint16_t)c) >> 8));
					_data.push_back((uint8_t)c);
				}
				else
				if(_size1 == 2)
				{
					_data.push_back((uint8_t)(((uint16_t)c) >> 8));
					_data.push_back((uint8_t)c);
				}
				else
				if(c > 127)
				{
					// non-ASCII character
					mbstate_t mbs{};
					auto n = std::wcrtomb(mbc, c, &mbs);
					if(n == static_cast<std::size_t>(-1))
					{
						_warnings.push_back(A1_T_WARNING::A1_WRN_WBADWCHAR);
						mbc[0] = '?';
					}
					else
					if(n != 1)
					{
						_warnings.push_back(A1_T_WARNING::A1_WRN_WNONANSICHAR);
						mbc[0] = '?';
					}

					_data.push_back((uint8_t)mbc[0]);
				}
				else
				{
					_data.push_back((uint8_t)c);
				}
			}

			start++;
		}
		else
		{
			int32_t num = 0;
			Exp exp;

			auto err = Exp::BuildExp(start, end, exp, terms, &settings);
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}

			err = exp.Eval(num);
			if(err != A1_T_ERROR::A1_RES_OK && err != A1_T_ERROR::A1_RES_EUNRESSYMB)
			{
				return err;
			}

			if(err == A1_T_ERROR::A1_RES_EUNRESSYMB)
			{
				// save expression to evaluate later
				_exps.push_back({ (int32_t)_data.size(), exp });
			}

			if(_size1 == 4)
			{
				_data.push_back((uint8_t)(num >> 24));
				_data.push_back((uint8_t)(num >> 16));
			}

			if(_size1 >= 2)
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

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR ConstStmt::Write(IhxWriter *writer, const std::map<std::wstring, MemRef> &memrefs)
{
	// evaluate expressions if any
	for(auto &exp: _exps)
	{
		int32_t val = 0;
		auto err = exp.second.Eval(val, memrefs);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		auto i = exp.first;

		if(_size1 == 4)
		{
			_data[i++] = ((uint8_t)(val >> 24));
			_data[i++] = ((uint8_t)(val >> 16));
		}

		if(_size1 >= 2)
		{
			_data[i++] = (((uint16_t)val) >> 8);
		}

		_data[i] = ((uint8_t)val);
	}

	if(_truncated)
	{
		// data truncated
		_warnings.push_back(A1_T_WARNING::A1_WRN_WDATATRUNC);
	}

	auto err = writer->Write(_data.data(), _size);
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR CodeStmt::GetRefValue(const std::pair<std::reference_wrapper<const ArgType>, Exp> &ref, const std::map<std::wstring, MemRef> &memrefs, uint32_t &value, int &size)
{
	int32_t addr = 0;

	auto err = ref.second.Eval(addr, memrefs);
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	if(ref.first.get() == ArgType::AT_1BYTE_OFF)
	{
		addr = addr - _address - _size;
	}

	size = ref.first.get()._size;
	if(!ref.first.get().IsValidValue(addr))
	{
		if(ref.first.get() == ArgType::AT_1BYTE_OFF)
		{
			return A1_T_ERROR::A1_RES_ERELOUTRANGE;
		}
		_warnings.push_back(A1_T_WARNING::A1_WRN_WINTOUTRANGE);
	}

	value = addr;

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR CodeStmt::ReadInstArg(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, std::wstring &argsign, const A1Settings &settings)
{
	std::wstring tok;
	std::vector<std::wstring> brackets;

	if(start == end)
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
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
				return A1_T_ERROR::A1_RES_ESYNTAX;
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

			auto err = Exp::BuildExp(start, end, exp, terms, &settings);
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}

			// check if the expression is a register
			std::wstring exp_sign;
			err = GetExpressionSignature(exp, exp_sign);
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}

			if(!exp.IsEmpty())
			{
				_refs.emplace_back(ArgType::AT_NONE, exp);
			}
			argsign += exp_sign;

			continue;
		}

		argsign += start->GetToken();
		start++;
	}

	if(!brackets.empty())
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR CodeStmt::Write(IhxWriter *writer, const std::map<std::wstring, MemRef> &memrefs)
{
	if(!_is_inst)
	{
		return ConstStmt::Write(writer, memrefs);
	}

	uint64_t bits = 0;
	int bit_num = 0;

	bool is_arg;
	uint32_t code;
	int start;
	int len;

	for(auto &cp: _inst->_code)
	{
		is_arg = std::get<0>(cp);
		code = std::get<1>(cp);
		start = std::get<2>(cp);
		len = std::get<3>(cp);

		if(is_arg)
		{
			int size = 0;
			auto err = GetRefValue(_refs[code - 1], memrefs, code, size);
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}

			if(start < 0)
			{
				len = size * 8;
			}
		}

		if(start < 0)
		{
			code <<= (32 - len);
		}
		else
		{
			code <<= (32 - start - 1);
		}

		bits |= ((uint64_t)code) << (32 - bit_num);
		bit_num += len;

		for(; bit_num >= 8; bit_num -= 8)
		{
			_data.push_back((uint8_t)(bits >> 56));
			bits <<= 8;
		}
	}

	if(bit_num != 0)
	{
		return A1_T_ERROR::A1_RES_EINTERR;
	}

	auto err = writer->Write(_data.data(), _data.size());
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	return A1_T_ERROR::A1_RES_OK;
}


A1_T_ERROR Sections::ReadStmt(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end)
{
	auto stype = back().GetType();

	if(start->IsLabel())
	{
		MemRef mr;

		auto err = mr.Read(start, end);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		int32_t ssize = 0;
		err = back().GetSize(ssize);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		mr.SetAddress(back().GetAddress() + ssize);

		auto refn = mr.GetName();

		if(_memrefs.find(refn) != _memrefs.cend())
		{
			return A1_T_ERROR::A1_RES_EDUPSYM;
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
		std::unique_ptr<GenStmt> stmt(CreateNewStmt(stype, back().GetTypeMod()));
		if(stmt == nullptr)
		{
			return A1_T_ERROR::A1_RES_EWSECNAME;
		}

		auto err = stmt->Read(start, end, _memrefs, _curr_file_name, _settings);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		int32_t ssize = 0;
		err = back().GetSize(ssize);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		stmt->SetAddress(back().GetAddress() + ssize);

		back().push_back(stmt.release());
	}
	else
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::CheckIFDir(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, bool &res)
{
	int32_t resl = 0, resr = 0;
	bool not_def = false;

	start++;

	if(start != end && start->IsString() && start->GetToken() == L"NOT")
	{
		not_def = true;
		start++;
	}

	if(start != end && start->IsString() && start->GetToken() == L"DEFINED")
	{
		start++;
		if(start == end || *start != Token(TokType::TT_OPER, L"(", -1))
		{
			A1_T_ERROR::A1_RES_ESYNTAX;
		}

		start++;
		if(start == end || !start->IsString())
		{
			A1_T_ERROR::A1_RES_ESYNTAX;
		}

		const auto symbol = start->GetToken();

		start++;
		if(start == end || *start != Token(TokType::TT_OPER, L")", -1))
		{
			A1_T_ERROR::A1_RES_ESYNTAX;
		}

		start++;
		if(start != end && !start->IsEOL())
		{
			A1_T_ERROR::A1_RES_ESYNTAX;
		}

		res = (_memrefs.find(symbol) != _memrefs.cend());

		if(not_def)
		{
			res = !res;
		}

		return A1_T_ERROR::A1_RES_OK;
	}

	if(not_def)
	{
		A1_T_ERROR::A1_RES_ESYNTAX;
	}

	Exp exp_l, exp_r;
	std::wstring sval_l, sval_r;
	std::vector<Token> terms;

	terms.push_back(Token(TokType::TT_OPER, L"==", -1));
	terms.push_back(Token(TokType::TT_OPER, L"!=", -1));
	terms.push_back(Token(TokType::TT_OPER, L">", -1));
	terms.push_back(Token(TokType::TT_OPER, L"<", -1));
	terms.push_back(Token(TokType::TT_OPER, L">=", -1));
	terms.push_back(Token(TokType::TT_OPER, L"<=", -1));

	auto err = Exp::BuildExp(start, end, exp_l, terms, &_settings);
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	err = exp_l.Eval(resl, _memrefs);
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		// allow simple expressions like ".IF SOMETEXT == SOMETEXT"
		if(err != A1_T_ERROR::A1_RES_EUNRESSYMB || !exp_l.GetSimpleValue(sval_l))
		{
			return err;
		}
		if(sval_l.empty())
		{
			return A1_T_ERROR::A1_RES_EUNRESSYMB;
		}
	}

	const auto &cmp_op = *start;
	start++;

	terms.clear();
	terms.push_back(Token(TokType::TT_EOL, L"", -1));
		
	err = Exp::BuildExp(start, end, exp_r, terms, &_settings);
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	err = exp_r.Eval(resr, _memrefs);
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		// allow simple expressions like ".IF SOMETEXT == SOMETEXT"
		if(err != A1_T_ERROR::A1_RES_EUNRESSYMB || !exp_r.GetSimpleValue(sval_r))
		{
			return err;
		}
		if(sval_r.empty())
		{
			return A1_T_ERROR::A1_RES_EUNRESSYMB;
		}
	}

	// allow simple expressions like ".IF SOMETEXT == SOMETEXT"
	if(!sval_l.empty())
	{
		if(cmp_op.GetToken() == L"==")
		{
			res = (sval_l == sval_r);
		}
		else
		if(cmp_op.GetToken() == L"!=")
		{
			res = (sval_l != sval_r);
		}
		else
		{
			return A1_T_ERROR::A1_RES_ESYNTAX;
		}
	}
	else
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
	if(cmp_op.GetToken() == L">")
	{
		res = (resl > resr);
	}
	else
	if(cmp_op.GetToken() == L"<")
	{
		res = (resl < resr);
	}
	else
	if(cmp_op.GetToken() == L">=")
	{
		res = (resl >= resr);
	}
	else
	if(cmp_op.GetToken() == L"<=")
	{
		res = (resl <= resr);
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::ReadUntil(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, std::vector<std::reference_wrapper<const Token>>::const_iterator stop_dirs_start, std::vector<std::reference_wrapper<const Token>>::const_iterator stop_dirs_end)
{
	while(start != end)
	{
		if(start->IsEOL())
		{
			start++;
			continue;
		}

		_curr_line_num = start->GetLineNum();

		if(start->IsDir())
		{
			for(auto sdi = stop_dirs_start; sdi != stop_dirs_end; sdi++)
			{
				if(sdi->get() == *start)
				{
					return A1_T_ERROR::A1_RES_OK;
				}
			}

			if(*start == Token::DEF_DIR)
			{
				start++;
				if(start == end || start->GetType() != TokType::TT_STRING)
				{
					A1_T_ERROR::A1_RES_ESYNTAX;
				}

				const auto symbol = start->GetToken();

				if(_memrefs.find(symbol) != _memrefs.cend())
				{
					return A1_T_ERROR::A1_RES_EDUPSYM;
				}

				MemRef mr;
				mr.SetName(symbol);
				mr.SetAddress(0);

				start++;
				if(start != end && !start->IsEOL())
				{
					Exp exp;
					int32_t res = 0;

					auto err = Exp::BuildExp(start, end, exp, { Token(TokType::TT_EOL, L"", -1) }, &_settings);
					if(err != A1_T_ERROR::A1_RES_OK)
					{
						return err;
					}

					err = exp.Eval(res, _memrefs);
					if(err != A1_T_ERROR::A1_RES_OK)
					{
						return err;
					}

					mr.SetAddress(res);
				}

				_memrefs.emplace(std::make_pair(symbol, mr));

				continue;
			}
			else
			if(*start == Token::ERROR_DIR)
			{
				start++;
				if(start == end || start->GetType() != TokType::TT_QSTRING)
				{
					A1_T_ERROR::A1_RES_ESYNTAX;
				}

				std::wstring str;
				auto err = Token::QStringToString(start->GetToken(), str);
				if(err != A1_T_ERROR::A1_RES_OK)
				{
					return err;
				}
				_custom_err_msg = Utils::wstr2str(str);

				start++;
				if(start != end && start->IsEOL())
				{
					start++;
				}

				return A1_T_ERROR::A1_RES_EERRDIR;
			}
		}

		auto err = ReadStmt(start, end);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		if(start != end)
		{
			start++;
		}
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::SkipUntil(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, std::vector<std::reference_wrapper<const Token>>::const_iterator stop_dirs_start, std::vector<std::reference_wrapper<const Token>>::const_iterator stop_dirs_end)
{
	while(start != end)
	{
		if(start->IsEOL())
		{
			start++;
			continue;
		}

		_curr_line_num = start->GetLineNum();

		if(start->IsDir())
		{
			for(auto sdi = stop_dirs_start; sdi != stop_dirs_end; sdi++)
			{
				if(sdi->get() == *start)
				{
					return A1_T_ERROR::A1_RES_OK;
				}
			}
		}

		start++;
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::ReadSingleIFDir(bool is_else, std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, bool &skip)
{
	bool if_res = false;

	if(!skip)
	{
		if(is_else)
		{
			if_res = true;
			start++;
		}
		else
		{
			auto err = CheckIFDir(start, end, if_res);
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}
		}
	}
	else
	{
		for(; start != end && !start->IsEOL(); start++);
	}

	while(start != end)
	{
		auto err = skip || !if_res ?
			//                    .IF, .ELIF, .ELSE, .ENDIF
			SkipUntil(start, end, std::next(Sections::ALL_DIRS.cbegin(), 5), std::next(Sections::ALL_DIRS.cbegin(), 9)) :
			ReadUntil(start, end, std::next(Sections::ALL_DIRS.cbegin(), 5), std::next(Sections::ALL_DIRS.cbegin(), 9));
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
		if(start == end)
		{
			return A1_T_ERROR::A1_RES_ESYNTAX;
		}

		if(*start == Token::IF_DIR)
		{
			err = ReadIFDir(start, end, skip || !if_res);
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}
		}

		if(is_else)
		{
			if(*start == Token::ELIF_DIR || *start == Token::ELSE_DIR)
			{
				return A1_T_ERROR::A1_RES_ESYNTAX;
			}
		}

		if(*start == Token::ENDIF_DIR || *start == Token::ELIF_DIR || *start == Token::ELSE_DIR)
		{
			break;
		}
	}

	if(start == end)
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	if(if_res)
	{
		skip = true;
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::ReadIFDir(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, bool skip)
{
	auto err = ReadSingleIFDir(false, start, end, skip);
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	while(*start == Token::ELIF_DIR)
	{
		auto err = ReadSingleIFDir(false, start, end, skip);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
	}

	if(*start == Token::ELSE_DIR)
	{
		auto err = ReadSingleIFDir(true, start, end, skip);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
	}

	if(*start != Token::ENDIF_DIR)
	{
		return A1_T_ERROR::A1_RES_ESYNTAX;
	}

	start++;

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::ReadSection(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, bool skip)
{
	while(start != end)
	{
		auto err = skip ? 
			//                    .DATA, .CONST, .CODE, .STACK, .HEAP, .IF
			SkipUntil(start, end, Sections::ALL_DIRS.cbegin(), std::next(Sections::ALL_DIRS.cbegin(), 6)) :
			ReadUntil(start, end, Sections::ALL_DIRS.cbegin(), std::next(Sections::ALL_DIRS.cbegin(), 6));
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		if(start == end)
		{
			break;
		}

		if(*start == Token::IF_DIR)
		{
			err = ReadIFDir(start, end, skip);
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}
		}
		else
		{
			break;
		}
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::ReadSections(int32_t file_num, SectType sec_type, const std::wstring &type_mod, int32_t sec_base, int32_t &over_size, int32_t max_size)
{
	Section *psec = nullptr;

	over_size = 0;

	_curr_file_name = _src_files[file_num];
	_curr_line_num = 0;

	const auto &tok_file = _token_files[file_num];

	auto ti = tok_file.cbegin();

	while(ti != tok_file.cend() && !ti->IsDir())
	{
		_curr_line_num = ti->GetLineNum();

		if(!ti->IsEOL())
		{
			return A1_T_ERROR::A1_RES_ESYNTAX;
		}
		ti++;
	}

	if(ti->IsDir())
	{
		// allow program start from section declaration directive only
		for(auto d = std::next(ALL_DIRS.cbegin(), 5); d != ALL_DIRS.cend(); d++)
		{
			if(*ti == *d)
			{
				_curr_line_num = ti->GetLineNum();
				return A1_T_ERROR::A1_RES_ESYNTAX;
			}
		}
	}

	while(ti != tok_file.cend())
	{
		// find the next section start
		//                    .DATA, .CONST, .CODE, .STACK, .HEAP
		auto err = SkipUntil(ti, tok_file.cend(), Sections::ALL_DIRS.cbegin(), std::next(Sections::ALL_DIRS.cbegin(), 5));
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		if(ti == tok_file.cend())
		{
			break;
		}

		SectType st =
			*ti == Token::DATA_DIR	?	SectType::ST_DATA :
			*ti == Token::CONST_DIR	?	SectType::ST_CONST :
			*ti == Token::CODE_DIR	?	SectType::ST_CODE :
			*ti == Token::STACK_DIR	?	SectType::ST_STACK :
			*ti == Token::HEAP_DIR	?	SectType::ST_HEAP :
										SectType::ST_NONE;
		if(st == SectType::ST_NONE)
		{
			return A1_T_ERROR::A1_RES_EWSECNAME;
		}

		std::wstring sec_mod;

		ti++;

		if(!(ti == tok_file.cend() || ti->IsEOL()))
		{
			sec_mod = ti->GetToken();

			ti++;

			// the only built-in section type modifier now
			if(st == SectType::ST_CODE && sec_mod == L"INIT")
			{
				st = SectType::ST_INIT;
				sec_mod.clear();
			}
		}

		if(!CheckSectionName(st, sec_mod))
		{
			return A1_T_ERROR::A1_RES_EWSECNAME;
		}

		if(psec != nullptr && !(psec->GetType() == SectType::ST_STACK || psec->GetType() == SectType::ST_HEAP))
		{
			int32_t size = 0;
			auto err = psec->GetSize(size);
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}

			over_size += size;
			if(over_size > max_size)
			{
				return A1_T_ERROR::A1_RES_EWSECSIZE;
			}
		}

		psec = nullptr;

		if(st == sec_type && sec_mod == type_mod)
		{
			push_back(Section(_curr_file_name, _curr_line_num, st, sec_mod, sec_base + over_size));
			psec = &back();
		}

		if(!(ti == tok_file.cend() || ti->IsEOL()))
		{
			return A1_T_ERROR::A1_RES_ESYNTAX;
		}

		if(ti != tok_file.cend())
		{
			ti++;
		}

		if(psec == nullptr)
		{
			// skip the section
			continue;
		}

		// read the section
		err = ReadSection(ti, tok_file.cend(), false);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
	}

	if(psec != nullptr && !(psec->GetType() == SectType::ST_STACK || psec->GetType() == SectType::ST_HEAP))
	{
		int32_t size = 0;
		auto err = psec->GetSize(size);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		over_size += size;
		if(over_size > max_size)
		{
			return A1_T_ERROR::A1_RES_EWSECSIZE;
		}
	}

	_curr_file_name.clear();
	_curr_line_num = 0;

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::ReadSourceFiles(const std::vector<std::string> &src_files)
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
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		_token_files.push_back(std::vector<Token>());
		Token tok;

		while(true)
		{
			err = file.GetNextToken(tok);
			if(err != A1_T_ERROR::A1_RES_OK)
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
	return A1_T_ERROR::A1_RES_OK;
}

void Sections::Clear()
{
	clear();

	_memrefs.clear();
	_warnings.clear();

	_data_size = 0;
	_init_size = 0;
	_const_size = 0;
	_code_size = 0;
}

A1_T_ERROR Sections::ReadHeapSections()
{
	// read .HEAP section
	auto first_sec_num = size();

	for(int32_t i = 0; i < _token_files.size(); i++)
	{
		int32_t hs = 0;
		auto err = ReadSections(i, SectType::ST_HEAP, L"", 0, hs, 0);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
	}

	if(size() == first_sec_num + 1)
	{
		int32_t hs = 0;
		auto err = at(first_sec_num).GetSize(hs);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			_curr_file_name = at(first_sec_num).GetFileName();
			return err;
		}

		if(hs > _settings.GetRAMSize())
		{
			_curr_file_name = at(first_sec_num).GetFileName();
			return A1_T_ERROR::A1_RES_EWSECSIZE;
		}

		_settings.SetHeapSize(hs);
	}
	else
	if(size() > first_sec_num + 1)
	{
		int32_t hs = 0;

		for(auto i = first_sec_num; i < size(); i++)
		{
			int32_t hs1;
			auto err = at(i).GetSize(hs1);
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				_curr_file_name = at(i).GetFileName();
				return err;
			}

			hs = hs1 > hs ? hs1 : hs;

			_warnings.push_back(std::make_tuple(at(i).GetSectLineNum(), at(i).GetFileName(), A1_T_WARNING::A1_WRN_WMANYHPSECT));
			if(hs > _settings.GetRAMSize())
			{
				_curr_file_name = at(i).GetFileName();
				return A1_T_ERROR::A1_RES_EWSECSIZE;
			}
		}

		_settings.SetHeapSize(hs);
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::ReadStackSections()
{
	// read .STACK section
	auto first_sec_num = size();

	for(int32_t i = 0; i < _token_files.size(); i++)
	{
		int32_t ss = 0;
		auto err = ReadSections(i, SectType::ST_STACK, L"", 0, ss, 0);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
	}

	if(size() == first_sec_num + 1)
	{
		int32_t ss = 0;
		auto err = at(first_sec_num).GetSize(ss);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		if(_settings.GetHeapSize() + ss > _settings.GetRAMSize())
		{
			_curr_file_name = at(first_sec_num).GetFileName();
			return A1_T_ERROR::A1_RES_EWSECSIZE;
		}

		_settings.SetStackSize(ss);
	}
	else
	if(size() > first_sec_num + 1)
	{
		int32_t ss = 0;

		for(auto i = first_sec_num; i < size(); i++)
		{
			int32_t ss1;
			auto err = at(i).GetSize(ss1);
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}

			ss = ss1 > ss ? ss1 : ss;

			_warnings.push_back(std::make_tuple(at(i).GetSectLineNum(), at(i).GetFileName(), A1_T_WARNING::A1_WRN_WMANYSTKSECT));
			if(_settings.GetHeapSize() + ss > _settings.GetRAMSize())
			{
				_curr_file_name = at(i).GetFileName();
				return A1_T_ERROR::A1_RES_EWSECSIZE;
			}
		}

		_settings.SetStackSize(ss);
	}

	// add special symbols
	MemRef mr;

	mr.SetName(L"__RET_ADDR_SIZE");
	mr.SetAddress(_settings.GetRetAddressSize());
	_memrefs[L"__RET_ADDR_SIZE"] = mr;

	// add .STACK section symbols
	mr.SetName(L"__STACK_START");
	mr.SetAddress(_settings.GetRAMStart() + (_settings.GetRAMSize() - _settings.GetStackSize()));
	_memrefs[L"__STACK_START"] = mr;
	mr.SetName(L"__STACK_SIZE");
	mr.SetAddress(_settings.GetStackSize());
	_memrefs[L"__STACK_SIZE"] = mr;

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::ReadDataSections()
{
	// read .DATA sections
	for(int32_t i = 0; i < _token_files.size(); i++)
	{
		int32_t size = 0;
		auto err = ReadSections(i, SectType::ST_DATA, L"", _settings.GetRAMStart() + _data_size, size, _settings.GetRAMSize() - _data_size - _settings.GetHeapSize());
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
		_data_size += size;

		if(_data_size + _settings.GetHeapSize() > _settings.GetRAMSize())
		{
			_curr_file_name = _src_files[i];
			return A1_T_ERROR::A1_RES_EWSECSIZE;
		}

		if(_data_size + _settings.GetHeapSize() + _settings.GetStackSize() > _settings.GetRAMSize())
		{
			_warnings.push_back(std::make_tuple(-1, _src_files[i], A1_T_WARNING::A1_WRN_EWNORAM));
		}
	}

	MemRef mr;
	// add .HEAP section symbols
	mr.SetName(L"__HEAP_START");
	mr.SetAddress(_settings.GetRAMStart() + _data_size);
	_memrefs[L"__HEAP_START"] = mr;
	mr.SetName(L"__HEAP_SIZE");
	mr.SetAddress(_settings.GetHeapSize());
	_memrefs[L"__HEAP_SIZE"] = mr;

	// add .DATA sections symbols
	mr.SetName(L"__DATA_START");
	mr.SetAddress(_settings.GetRAMStart());
	_memrefs[L"__DATA_START"] = mr;
	mr.SetName(L"__DATA_SIZE");
	mr.SetAddress(_data_size);
	_memrefs[L"__DATA_SIZE"] = mr;
	mr.SetName(L"__DATA_TOTAL_SIZE");
	mr.SetAddress(_settings.GetRAMSize());
	_memrefs[L"__DATA_TOTAL_SIZE"] = mr;

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::ReadCodeInitSections()
{
	// read .CODE INIT sections
	auto first_sec_num = size();

	for(int32_t i = 0; i < _token_files.size(); i++)
	{
		int32_t size = 0;
		auto err = ReadSections(i, SectType::ST_INIT, L"", _settings.GetROMStart() + _init_size, size, _settings.GetROMSize());
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
		_init_size += size;

		if(_init_size > _settings.GetROMSize())
		{
			_curr_file_name = _src_files[i];
			return A1_T_ERROR::A1_RES_EWSECSIZE;
		}
	}

	if(size() > first_sec_num + 1)
	{
		for(auto i = first_sec_num; i < size(); i++)
		{
			_warnings.push_back(std::make_tuple(at(i).GetSectLineNum(), at(i).GetFileName(), A1_T_WARNING::A1_WRN_WMANYCODINIT));
		}
	}

	// add .CODE INIT sections symbols
	MemRef mr;
	mr.SetName(L"__INIT_START");
	mr.SetAddress(_settings.GetROMStart());
	_memrefs[L"__INIT_START"] = mr;
	mr.SetName(L"__INIT_SIZE");
	mr.SetAddress(_init_size);
	_memrefs[L"__INIT_SIZE"] = mr;

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::ReadConstSections()
{
	// read .CONST sections
	for(int32_t i = 0; i < _token_files.size(); i++)
	{
		int32_t size = 0;
		auto err = ReadSections(i, SectType::ST_CONST, L"", _settings.GetROMStart() + _init_size + _const_size, size, _settings.GetROMSize() - _init_size);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
		_const_size += size;

		if(_const_size + _init_size > _settings.GetROMSize())
		{
			_curr_file_name = _src_files[i];
			return A1_T_ERROR::A1_RES_EWSECSIZE;
		}
	}

	// add .CONST sections symbols
	MemRef mr;
	mr.SetName(L"__CONST_START");
	mr.SetAddress(_settings.GetROMStart() + _init_size);
	_memrefs[L"__CONST_START"] = mr;
	mr.SetName(L"__CONST_SIZE");
	mr.SetAddress(_const_size);
	_memrefs[L"__CONST_SIZE"] = mr;

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::ReadCodeSections()
{
	// read .CODE sections
	for(int32_t i = 0; i < _token_files.size(); i++)
	{
		int32_t size = 0;
		auto err = ReadSections(i, SectType::ST_CODE, L"", _settings.GetROMStart() + _init_size + _const_size + _code_size, size, _settings.GetROMSize() - _init_size - _const_size);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}
		_code_size += size;

		if(_code_size + _init_size + _const_size > _settings.GetROMSize())
		{
			_curr_file_name = _src_files[i];
			return A1_T_ERROR::A1_RES_EWSECSIZE;
		}
	}

	// add .CODE sections symbols
	MemRef mr;
	mr.SetName(L"__CODE_START");
	mr.SetAddress(_settings.GetROMStart() + _init_size + _const_size);
	_memrefs[L"__CODE_START"] = mr;
	mr.SetName(L"__CODE_SIZE");
	mr.SetAddress(_code_size);
	_memrefs[L"__CODE_SIZE"] = mr;
	mr.SetName(L"__CODE_TOTAL_SIZE");
	mr.SetAddress(_settings.GetROMSize());
	_memrefs[L"__CODE_TOTAL_SIZE"] = mr;

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::ReadSections()
{
	Clear();

	// read .HEAP section
	auto err = ReadHeapSections();
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	// read .STACK section
	err = ReadStackSections();
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	// read .DATA sections
	err = ReadDataSections();
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	// read .CODE INIT sections
	err = ReadCodeInitSections();
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	// read .CONST sections
	err = ReadConstSections();
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	// read .CODE sections
	err = ReadCodeSections();
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		return err;
	}

	return A1_T_ERROR::A1_RES_OK;
}

A1_T_ERROR Sections::Write(const std::string &file_name)
{
	bool rel_out_range = false;
	int ror_line_num = 0;
	std::string ror_file_name;

	_curr_line_num = 0;
	_curr_file_name.clear();

	IhxWriter writer(file_name);

	auto err = writer.Open();
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		writer.Close();
		std::remove(file_name.c_str());
		return err;
	}

	err = writer.SetAddress(_settings.GetROMStart());
	if(err != A1_T_ERROR::A1_RES_OK)
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
		if(err != A1_T_ERROR::A1_RES_OK)
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

				if(err != A1_T_ERROR::A1_RES_OK)
				{
					if(_settings.GetFixAddresses() && err == A1_T_ERROR::A1_RES_ERELOUTRANGE)
					{
						rel_out_range = true;
						ror_line_num = i->GetLineNum();
						ror_file_name = _curr_file_name;
						_settings.AddInstToReplace(ror_line_num, _curr_file_name);
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
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		std::remove(file_name.c_str());
		return err;
	}

	if(rel_out_range)
	{
		std::remove(file_name.c_str());
		_curr_line_num = ror_line_num;
		_curr_file_name = ror_file_name;
		return A1_T_ERROR::A1_RES_ERELOUTRANGE;
	}

	_curr_line_num = 0;
	_curr_file_name.clear();
	return A1_T_ERROR::A1_RES_OK;
}


// the order of references is important
const std::vector<std::reference_wrapper<const Token>> Sections::ALL_DIRS =
{
	Token::DATA_DIR,
	Token::CONST_DIR,
	Token::CODE_DIR,
	Token::STACK_DIR,
	Token::HEAP_DIR,

	Token::IF_DIR,
	Token::ELIF_DIR,
	Token::ELSE_DIR,
	Token::ENDIF_DIR,

	Token::ERROR_DIR,

	Token::DEF_DIR,
};
