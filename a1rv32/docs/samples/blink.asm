; blink sample for CH32V003F4P6 MCU with a LED connected to PD2
; command line to build the sample: a1rv32 -d -f -a -m CH32V003F4 blink.asm

.STACK
DB (0x200)


.CODE INIT
; initialize stack
LI SP, __DATA_START + __DATA_TOTAL_SIZE

; enable port D module clock
LW A0, RCC_APB2PCENR
ORI A0, A0, RCC_APB2PCENR_IOPDEN
SW A0, RCC_APB2PCENR

; configure PD2 for output (50 MHz, push-pull)
LW A0, GPIOD_CFGLR
ANDI A0, A0, 0xFFFFF0FF
ORI A0, A0, 3 << 8
SW A0, GPIOD_CFGLR

:__BLINK_LOOP
; invert PD2 pin
LHU A0, GPIOD_OUTDR
XORI A0, A0, 1 << 2
SH A0, GPIOD_OUTDR
; 500 ms delay
LI A1, 500
CALL DELAY_MS
J __BLINK_LOOP


.ALIGN 4
; delay function, A1 - delay value (in ms)
:DELAY_MS
I32.BEQ A1, ZERO, __EXIT
I32.LI T2, 1349 ; the MCU starts at 8 MHz clock speed
I32.LI T1, 0

:__DELAY_LOOP
I32.MV A0, T2

:__DELAY_LOOP_1MS
I32.ADDI A0, A0, -1
I32.NOP
I32.NOP
I32.BNE A0, ZERO, __DELAY_LOOP_1MS
I32.ADDI T1, T1, 1
I32.BNE A1, T1, __DELAY_LOOP

:__EXIT
I32.RET
