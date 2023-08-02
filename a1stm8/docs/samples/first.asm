; the simplest program: does nothing
; a1stm8 -d first.asm
; .CODE INIT section - program execution starts here
.CODE INIT
; STM8 interrupt vector table, the only interrupt handler is for RESET event
INT __START
; INT instruction passes execution here
:__START
; jump on itself (just an infinite loop)
JRA __START