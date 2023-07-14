; blink sample for STM8S103F3P6 MCU with a LED on PB5
; a1stm8 -d blink.asm
.CODE INIT
INT __START

:__START

; initialize GPIO PB5 pin as output push-pull
MOV (0x5008), 1 << 5 ; PB_CR1 register
MOV (0x5009), 1 << 5 ; PB_CR2 register
MOV (0x5007), 1 << 5 ; PB_DDR register

:__BLINK_LOOP
; read port B pins
LD A, (0x5005) ; PB_ODR register
; invert value of pin #5
XOR A, 1 << 5
; write the values back
LD (0x5005), A

; delay
CLRW X
:__DELAY_LOOP
NOP
NOP
NOP
NOP
NOP
NOP
INCW X
JRNE __DELAY_LOOP

JRA __BLINK_LOOP
