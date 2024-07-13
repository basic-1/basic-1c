/*
 A1 assembler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 a1.h: basic assembler classes
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>

#include "a1errors.h"
#include "Utils.h"


#define A1_MAX_INST_ARGS_NUM 3


class Inst;


class A1Settings : public Settings
{
private:
	std::set<std::pair<int32_t, std::string>> _instructions_to_replace;

public:
	A1Settings()
	: Settings()
	{
	}

	void AddInstToReplace(int line_num, const std::string &file_name)
	{
		_instructions_to_replace.insert(std::make_pair(line_num, file_name));
	}

	bool IsInstToReplace(int line_num, const std::string &file_name) const
	{
		return (_instructions_to_replace.find(std::make_pair(line_num, file_name)) != _instructions_to_replace.end());
	}

	virtual A1_T_ERROR ProcessNumPostfix(const std::wstring &postfix, int32_t &n)
	{
		if(!postfix.empty())
		{
			if(postfix.size() > 2)
			{
				return A1_T_ERROR::A1_RES_ESYNTAX;
			}

			if(postfix[0] == L'l' || postfix[0] == L'L')
			{
				n = (uint16_t)n;
			}
			else
			if(postfix[0] == L'h' || postfix[0] == L'H')
			{
				n = (uint16_t)(n >> 16);
			}
			else
			{
				return A1_T_ERROR::A1_RES_ESYNTAX;
			}

			if(postfix.size() > 1)
			{
				if(postfix[1] == L'l' || postfix[1] == L'L')
				{
					n = (uint8_t)n;
				}
				else
				if(postfix[1] == L'h' || postfix[1] == L'H')
				{
					n = (uint8_t)(n >> 8);
				}
				else
				{
					return A1_T_ERROR::A1_RES_ESYNTAX;
				}
			}
		}

		return A1_T_ERROR::A1_RES_OK;
	}

	virtual A1_T_ERROR GetInstructions(const std::wstring &inst_name, const std::wstring &inst_sign, std::vector<const Inst *> &insts, int line_num, const std::string &file_name) const = 0;
};


extern A1Settings &_global_settings;


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

	A1_T_ERROR WriteDataRecord(int32_t first_pos, int32_t last_pos);
	A1_T_ERROR WriteExtLinearAddress(uint32_t address);
	A1_T_ERROR WriteEndOfFile();
	A1_T_ERROR Flush();


public:
	IhxWriter(const std::string &file_name);
	virtual ~IhxWriter();

	A1_T_ERROR Open();
	A1_T_ERROR Open(const std::string &file_name);

	A1_T_ERROR Write(const void *data, int32_t size);
	A1_T_ERROR SetAddress(uint32_t address);
	A1_T_ERROR Close();
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

	void MakeUpper();


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

	bool operator==(Token &&token) const
	{
		return (_toktype == token._toktype) && (_token == token._token);
	}

	bool operator!=(Token &&token) const
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

	static A1_T_ERROR QStringToString(const std::wstring &qstr, std::wstring &str);


	static const Token DATA_DIR;
	static const Token CONST_DIR;
	static const Token CODE_DIR;
	static const Token STACK_DIR;
	static const Token HEAP_DIR;

	static const Token IF_DIR;
	static const Token ELIF_DIR;
	static const Token ELSE_DIR;
	static const Token ENDIF_DIR;

	static const Token ERROR_DIR;

	static const Token DEF_DIR;
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

	A1_T_ERROR ReadChar(wchar_t &chr);

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

	A1_T_ERROR Open();
	void Close();

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
	A1_T_ERROR GetNextToken(Token &token);

	int GetLineNum() const
	{
		return _line_num;
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

	A1_T_ERROR Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end);

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
	std::vector<A1_T_WARNING> _warnings;

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

	virtual A1_T_ERROR Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, const std::map<std::wstring, MemRef> &memrefs, const std::string &file_name) = 0;

	virtual A1_T_ERROR Write(IhxWriter *writer, const std::map<std::wstring, MemRef> &memrefs) = 0;

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

	const std::vector<A1_T_WARNING> &GetWarnings() const
	{
		return _warnings;
	}
};


enum class SectType
{
	ST_NONE,
	ST_DATA,
	ST_CONST,
	ST_CODE,
	ST_INIT,
	ST_STACK,
	ST_HEAP,
};


class Section : public std::list<GenStmt *>
{
private:
	int32_t _sect_line_num;
	mutable int32_t _curr_line_num;
	std::string _file_name;

	SectType _type;
	std::wstring _type_mod;

	int32_t _address;

public:
	Section() = delete;

	Section(const Section &sect) = delete;

	Section(Section &&sect);

	Section(const std::string &file_name, int32_t sect_line_num, SectType st, const std::wstring &type_mod, int32_t address = -1);

	virtual ~Section();

	Section &operator=(const Section &) = delete;

	Section &operator=(Section &&) = delete;

	SectType GetType() const;

	const std::wstring &GetTypeMod() const;

	A1_T_ERROR GetAddress(int32_t &address) const;

	void SetAddress(int32_t address);

	A1_T_ERROR GetSize(int32_t &size) const;

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
	int32_t _val;
	bool _resolved;
	USGN _usgn;
	std::wstring _symbol;
	std::wstring _postfix;


public:
	EVal() = delete;

	EVal(int32_t val, USGN usgn = USGN::US_NONE);
	EVal(const std::wstring &symbol, USGN usgn);

	bool IsResolved() const
	{
		return _resolved;
	}

	A1_T_ERROR Resolve(const std::map<std::wstring, MemRef> &symbols = std::map<std::wstring, MemRef>());

	int32_t GetValue() const
	{
		return _val;
	}

	const std::wstring &GetSymbol() const
	{
		return _symbol;
	}

	std::wstring GetFullSymbol() const
	{
		return (_usgn == USGN::US_MINUS ? L"-" : (_usgn == USGN::US_NOT ? L"!" : L"")) + _symbol + (_postfix.empty() ? L"" : (L"." + _postfix));
	}
};


class Exp
{
protected:
	std::vector<std::wstring> _ops;
	std::vector<EVal> _vals;

public:
	static A1_T_ERROR BuildExp(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, Exp &exp, const std::vector<Token> &terms = std::vector<Token>());
	static A1_T_ERROR CalcSimpleExp(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, int32_t &res, const std::vector<Token> &terms = std::vector<Token>());

	void Clear()
	{
		_vals.clear();
		_ops.clear();
	}

	bool IsEmpty()
	{
		return _vals.empty() && _ops.empty();
	}

	void AddVal(const EVal &val)
	{
		_vals.push_back(val);
	}

	void AddOp(const std::wstring &op)
	{
		_ops.push_back(op);
	}

	A1_T_ERROR Eval(int32_t &res, const std::map<std::wstring, MemRef> &symbols = std::map<std::wstring, MemRef>()) const;

	bool GetSimpleValue(std::wstring &val) const
	{
		if(_ops.size() == 0 && _vals.size() == 1)
		{
			val = _vals[0].IsResolved() ? std::to_wstring(_vals[0].GetValue()) : _vals[0].GetFullSymbol();
			return true;
		}

		return false;
	}
};


class ArgType
{
public:
	int _size;
	int32_t _minval;
	int32_t _maxval;
	int32_t _multof;

	ArgType() = delete;

	ArgType(int size, int32_t minval, int32_t maxval, int32_t multipleof = 1)
	: _size(size)
	, _minval(minval)
	, _maxval(maxval)
	, _multof(multipleof)
	{
	}

	ArgType(const ArgType &) = delete;
	ArgType(ArgType &&) = delete;

	ArgType &operator=(const ArgType &) = delete;
	ArgType &operator=(ArgType &&) = delete;

	bool operator==(const ArgType &at) const
	{
		return std::addressof(*this) == std::addressof(at);
	}

	bool operator==(ArgType &&at) const
	{
		return std::addressof(*this) == std::addressof(at);
	}

	bool operator!=(const ArgType &at) const
	{
		return std::addressof(*this) != std::addressof(at);
	}

	bool operator!=(ArgType &&at) const
	{
		return std::addressof(*this) != std::addressof(at);
	}

	virtual bool IsValidValue(int32_t value) const
	{
		return (*this == AT_NONE) || ((value >= _minval) && (value <= _maxval) && (value % _multof) == 0);
	}

	static const ArgType AT_NONE;
	static const ArgType AT_1BYTE_ADDR; // 0..FF
	static const ArgType AT_2BYTE_ADDR; // 0..FFFF
	static const ArgType AT_3BYTE_ADDR; // 0..FFFFFF
	static const ArgType AT_1BYTE_OFF; // -128..+127 (offset for JRx, CALLR instructions)
	static const ArgType AT_1BYTE_VAL; // -128..255
	static const ArgType AT_2BYTE_VAL; // -32768..65535
	static const ArgType AT_SPEC_TYPE; // special arg. type processed by Inst::CheckArgs
};


class Inst
{
private:
	uint32_t get_hex_value(const std::wstring &str, std::wstring *postfix = nullptr, int *len = nullptr);
	uint32_t get_bit_arg(const std::wstring &bit_arg, int &start_pos, int &len, std::wstring &postfix);

	void Init(const wchar_t *code, int speed, const ArgType &arg1type, const ArgType &arg2type, const ArgType &arg3type);
public:
	// instruction code size
	int _size;
	// instruction speed (in ticks)
	int _speed;
	// arguments count
	int _argnum;
	// argument types
	std::vector<std::reference_wrapper<const ArgType>> _argtypes;
	// code:               inst_end is_arg code      start len  postfix
	std::vector<std::tuple<bool,    bool,  uint32_t, int,  int, std::wstring>> _code;

	Inst(const wchar_t *code, const ArgType &arg1type = ArgType::AT_NONE, const ArgType &arg2type = ArgType::AT_NONE, const ArgType &arg3type = ArgType::AT_NONE);
	Inst(const wchar_t *code, int speed, const ArgType &arg1type = ArgType::AT_NONE, const ArgType &arg2type = ArgType::AT_NONE, const ArgType &arg3type = ArgType::AT_NONE);

	virtual bool CheckArgs(int32_t a1, int32_t a2, int32_t a3) const
	{
		return _argtypes[0].get().IsValidValue(a1) && _argtypes[1].get().IsValidValue(a2) && _argtypes[2].get().IsValidValue(a3);
	}

	virtual A1_T_ERROR GetSpecArg(int arg_num, std::pair<std::reference_wrapper<const ArgType>, Exp> &ref, int32_t &val) const
	{
		return A1_T_ERROR::A1_RES_EINVINST;
	}
};


class DataStmt: public GenStmt
{
protected:
	int32_t _size1;
	bool _size_specified;

	bool IsDataStmt(const Token &token, int *data_size = nullptr);


public:
	DataStmt()
	: GenStmt()
	, _size1(-1)
	, _size_specified(false)
	{
	}

	DataStmt(int32_t size1, int32_t size)
	: GenStmt()
	, _size1(size1)
	, _size_specified(false)
	{
		_size = size;
	}

	virtual ~DataStmt()
	{
	}

	A1_T_ERROR Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, const std::map<std::wstring, MemRef> &memrefs, const std::string &file_name) override;
	A1_T_ERROR Write(IhxWriter *writer, const std::map<std::wstring, MemRef> &memrefs) override
	{
		// do nothing
		return A1_T_ERROR::A1_RES_OK;
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

	StackStmt(int32_t size1, int32_t size)
	: DataStmt(size1, size)
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

	ConstStmt(int32_t size1, int32_t size)
	: DataStmt(size1, size)
	, _truncated(false)
	{
		_data.assign(size, 0);
	}

	virtual ~ConstStmt()
	{
	}

	A1_T_ERROR Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, const std::map<std::wstring, MemRef> &memrefs, const std::string &file_name) override;
	A1_T_ERROR Write(IhxWriter *writer, const std::map<std::wstring, MemRef> &memrefs) override;
};

class CodeStmt: public ConstStmt
{
protected:
	bool _is_inst; // true stands for instruction, false - data definition
	std::vector<std::pair<std::reference_wrapper<const ArgType>, Exp>> _refs;
	const Inst *_inst;

	virtual A1_T_ERROR GetRefValue(const std::pair<std::reference_wrapper<const ArgType>, Exp> &ref, const std::map<std::wstring, MemRef> &memrefs, uint32_t &value, int &size);
	A1_T_ERROR ReadInstArg(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, std::wstring &argsign);

	// the function should return expression and signature for the specified expression, e.g.
	// input: exp = 10 + 5, output: exp = 10 + 5, sign = "V"
	// input: exp = "X", output: exp = <empty>, sign = "X" (here X is a register name)
	virtual A1_T_ERROR GetExpressionSignature(Exp &exp, std::wstring &sign) = 0;
public:
	CodeStmt()
	: ConstStmt()
	, _is_inst(false)
	, _inst(nullptr)
	{
	}

	virtual ~CodeStmt()
	{
	}

	A1_T_ERROR Read(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, const std::map<std::wstring, MemRef> &memrefs, const std::string &file_name) override;
	A1_T_ERROR Write(IhxWriter *writer, const std::map<std::wstring, MemRef> &memrefs) override;
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
protected:
	int32_t _curr_line_num;
	std::string _curr_file_name;
	std::vector<std::tuple<int32_t, std::string, A1_T_WARNING>> _warnings;

	std::vector<std::string> _src_files;
	std::vector<std::vector<Token>> _token_files;

	std::map<std::wstring, MemRef> _memrefs;

	std::string _custom_err_msg;

	int32_t _data_size;
	int32_t _init_size;
	int32_t _const_size;
	int32_t _code_size;

	static const std::vector<std::reference_wrapper<const Token>> ALL_DIRS;

	// the method should return true if the section type and its modifier string are correct
	virtual bool CheckSectionName(SectType stype, const std::wstring &type_mod) const = 0;
	// creates object representing new statement of type specified with stype and type_mod,
	// e.g. DataStmt for SectType::ST_DATA
	virtual GenStmt *CreateNewStmt(SectType stype, const std::wstring &type_mod) const = 0;

	void Clear();

	A1_T_ERROR ReadStmt(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end);
	A1_T_ERROR CheckIFDir(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, bool &res);
	A1_T_ERROR ReadUntil(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, std::vector<std::reference_wrapper<const Token>>::const_iterator stop_dirs_start, std::vector<std::reference_wrapper<const Token>>::const_iterator stop_dirs_end);
	A1_T_ERROR SkipUntil(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, std::vector<std::reference_wrapper<const Token>>::const_iterator stop_dirs_start, std::vector<std::reference_wrapper<const Token>>::const_iterator stop_dirs_end);
	A1_T_ERROR ReadSingleIFDir(bool is_else, std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, bool &skip);
	A1_T_ERROR ReadIFDir(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, bool skip);
	A1_T_ERROR ReadSection(std::vector<Token>::const_iterator &start, const std::vector<Token>::const_iterator &end, bool skip);
	virtual A1_T_ERROR AlignSectionBegin(Section *psec);
	virtual A1_T_ERROR AlignSectionEnd(Section *psec);
	A1_T_ERROR ReadSections(int32_t file_num, SectType sec_type, const std::wstring &type_mod, int32_t sec_base, int32_t &over_size, int32_t max_size);

	virtual A1_T_ERROR ReadHeapSections();
	virtual A1_T_ERROR ReadStackSections();
	virtual A1_T_ERROR ReadDataSections();
	virtual A1_T_ERROR ReadCodeInitSections();
	virtual A1_T_ERROR ReadConstSections();
	virtual A1_T_ERROR ReadCodeSections();


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

	A1_T_ERROR ReadSourceFiles(const std::vector<std::string> &src_files);
	A1_T_ERROR ReadSections();
	A1_T_ERROR Write(const std::string &file_name);

	int32_t GetCurrLineNum() const
	{
		return _curr_line_num;
	}

	std::string GetCurrFileName() const
	{
		return _curr_file_name;
	}

	const std::vector<std::tuple<int32_t, std::string, A1_T_WARNING>> &GetWarnings() const
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

	std::string GetCustomErrorMsg() const
	{
		return _custom_err_msg;
	}
};
