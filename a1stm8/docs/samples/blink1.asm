; blink sample for STM8S103F3P6 MCU with a LED on PB5
; using symbolic constants instead of register addresses requires -m command-line option with MCU name
; e.g. a1stm8 -d -m STM8S103F3 blink1.asm
.CODE INIT
INT __START

:__START

; initialize GPIO PB5 pin as output push-pull
MOV (PB_CR1), 1 << GPIO_PIN5_POS
MOV (PB_CR2), 1 << GPIO_PIN5_POS
MOV (PB_DDR), 1 << GPIO_PIN5_POS

:__BLINK_LOOP
; read port B pins
LD A, (PB_ODR)
; invert value of pin #5
XOR A, 1 << 5
; write the values back
LD (PB_ODR), A

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
