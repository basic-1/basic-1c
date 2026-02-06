/*
 STM8 assembler
 Copyright (c) 2021-2026 Nikolay Pletnev
 MIT license

 a1stm8.cpp: STM8 assembler
*/


#include <cstdio>
#include <clocale>
#include <cstring>
#include <cwctype>
#include <memory>
#include <fstream>

#include "../../common/source/trgsel.h"
#include "../../common/source/a1.h"
#include "../../common/source/a1errors.h"
#include "../../common/source/version.h"
#include "../../common/source/gitrev.h"


static const char *version = B1_CMP_VERSION;


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


static std::multimap<std::wstring, std::unique_ptr<Inst>> _instructions;

#define ADD_INST(SIGN, OPCODE, ...) _instructions.emplace(SIGN, new Inst((OPCODE), ##__VA_ARGS__))

static void load_all_instructions()
{
	// ADC
	ADD_INST(L"ADCA,V",			L"A9 {1}", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"ADCA,(V)",		L"B9 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADCA,(V)",		L"C9 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADCA,(X)",		L"F9");
	ADD_INST(L"ADCA,(V,X)",		L"E9 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADCA,(V,X)",		L"D9 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADCA,(Y)",		L"90F9");
	ADD_INST(L"ADCA,(V,Y)",		L"90E9 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADCA,(V,Y)",		L"90D9 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADCA,(V,SP)",	L"19 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADCA,[V]",		L"92C9 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADCA,[V]",		L"72C9 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADCA,([V],X)",	L"92D9 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADCA,([V],X)",	L"72D9 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADCA,([V],Y)",	L"91D9 {1}", ArgType::AT_1BYTE_ADDR);

	// ADD
	ADD_INST(L"ADDA,V",			L"AB {1}", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"ADDA,(V)",		L"BB {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDA,(V)",		L"CB {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDA,(X)",		L"FB");
	ADD_INST(L"ADDA,(V,X)",		L"EB {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDA,(V,X)",		L"DB {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDA,(Y)",		L"90FB");
	ADD_INST(L"ADDA,(V,Y)",		L"90EB {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDA,(V,Y)",		L"90DB {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDA,(V,SP)",	L"1B {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDA,[V]",		L"92CB {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDA,[V]",		L"72CB {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDA,([V],X)",	L"92DB {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDA,([V],X)",	L"72DB {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDA,([V],Y)",	L"91DB {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDSP,V",		L"5B {1}", ArgType::AT_1BYTE_ADDR);

	// ADDW
	ADD_INST(L"ADDWX,V",		L"1C {1}", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"ADDWX,(V)",		L"72BB {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDWX,(V,SP)",	L"72FB {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDWY,V",		L"72A9 {1}", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"ADDWY,(V)",		L"72B9 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ADDWY,(V,SP)",	L"72F9 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ADDWSP,V",		L"5B {1}", ArgType::AT_1BYTE_ADDR);

	// AND
	ADD_INST(L"ANDA,V",			L"A4 {1}", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"ANDA,(V)",		L"B4 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ANDA,(V)",		L"C4 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ANDA,(X)",		L"F4");
	ADD_INST(L"ANDA,(V,X)",		L"E4 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ANDA,(V,X)",		L"D4 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ANDA,(Y)",		L"90F4");
	ADD_INST(L"ANDA,(V,Y)",		L"90E4 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ANDA,(V,Y)",		L"90D4 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ANDA,(V,SP)",	L"14 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ANDA,[V]",		L"92C4 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ANDA,[V]",		L"72C4 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ANDA,([V],X)",	L"92D4 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ANDA,([V],X)",	L"72D4 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ANDA,([V],Y)",	L"91D4 {1}", ArgType::AT_1BYTE_ADDR);

	// BCCM
	// 901n, where n = 1 + 2 * pos
	ADD_INST(L"BCCM(V),V",		L"90 1:4 {2:2:3} 1:1 {1}", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_VAL);

	// BCP
	ADD_INST(L"BCPA,V",			L"A5 {1}", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"BCPA,(V)",		L"B5 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"BCPA,(V)",		L"C5 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPA,(X)",		L"F5");
	ADD_INST(L"BCPA,(V,X)",		L"E5 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"BCPA,(V,X)",		L"D5 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPA,(Y)",		L"90F5");
	ADD_INST(L"BCPA,(V,Y)",		L"90E5 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"BCPA,(V,Y)",		L"90D5 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPA,(V,SP)",	L"15 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"BCPA,[V]",		L"92C5 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"BCPA,[V]",		L"72C5 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPA,([V],X)",	L"92D5 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"BCPA,([V],X)",	L"72D5 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"BCPA,([V],Y)",	L"91D5 {1}", ArgType::AT_1BYTE_ADDR);

	// BCPL
	// 901n, where n = 2 * pos
	ADD_INST(L"BCPL(V),V",		L"90 1:4 {2:2:3} 0:1 {1}", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_VAL);

	// BREAK
	ADD_INST(L"BREAK", L"8B");

	// BRES
	// 721n, where n = 1 + 2 * pos
	ADD_INST(L"BRES(V),V",		L"72 1:4 {2:2:3} 1:1 {1}", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_VAL);

	// BSET
	// 721n, where n = 2 * pos
	ADD_INST(L"BSET(V),V",		L"72 1:4 {2:2:3} 0:1 {1}", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_VAL);

	// BTJF
	// 720n, where n = 1 + 2 * pos
	ADD_INST(L"BTJF(V),V,V",	L"72 0:4 {2:2:3} 1:1 {1} {3}", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_VAL, ArgType::AT_1BYTE_OFF);

	// BTJT
	// 720n, where n = 2 * pos
	ADD_INST(L"BTJT(V),V,V",	L"72 0:4 {2:2:3} 0:1 {1} {3}", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_VAL, ArgType::AT_1BYTE_OFF);

	// CALL
	ADD_INST(L"CALLV",			L"CD {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CALL(V)",		L"CD {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CALL(X)",		L"FD");
	ADD_INST(L"CALL(V,X)",		L"ED {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CALL(V,X)",		L"DD {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CALL(Y)",		L"90FD");
	ADD_INST(L"CALL(V,Y)",		L"90ED {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CALL(V,Y)",		L"90DD {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CALL[V]",		L"92CD {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CALL[V]",		L"72CD {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CALL([V],X)",	L"92DD {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CALL([V],X)",	L"72DD {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CALL([V],Y)",	L"91DD {1}", ArgType::AT_1BYTE_ADDR);

	// CALLF
	ADD_INST(L"CALLFV",			L"8D {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"CALLF(V)",		L"8D {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"CALLF[V]",		L"928D {1}", ArgType::AT_2BYTE_ADDR);

	// CALLR
	ADD_INST(L"CALLRV",			L"AD {1}", ArgType::AT_1BYTE_OFF);

	// CCF
	ADD_INST(L"CCF",			L"8C");

	// CLR
	ADD_INST(L"CLRA",			L"4F");
	ADD_INST(L"CLR(V)",			L"3F {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CLR(V)",			L"725F {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CLR(X)",			L"7F");
	ADD_INST(L"CLR(V,X)",		L"6F {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CLR(V,X)",		L"724F {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CLR(Y)",			L"907F");
	ADD_INST(L"CLR(V,Y)",		L"906F {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CLR(V,Y)",		L"904F {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CLR(V,SP)",		L"0F {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CLR[V]",			L"923F {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CLR[V]",			L"723F {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CLR([V],X)",		L"926F {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CLR([V],X]",		L"726F {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CLR([V],Y)",		L"916F {1}", ArgType::AT_1BYTE_ADDR);

	// CLRW
	ADD_INST(L"CLRWX",			L"5F");
	ADD_INST(L"CLRWY",			L"905F");

	// CP
	ADD_INST(L"CPA,V",			L"A1 {1}", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"CPA,(V)",		L"B1 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPA,(V)",		L"C1 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPA,(X)",		L"F1");
	ADD_INST(L"CPA,(V,X)",		L"E1 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPA,(V,X)",		L"D1 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPA,(Y)",		L"90F1");
	ADD_INST(L"CPA,(V,Y)",		L"90E1 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPA,(V,Y)",		L"90D1 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPA,(V,SP)",		L"11 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPA,[V]",		L"92C1 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPA,[V]",		L"72C1 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPA,([V],X)",	L"92D1 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPA,([V],X)",	L"72D1 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPA,([V],Y)",	L"91D1 {1}", ArgType::AT_1BYTE_ADDR);

	// CPW
	ADD_INST(L"CPWX,V",			L"A3 {1}", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"CPWX,(V)",		L"B3 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWX,(V)",		L"C3 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPWX,(Y)",		L"90F3");
	ADD_INST(L"CPWX,(V,Y)",		L"90E3 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWX,(V,Y)",		L"90D3 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPWX,(V,SP)",	L"13 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWX,[V]",		L"92C3 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWX,[V]",		L"72C3 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPWX,([V],Y)",	L"91D3 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWY,V",			L"90A3 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPWY,(V)",		L"90B3 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWY,(V)",		L"90C3 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPWY,(X)",		L"F3");
	ADD_INST(L"CPWY,(V,X)",		L"E3 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWY,(V,X)",		L"D3 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPWY,[V]",		L"91C3 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWY,[V],X",		L"92D3 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPWY,[V],X",		L"72D3 {1}", ArgType::AT_2BYTE_ADDR);

	// CPL
	ADD_INST(L"CPLA",			L"43");
	ADD_INST(L"CPL(V)",			L"33 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPL(V)",			L"7253 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPL(X)",			L"73");
	ADD_INST(L"CPL(V,X)",		L"63 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPL(V,X)",		L"7243 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPL(Y)",			L"9073");
	ADD_INST(L"CPL(V,Y)",		L"9063 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPL(V,Y)",		L"9043 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPL(V,SP)",		L"03 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPL[V]",			L"9233 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPL[V]",			L"7233 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPL([V],X)",		L"9263 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"CPL([V],X]",		L"7263 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"CPL([V],Y)",		L"9163 {1}", ArgType::AT_1BYTE_ADDR);

	// CPLW
	ADD_INST(L"CPLWX",			L"53");
	ADD_INST(L"CPLWY",			L"9053");

	// DEC
	ADD_INST(L"DECA",			L"4A");
	ADD_INST(L"DEC(V)",			L"3A {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"DEC(V)",			L"725A {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"DEC(X)",			L"7A");
	ADD_INST(L"DEC(V,X)",		L"6A {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"DEC(V,X)",		L"724A {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"DEC(Y)",			L"907A");
	ADD_INST(L"DEC(V,Y)",		L"906A {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"DEC(V,Y)",		L"904A {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"DEC(V,SP)",		L"0A {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"DEC[V]",			L"923A {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"DEC[V]",			L"723A {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"DEC([V],X)",		L"926A {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"DEC([V],X]",		L"726A {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"DEC([V],Y)",		L"916A {1}", ArgType::AT_1BYTE_ADDR);

	// DECW
	ADD_INST(L"DECWX",			L"5A");
	ADD_INST(L"DECWY",			L"905A");

	// DIV
	ADD_INST(L"DIVX,A",			L"62");
	ADD_INST(L"DIVY,A",			L"9062");

	// DIVW
	ADD_INST(L"DIVWX,Y",		L"65");

	// EXG
	ADD_INST(L"EXGA,XL",		L"41");
	ADD_INST(L"EXGA,YL",		L"61");
	ADD_INST(L"EXGA,(V)",		L"31 {1}", ArgType::AT_2BYTE_ADDR);

	// EXGW
	ADD_INST(L"EXGWX,Y",		L"51");

	// HALT
	ADD_INST(L"HALT",			L"8E");

	// INC
	ADD_INST(L"INCA",			L"4C");
	ADD_INST(L"INC(V)",			L"3c {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"INC(V)",			L"725C {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"INC(X)",			L"7C");
	ADD_INST(L"INC(V,X)",		L"6C {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"INC(V,X)",		L"724C {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"INC(Y)",			L"907C");
	ADD_INST(L"INC(V,Y)",		L"906C {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"INC(V,Y)",		L"904C {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"INC(V,SP)",		L"0C {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"INC[V]",			L"923C {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"INC[V]",			L"723C {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"INC([V],X)",		L"926C {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"INC([V],X]",		L"726C {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"INC([V],Y)",		L"916C {1}", ArgType::AT_1BYTE_ADDR);

	// INCW
	ADD_INST(L"INCWX",			L"5C");
	ADD_INST(L"INCWY",			L"905C");

	// INT
	ADD_INST(L"INTV",			L"82 {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"INT(V)",			L"82 {1}", ArgType::AT_3BYTE_ADDR);

	// IRET
	ADD_INST(L"IRET",			L"80");

	// JP
	ADD_INST(L"JPV",			L"CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"JP(V)",			L"CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"JP(X)",			L"FC");
	ADD_INST(L"JP(V,X)",		L"EC {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"JP(V,X)",		L"DC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"JP(Y)",			L"90FC");
	ADD_INST(L"JP(V,Y)",		L"90EC {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"JP(V,Y)",		L"90DC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"JP[V]",			L"92CC {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"JP[V]",			L"72CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"JP([V],X)",		L"92DC {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"JP([V],X)",		L"72DC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"JP([V],Y)",		L"91DC {1}", ArgType::AT_1BYTE_ADDR);

	// JPF
	ADD_INST(L"JPFV",			L"AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"JPF(V)",			L"AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"JPF[V]",			L"92AC {1}", ArgType::AT_2BYTE_ADDR);

	// JRX
	ADD_INST(L"JRAV",			L"20 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRTV",			L"20 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRCV",			L"25 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRULTV",			L"25 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JREQV",			L"27 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRFV",			L"21 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRHV",			L"9029 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRIHV",			L"902F {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRILV",			L"902E {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRMV",			L"902D {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRMIV",			L"2B {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRNCV",			L"24 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRUGEV",			L"24 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRNEV",			L"26 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRNHV",			L"9028 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRNMV",			L"902C {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRNVV",			L"28 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRPLV",			L"2A {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRSGEV",			L"2E {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRSGTV",			L"2C {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRSLEV",			L"2D {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRSLTV",			L"2F {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRUGTV",			L"22 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRULEV",			L"23 {1}", ArgType::AT_1BYTE_OFF);
	ADD_INST(L"JRVV",			L"29 {1}", ArgType::AT_1BYTE_OFF);

	// LD
	ADD_INST(L"LDA,V",			L"A6 {1}", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"LDA,(V)",		L"B6 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDA,(V)",		L"C6 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDA,(X)",		L"F6");
	ADD_INST(L"LDA,(V,X)",		L"E6 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDA,(V,X)",		L"D6 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDA,(Y)",		L"90F6");
	ADD_INST(L"LDA,(V,Y)",		L"90E6 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDA,(V,Y)",		L"90D6 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDA,(V,SP)",		L"7B {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDA,[V]",		L"92C6 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDA,[V]",		L"72C6 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDA,([V],X)",	L"92D6 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDA,([V],X)",	L"72D6 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDA,([V],Y)",	L"91D6 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD(V),A",		L"B7 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD(V),A",		L"C7 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LD(X),A",		L"F7");
	ADD_INST(L"LD(V,X),A",		L"E7 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD(V,X),A",		L"D7 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LD(Y),A",		L"90F7");
	ADD_INST(L"LD(V,Y),A",		L"90E7 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD(V,Y),A",		L"90D7 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LD(V,SP),A",		L"6B {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD[V],A",		L"92C7 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD[V],A",		L"72C7 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LD([V],X),A",	L"92D7 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LD([V],X),A",	L"72D7 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LD([V],Y),A",	L"91D7 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDXL,A",			L"97");
	ADD_INST(L"LDA,XL",			L"9F");
	ADD_INST(L"LDYL,A",			L"9097");
	ADD_INST(L"LDA,YL",			L"909F");
	ADD_INST(L"LDXH,A",			L"95");
	ADD_INST(L"LDA,XH",			L"9E");
	ADD_INST(L"LDYH,A",			L"9095");
	ADD_INST(L"LDA,YH",			L"909E");

	// LDF
	ADD_INST(L"LDFA,(V)",		L"BC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"LDFA,(V,X)",		L"AF {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"LDFA,(V,Y)",		L"90AF {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"LDFA,[V]",		L"92BC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDFA,([V],X)",	L"92AF {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDFA,([V],Y)",	L"91AF {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDF(V),A",		L"BD {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"LDF(V,X),A",		L"A7 {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"LDF(V,Y),A",		L"90A7 {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST(L"LDF[V],A",		L"92BD {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDF([V],X),A",	L"92A7 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDF([V],Y),A",	L"91A7 {1}", ArgType::AT_2BYTE_ADDR);

	// LDW
	ADD_INST(L"LDWX,V",			L"AE {1}", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"LDWX,(V)",		L"BE {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWX,(V)",		L"CE {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDWX,(X)",		L"FE");
	ADD_INST(L"LDWX,(V,X)",		L"EE {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWX,(V,X)",		L"DE {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDWX,(V,SP)",	L"1E {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWX,[V]",		L"92CE {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWX,[V]",		L"72CE {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDWX,([V],X)",	L"92DE {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWX,([V],X)",	L"92DE {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDW(V),X",		L"BF {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW(V),X",		L"CF {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDW(X),Y",		L"FF");
	ADD_INST(L"LDW(V,X),Y",		L"EF {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW(V,X),Y",		L"DF {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDW(V,SP),X",	L"1F {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW[V],X",		L"92CF {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW[V],X",		L"72CF {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDW([V],X),Y",	L"92DF {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW([V],X),Y",	L"72DF {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDWY,V",			L"90AE {1}", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"LDWY,(V)",		L"90BE {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWY,(V)",		L"90CE {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDWY,(Y)",		L"90FE");
	ADD_INST(L"LDWY,(V,Y)",		L"90EE {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWY,(V,Y)",		L"90DE {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDWY,(V,SP)",	L"16 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWY,[V]",		L"91CE {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWY,([V],Y)",	L"91DE {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW(V),Y",		L"90BF {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW(V),Y",		L"90CF {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDW(Y),X",		L"90FF");
	ADD_INST(L"LDW(V,Y),X",		L"90EF {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW(V,Y),X",		L"90DF {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"LDW(V,SP),Y",	L"17 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW[V],Y",		L"91CF {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDW([V],Y),X",	L"91DF {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"LDWY,X",			L"9093");
	ADD_INST(L"LDWX,Y",			L"93");
	ADD_INST(L"LDWX,SP",		L"96");
	ADD_INST(L"LDWSP,X",		L"94");
	ADD_INST(L"LDWY,SP",		L"9096");
	ADD_INST(L"LDWSP,Y",		L"9094");

	// MOV
	ADD_INST(L"MOV(V),V",		L"35 {2} {1}", ArgType::AT_2BYTE_ADDR, ArgType::AT_1BYTE_VAL);
	ADD_INST(L"MOV(V),(V)",		L"45 {2} {1}", ArgType::AT_1BYTE_ADDR, ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"MOV(V),(V)",		L"55 {2} {1}", ArgType::AT_2BYTE_ADDR, ArgType::AT_2BYTE_ADDR);

	// MUL
	ADD_INST(L"MULX,A",			L"42");
	ADD_INST(L"MULY,A",			L"9042");

	// NEG
	ADD_INST(L"NEGA",			L"40");
	ADD_INST(L"NEG(V)",			L"30 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"NEG(V)",			L"7250 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"NEG(X)",			L"70");
	ADD_INST(L"NEG(V,X)",		L"60 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"NEG(V,X)",		L"7240 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"NEG(Y)",			L"9070");
	ADD_INST(L"NEG(V,Y)",		L"9060 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"NEG(V,Y)",		L"9040 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"NEG(V,SP)",		L"00 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"NEG[V]",			L"9230 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"NEG[V]",			L"7230 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"NEG([V],X)",		L"9260 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"NEG([V],X]",		L"7260 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"NEG([V],Y)",		L"9160 {1}", ArgType::AT_1BYTE_ADDR);

	// NEGW
	ADD_INST(L"NEGWX",			L"50");
	ADD_INST(L"NEGWY",			L"9050");

	// NOP
	ADD_INST(L"NOP",			L"9D");

	// OR
	ADD_INST(L"ORA,V",			L"AA {1}", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"ORA,(V)",		L"BA {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ORA,(V)",		L"CA {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ORA,(X)",		L"FA");
	ADD_INST(L"ORA,(V,X)",		L"EA {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ORA,(V,X)",		L"DA {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ORA,(Y)",		L"90FA");
	ADD_INST(L"ORA,(V,Y)",		L"90EA {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ORA,(V,Y)",		L"90DA {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ORA,(V,SP)",		L"1A {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ORA,[V]",		L"92CA {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ORA,[V]",		L"72CA {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ORA,([V],X)",	L"92DA {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"ORA,([V],X)",	L"72DA {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"ORA,([V],Y)",	L"91DA {1}", ArgType::AT_1BYTE_ADDR);

	// POP
	ADD_INST(L"POPA",			L"84");
	ADD_INST(L"POPCC",			L"86");
	ADD_INST(L"POP(V)",			L"32 {1}", ArgType::AT_2BYTE_ADDR);

	// POPW
	ADD_INST(L"POPWX",			L"85");
	ADD_INST(L"POPWY",			L"9085");

	// PUSH
	ADD_INST(L"PUSHA",			L"88");
	ADD_INST(L"PUSHCC",			L"8A");
	ADD_INST(L"PUSHV",			L"4B {1}", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"PUSH(V)",		L"3B {1}", ArgType::AT_2BYTE_ADDR);

	// PUSHW
	ADD_INST(L"PUSHWX",			L"89");
	ADD_INST(L"PUSHWY",			L"9089");

	// RCF
	ADD_INST(L"RCF",			L"98");

	// RET
	ADD_INST(L"RET",			L"81");

	// RETF
	ADD_INST(L"RETF",			L"87");

	// RIM
	ADD_INST(L"RIM",			L"9A");

	// RLC
	ADD_INST(L"RLCA",			L"49");
	ADD_INST(L"RLC(V)",			L"39 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RLC(V)",			L"7259 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RLC(X)",			L"79");
	ADD_INST(L"RLC(V,X)",		L"69 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RLC(V,X)",		L"7249 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RLC(Y)",			L"9079");
	ADD_INST(L"RLC(V,Y)",		L"9069 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RLC(V,Y)",		L"9049 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RLC(V,SP)",		L"09 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RLC[V]",			L"9239 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RLC[V]",			L"7239 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RLC([V],X)",		L"9269 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RLC([V],X]",		L"7269 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RLC([V],Y)",		L"9169 {1}", ArgType::AT_1BYTE_ADDR);

	// RLCW
	ADD_INST(L"RLCWX",			L"59");
	ADD_INST(L"RLCWY",			L"9059");

	// RLWA
	ADD_INST(L"RLWAX",			L"02");
	ADD_INST(L"RLWAY",			L"9002");

	// RRC
	ADD_INST(L"RRCA",			L"46");
	ADD_INST(L"RRC(V)",			L"36 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RRC(V)",			L"7256 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RRC(X)",			L"76");
	ADD_INST(L"RRC(V,X)",		L"66 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RRC(V,X)",		L"7246 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RRC(Y)",			L"9076");
	ADD_INST(L"RRC(V,Y)",		L"9066 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RRC(V,Y)",		L"9046 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RRC(V,SP)",		L"06 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RRC[V]",			L"9236 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RRC[V]",			L"7236 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RRC([V],X)",		L"9266 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"RRC([V],X]",		L"7266 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"RRC([V],Y)",		L"9166 {1}", ArgType::AT_1BYTE_ADDR);
	
	// RRCW
	ADD_INST(L"RRCWX",			L"56");
	ADD_INST(L"RRCWY",			L"9056");

	// RRWA
	ADD_INST(L"RRWAX",			L"01");
	ADD_INST(L"RRWAY",			L"9001");

	// RVF
	ADD_INST(L"RVF",			L"9C");

	// SBC
	ADD_INST(L"SBCA,V",			L"A2 {1}", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"SBCA,(V)",		L"B2 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SBCA,(V)",		L"C2 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SBCA,(X)",		L"F2");
	ADD_INST(L"SBCA,(V,X)",		L"E2 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SBCA,(V,X)",		L"D2 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SBCA,(Y)",		L"90F2");
	ADD_INST(L"SBCA,(V,Y)",		L"90E2 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SBCA,(V,Y)",		L"90D2 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SBCA,(V,SP)",	L"12 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SBCA,[V]",		L"92C2 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SBCA,[V]",		L"72C2 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SBCA,([V],X)",	L"92D2 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SBCA,([V],X)",	L"72D2 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SBCA,([V],Y)",	L"91D2 {1}", ArgType::AT_1BYTE_ADDR);

	// SCF
	ADD_INST(L"SCF",			L"99");

	// SIM
	ADD_INST(L"SIM",			L"9B");

	// SLA
	ADD_INST(L"SLAA",			L"48");
	ADD_INST(L"SLA(V)",			L"38 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLA(V)",			L"7258 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLA(X)",			L"78");
	ADD_INST(L"SLA(V,X)",		L"68 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLA(V,X)",		L"7248 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLA(Y)",			L"9078");
	ADD_INST(L"SLA(V,Y)",		L"9068 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLA(V,Y)",		L"9048 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLA(V,SP)",		L"08 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLA[V]",			L"9238 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLA[V]",			L"7238 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLA([V],X)",		L"9268 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLA([V],X]",		L"7268 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLA([V],Y)",		L"9168 {1}", ArgType::AT_1BYTE_ADDR);

	// SLAW
	ADD_INST(L"SLAWX",			L"58");
	ADD_INST(L"SLAWY",			L"9058");

	// SLL
	ADD_INST(L"SLLA",			L"48");
	ADD_INST(L"SLL(V)",			L"38 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLL(V)",			L"7258 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLL(X)",			L"78");
	ADD_INST(L"SLL(V,X)",		L"68 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLL(V,X)",		L"7248 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLL(Y)",			L"9078");
	ADD_INST(L"SLL(V,Y)",		L"9068 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLL(V,Y)",		L"9048 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLL(V,SP)",		L"08 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLL[V]",			L"9238 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLL[V]",			L"7238 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLL([V],X)",		L"9268 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SLL([V],X]",		L"7268 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SLL([V],Y)",		L"9168 {1}", ArgType::AT_1BYTE_ADDR);

	// SLLW
	ADD_INST(L"SLLWX",			L"58");
	ADD_INST(L"SLLWY",			L"9058");

	// SRA
	ADD_INST(L"SRAA",			L"47");
	ADD_INST(L"SRA(V)",			L"37 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRA(V)",			L"7257 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRA(X)",			L"77");
	ADD_INST(L"SRA(V,X)",		L"67 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRA(V,X)",		L"7247 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRA(Y)",			L"9077");
	ADD_INST(L"SRA(V,Y)",		L"9067 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRA(V,Y)",		L"9047 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRA(V,SP)",		L"07 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRA[V]",			L"9237 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRA[V]",			L"7237 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRA([V],X)",		L"9267 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRA([V],X]",		L"7267 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRA([V],Y)",		L"9167 {1}", ArgType::AT_1BYTE_ADDR);

	// SRAW
	ADD_INST(L"SRAWX",			L"57");
	ADD_INST(L"SRAWY",			L"9057");

	// SRL
	ADD_INST(L"SRLA",			L"44");
	ADD_INST(L"SRL(V)",			L"34 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRL(V)",			L"7254 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRL(X)",			L"74");
	ADD_INST(L"SRL(V,X)",		L"64 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRL(V,X)",		L"7244 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRL(Y)",			L"9074");
	ADD_INST(L"SRL(V,Y)",		L"9064 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRL(V,Y)",		L"9044 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRL(V,SP)",		L"04 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRL[V]",			L"9234 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRL[V]",			L"7234 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRL([V],X)",		L"9264 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SRL([V],X]",		L"7264 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SRL([V],Y)",		L"9164 {1}", ArgType::AT_1BYTE_ADDR);

	// SRLW
	ADD_INST(L"SRLWX",			L"54");
	ADD_INST(L"SRLWY",			L"9054");

	// SUB
	ADD_INST(L"SUBA,V",			L"A0 {1}", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"SUBA,(V)",		L"B0 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBA,(V)",		L"C0 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBA,(X)",		L"F0");
	ADD_INST(L"SUBA,(V,X)",		L"E0 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBA,(V,X)",		L"D0 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBA,(Y)",		L"90F0");
	ADD_INST(L"SUBA,(V,Y)",		L"90E0 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBA,(V,Y)",		L"90D0 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBA,(V,SP)",	L"10 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBA,[V]",		L"92C0 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBA,[V]",		L"72C0 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBA,([V],X)",	L"92D0 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBA,([V],X)",	L"72D0 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBA,([V],Y)",	L"91D0 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBSP,V",		L"52 {1}", ArgType::AT_1BYTE_ADDR);

	// SUBW
	ADD_INST(L"SUBWX,V",		L"1D {1}", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"SUBWX,(V)",		L"72B0 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBWX,(V,SP)",	L"72F0 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBWY,V",		L"72A2 {1}", ArgType::AT_2BYTE_VAL);
	ADD_INST(L"SUBWY,(V)",		L"72B2 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SUBWY,(V,SP)",	L"72F2 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SUBWSP,V",		L"52 {1}", ArgType::AT_1BYTE_ADDR);

	// SWAP
	ADD_INST(L"SWAPA",			L"4E");
	ADD_INST(L"SWAP(V)",		L"3E {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SWAP(V)",		L"725E {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SWAP(X)",		L"7E");
	ADD_INST(L"SWAP(V,X)",		L"6E {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SWAP(V,X)",		L"724E {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SWAP(Y)",		L"907E");
	ADD_INST(L"SWAP(V,Y)",		L"906E {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SWAP(V,Y)",		L"904E {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SWAP(V,SP)",		L"0E {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SWAP[V]",		L"923E {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SWAP[V]",		L"723E {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SWAP([V],X)",	L"926E {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"SWAP([V],X]",	L"726E {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"SWAP([V],Y)",	L"916E {1}", ArgType::AT_1BYTE_ADDR);

	// SWAPW
	ADD_INST(L"SWAPWX",			L"5E");
	ADD_INST(L"SWAPWY",			L"905E");

	// TNZ
	ADD_INST(L"TNZA",			L"4D");
	ADD_INST(L"TNZ(V)",			L"3D {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"TNZ(V)",			L"725D {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"TNZ(X)",			L"7D");
	ADD_INST(L"TNZ(V,X)",		L"6D {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"TNZ(V,X)",		L"724D {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"TNZ(Y)",			L"907D");
	ADD_INST(L"TNZ(V,Y)",		L"906D {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"TNZ(V,Y)",		L"904D {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"TNZ(V,SP)",		L"0D {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"TNZ[V]",			L"923D {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"TNZ[V]",			L"723D {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"TNZ([V],X)",		L"926D {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"TNZ([V],X]",		L"726D {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"TNZ([V],Y)",		L"916D {1}", ArgType::AT_1BYTE_ADDR);

	// TNZW
	ADD_INST(L"TNZWX",			L"5D");
	ADD_INST(L"TNZWY",			L"905D");

	// TRAP
	ADD_INST(L"TRAP",			L"83");

	// WFE
	ADD_INST(L"WFE",			L"728F");

	// WFI
	ADD_INST(L"WFI", L"8F");

	// XOR
	ADD_INST(L"XORA,V",			L"A8 {1}", ArgType::AT_1BYTE_VAL);
	ADD_INST(L"XORA,(V)",		L"B8 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"XORA,(V)",		L"C8 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"XORA,(X)",		L"F8");
	ADD_INST(L"XORA,(V,X)",		L"E8 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"XORA,(V,X)",		L"D8 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"XORA,(Y)",		L"90F8");
	ADD_INST(L"XORA,(V,Y)",		L"90E8 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"XORA,(V,Y)",		L"90D8 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"XORA,(V,SP)",	L"18 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"XORA,[V]",		L"92C8 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"XORA,[V]",		L"72C8 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"XORA,([V],X)",	L"92D8 {1}", ArgType::AT_1BYTE_ADDR);
	ADD_INST(L"XORA,([V],X)",	L"72D8 {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST(L"XORA,([V],Y)",	L"91D8 {1}", ArgType::AT_1BYTE_ADDR);
}

static std::multimap<std::wstring, std::unique_ptr<Inst>> _instructions_ex;

#define ADD_INST_EX(SIGN, OPCODE, ...) _instructions_ex.emplace(SIGN, new Inst((OPCODE), ##__VA_ARGS__))

// CALLR -> CALL (if necessary), JRX -> JP (if necessary)
static void load_extra_instructions_small()
{
	// CALLR
	ADD_INST_EX(L"CALLRV",			L"CD {1}", ArgType::AT_2BYTE_ADDR);

	// JRX
	ADD_INST_EX(L"JRAV",			L"CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRTV",			L"CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRCV",			L"2403CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRULTV",			L"2403CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JREQV",			L"2603CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRHV",			L"902803CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRIHV",			L"902E03CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRILV",			L"902F03CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRMV",			L"902C03CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRMIV",			L"2A03CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRNCV",			L"2503CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRUGEV",			L"2503CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRNEV",			L"2703CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRNHV",			L"902903CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRNMV",			L"902D03CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRNVV",			L"2903CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRPLV",			L"2B03CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRSGEV",			L"2F03CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRSGTV",			L"2D03CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRSLEV",			L"2C03CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRSLTV",			L"2E03CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRUGTV",			L"2303CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRULEV",			L"2203CC {1}", ArgType::AT_2BYTE_ADDR);
	ADD_INST_EX(L"JRVV",			L"2803CC {1}", ArgType::AT_2BYTE_ADDR);
}

// JRX -> JPF (if necessary), JP -> JPF, CALL and CALLR -> CALLF, RET -> RETF
static void load_extra_instructions_large()
{
	// CALLR
	ADD_INST_EX(L"CALLRV",			L"8D {1}", ArgType::AT_3BYTE_ADDR);

	// CALL
	ADD_INST_EX(L"CALLV",			L"8D {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"CALL(V)",			L"8D {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"CALL[V]",			L"928D {1}", ArgType::AT_2BYTE_ADDR);

	// JP
	ADD_INST_EX(L"JPV",				L"AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JP(V)",			L"AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JP[V]",			L"92AC {1}", ArgType::AT_2BYTE_ADDR);

	// JRX
	ADD_INST_EX(L"JRAV",			L"AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRTV",			L"AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRCV",			L"2404AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRULTV",			L"2404AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JREQV",			L"2604AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRHV",			L"902804AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRIHV",			L"902E04AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRILV",			L"902F04AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRMV",			L"902C04AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRMIV",			L"2A04AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRNCV",			L"2504AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRUGEV",			L"2504AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRNEV",			L"2704AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRNHV",			L"902904AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRNMV",			L"902D04AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRNVV",			L"2904AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRPLV",			L"2B04AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRSGEV",			L"2F04AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRSGTV",			L"2D04AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRSLEV",			L"2C04AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRSLTV",			L"2E04AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRUGTV",			L"2304AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRULEV",			L"2204AC {1}", ArgType::AT_3BYTE_ADDR);
	ADD_INST_EX(L"JRVV",			L"2804AC {1}", ArgType::AT_3BYTE_ADDR);

	// RET
	ADD_INST_EX(L"RET",				L"87");
}


class A1STM8Settings: public STM8Settings, public A1Settings
{
public:
	A1STM8Settings()
	: STM8Settings()
	, A1Settings()
	{
	}

	A1_T_ERROR GetInstructions(const std::wstring &inst_sign, std::vector<const Inst *> &insts, int line_num, const std::string &file_name) const override
	{
		bool use_ex_opcodes = false;

		// replace JP -> JPF, CALL and CALLR -> CALLF, RET -> RETF
		if(GetFixAddresses() && GetMemModelLarge())
		{
			if(inst_sign == L"JPV" || inst_sign == L"JP(V)" || inst_sign == L"CALLV" || inst_sign == L"CALL(V)" || inst_sign == L"CALLRV" || inst_sign == L"CALLR(V)" || inst_sign == L"RET")
			{
				use_ex_opcodes = true;
			}
		}

		// replace instructions with relative addressing if their addresses are out of range
		if(!use_ex_opcodes && (GetFixAddresses() && IsInstToReplace(line_num, file_name)))
		{
			use_ex_opcodes = true;
		}

		const auto &ginsts = use_ex_opcodes ? _instructions_ex : _instructions;
		
		auto inst_num = ginsts.count(inst_sign);
		
		if(inst_num == 0)
		{
			return A1_T_ERROR::A1_RES_EINVINST;
		}

		// sort the instructions by speed and size in ascending order
		for(auto i = 0; i < insts.size(); i++)
		{
			auto imin = i;
			auto min = insts[i]->_speed * 256 + insts[i]->_size;
			auto min_nxt = 0;
			for(auto j = i + 1; j < insts.size(); j++)
			{
				min_nxt = insts[j]->_speed * 256 + insts[j]->_size;
				if(min_nxt < min)
				{
					imin = j;
					min = min_nxt;
				}
			}
			if(imin != i)
			{
				std::swap(insts[i], insts[imin]);
			}
		}

		auto mi = ginsts.equal_range(inst_sign);
		for(auto inst = mi.first; inst != mi.second; inst++)
		{
			insts.push_back(inst->second.get());
		}

		return A1_T_ERROR::A1_RES_OK;
	}
};


A1STM8Settings global_settings;
A1Settings &_global_settings = global_settings;


class CodeStmtSTM8: public CodeStmt
{
protected:
	A1_T_ERROR GetExpressionSignature(Exp &exp, std::wstring &sign) const override
	{
		static const std::vector<const wchar_t *> _regs = { L"A", L"X", L"XL", L"XH", L"Y", L"YL", L"YH", L"SP", L"CC" };

		std::wstring reg_name;

		sign.clear();

		if(exp.GetSimpleValue(reg_name))
		{
			for(auto r:_regs)
			{
				if(r == reg_name)
				{
					// a register found
					sign += reg_name;
					// clear expression
					exp.Clear();

					return A1_T_ERROR::A1_RES_OK;
				}
			}
		}

		// some value or expression
		sign += L"V";

		return A1_T_ERROR::A1_RES_OK;
	}

	A1_T_ERROR GetInstruction(const std::wstring &signature, const std::map<std::wstring, MemRef> &memrefs, int line_num, const std::string &file_name) override
	{
		std::vector<const Inst *> insts;
		auto err = _global_settings.GetInstructions(signature, insts, line_num, file_name);
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			return err;
		}

		_inst = nullptr;

		int32_t args[A1_MAX_INST_ARGS_NUM] = { 0 };

		for(auto i: insts)
		{
			bool inst_found = true;

			_size = i->_size;
			_inst = i;

			for(auto a = 0; a < i->_argnum; a++)
			{
				int32_t val = -1;

				_refs[a].first = i->_argtypes[a];

				if(_refs[a].first.get().IsRelOffset())
				{
					continue;
				}

				auto err = _refs[a].second.Eval(val, memrefs);
				if(err == A1_T_ERROR::A1_RES_OK)
				{
					if(!_refs[a].first.get().IsValidValue(val))
					{
						inst_found = false;
					}
				}
				else
				{
					inst_found = false;
				}

				args[a] = val;
			}

			if(inst_found)
			{
				break;
			}
		}

		return A1_T_ERROR::A1_RES_OK;
	}

	A1_T_ERROR GetRefValue(const std::pair<std::reference_wrapper<const ArgType>, Exp> &ref, const std::map<std::wstring, MemRef> &memrefs, uint32_t &value, int &size) override
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
			_warnings.insert(A1_T_WARNING::A1_WRN_WINTOUTRANGE);
		}

		value = addr;

		return A1_T_ERROR::A1_RES_OK;
	}

public:
	CodeStmtSTM8()
	: CodeStmt()
	{
	}
};

class CodeInitStmtSTM8: public CodeStmtSTM8
{
public:
	CodeInitStmtSTM8()
	: CodeStmtSTM8()
	{
	}
};

class Page0StmtSTM8 : public DataStmt
{
public:
	Page0StmtSTM8()
	: DataStmt()
	{
	}
};

class STM8Sections : public Sections
{
protected:
	bool CheckSectionName(SectType stype, const std::wstring &type_mod) const override
	{
		if(type_mod == STM8_PAGE0_SECTION_TYPE_MOD)
		{
			return (stype == SectType::ST_DATA);
		}

		if(type_mod.empty())
		{
			return (stype == SectType::ST_HEAP) || (stype == SectType::ST_STACK) || (stype == SectType::ST_DATA) || (stype == SectType::ST_INIT) || (stype == SectType::ST_CONST) || (stype == SectType::ST_CODE);
		}

		return false;
	}

	GenStmt *CreateNewStmt(SectType stype, const std::wstring &type_mod) const override
	{
		switch(stype)
		{
			case SectType::ST_DATA:
				if(type_mod == STM8_PAGE0_SECTION_TYPE_MOD)
				{
					return new Page0StmtSTM8();
				}
				else
				if(type_mod.empty())
				{
					return new DataStmt();
				}
				return nullptr;

			case SectType::ST_HEAP:
				return new HeapStmt();

			case SectType::ST_STACK:
				return new StackStmt();

			case SectType::ST_CONST:
				return new ConstStmt();

			case SectType::ST_CODE:
				return new CodeStmtSTM8();

			case SectType::ST_INIT:
				return new CodeInitStmtSTM8();
		}

		return nullptr;
	}

	A1_T_ERROR ReadDataSections() override
	{
		// read PAGE0 sections
		for(int32_t i = 0; i < _token_files.size(); i++)
		{
			int32_t size = 0;
			auto err = ReadSections(i, SectType::ST_DATA, STM8_PAGE0_SECTION_TYPE_MOD, _global_settings.GetRAMStart() + _data_size, size, _global_settings.GetRAMSize() - _data_size - _global_settings.GetHeapSize());
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				return err;
			}
			_data_size += size;

			if(_data_size > STM8_PAGE0_SIZE)
			{
				_curr_file_name = _src_files[i];
				return A1_T_ERROR::A1_RES_EWSECSIZE;
			}

			if(_data_size + _global_settings.GetHeapSize() + _global_settings.GetStackSize() > _global_settings.GetRAMSize())
			{
				_warnings.push_back(std::make_tuple(-1, _src_files[i], A1_T_WARNING::A1_WRN_EWNORAM));
			}
		}

		return Sections::ReadDataSections();
	}


public:
	STM8Sections()
	: Sections()
	{
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
	bool print_mem_use = false;
	std::vector<std::string> files;
	bool args_error = false;
	std::string args_error_txt;
	A1_T_ERROR err;


	// use current locale
	std::setlocale(LC_ALL, "");


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
					MCU_name = get_MCU_config_name(argv[i]);
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
					_global_settings.SetRetAddressSize(STM8_RET_ADDR_SIZE_MM_SMALL);
				}
				else
				{
					_global_settings.SetMemModelLarge();
					_global_settings.SetRetAddressSize(STM8_RET_ADDR_SIZE_MM_LARGE);
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
					if(Utils::str_toupper(Utils::str_trim(argv[i])) != "STM8")
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
				continue;
			}
		}

		files.push_back(argv[i]);
	}

	_global_settings.SetTargetName("STM8");
	_global_settings.SetMCUName(MCU_name);
	_global_settings.SetLibDirRoot(lib_dir);

	// load target-specific stuff
	if(!select_target(global_settings))
	{
		args_error = true;
		args_error_txt = "invalid target";
	}

	if(args_error || files.empty() && !(print_version))
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

	_global_settings.InitLibDirs();

	// read settings file if specified
	if(!MCU_name.empty())
	{
		auto file_name = _global_settings.GetLibFileName(MCU_name, ".cfg");
		if(!file_name.empty())
		{
			auto err = static_cast<A1_T_ERROR>(_global_settings.Read(file_name));
			if(err != A1_T_ERROR::A1_RES_OK)
			{
				a1_print_error(err, -1, file_name, print_err_desc);
				return 2;
			}
		}
		else
		{
			a1_print_warning(A1_T_WARNING::A1_WRN_WUNKNMCU, -1, MCU_name, _global_settings.GetPrintWarningDesc());
		}

		// initialize library directories a time more to take into account additional ones read from cfg file
		_global_settings.InitLibDirs();
	}


	// prepare output file name
	if(ofn.empty())
	{
		// no output file, use input file's directory and name but with ihx extension
		ofn = files.front();
		auto delpos = ofn.find_last_of("\\/");
		auto pntpos = ofn.find_last_of('.');
		if(pntpos != std::string::npos && (delpos == std::string::npos || pntpos > delpos))
		{
			ofn.erase(pntpos, std::string::npos);
		}
		ofn += ".ihx";
	}
	else
	if(ofn.back() == '\\' || ofn.back() == '/')
	{
		// output directory only, use input file name but with ihx extension
		std::string tmp = files.front();
		auto delpos = tmp.find_last_of("\\/");
		if(delpos != std::string::npos)
		{
			tmp.erase(0, delpos + 1);
		}
		auto pntpos = tmp.find_last_of('.');
		if(pntpos != std::string::npos)
		{
			tmp.erase(pntpos, std::string::npos);
		}
		tmp += ".ihx";
		ofn += tmp;
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


	_B1C_consts[L"__TARGET_NAME"].first = "STM8";
	_B1C_consts[L"__MCU_NAME"].first = MCU_name;


	STM8Sections secs;

	err = secs.ReadSourceFiles(files);
	if(err != A1_T_ERROR::A1_RES_OK)
	{
		if(_global_settings.GetPrintWarnings())
		{
			auto &ws = secs.GetWarnings();
			for(auto &w: ws)
			{
				a1_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
			}
		}

		a1_print_error(err, secs.GetCurrLineNum(), secs.GetCurrFileName(), print_err_desc, secs.GetCustomErrorMsg());
		return 3;
	}

	while(true)
	{
		err = secs.ReadSections();
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			if(_global_settings.GetPrintWarnings())
			{
				auto &ws = secs.GetWarnings();
				for(auto &w: ws)
				{
					a1_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
				}
			}

			a1_print_error(err, secs.GetCurrLineNum(), secs.GetCurrFileName(), print_err_desc, secs.GetCustomErrorMsg());
			return 4;
		}

		err = secs.Write(ofn);
		if(err == A1_T_ERROR::A1_RES_ERELOUTRANGE && _global_settings.GetFixAddresses())
		{
			continue;
		}
		else
		if(err != A1_T_ERROR::A1_RES_OK)
		{
			if(_global_settings.GetPrintWarnings())
			{
				auto &ws = secs.GetWarnings();
				for(auto &w: ws)
				{
					a1_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
				}
			}

			a1_print_error(err, secs.GetCurrLineNum(), secs.GetCurrFileName(), print_err_desc, secs.GetCustomErrorMsg());
			return 5;
		}

		break;
	}

	if(_global_settings.GetPrintWarnings())
	{
		auto &ws = secs.GetWarnings();
		for(auto &w: ws)
		{
			a1_print_warning(std::get<2>(w), std::get<0>(w), std::get<1>(w), _global_settings.GetPrintWarningDesc());
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
