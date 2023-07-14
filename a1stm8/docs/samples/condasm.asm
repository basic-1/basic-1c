; conditional assembly sample
; to build with small memory model (2-byte code addresses):
; a1stm8.exe -d -mu -ms condasm.asm
; to build with large memory model (3-byte code addresses):
; a1stm8.exe -d -mu -ml condasm.asm

.CODE INIT
INT __START
:__START
.IF __RET_ADDR_SIZE == 2
CALLR __TEST_FN
.ELSE
CALLF __TEST_FN
.ENDIF
JRA __START

:__TEST_FN
.IF __RET_ADDR_SIZE == 2
RET
.ELSE
RETF
.ENDIF
