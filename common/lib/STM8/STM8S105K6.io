[UART2]
; name       id  call_type mask accept_data  data_type  predef_only  [val0_name  val0_value...]
INPUTECHO,   0,  INL,      2,   TRUE,        BYTE,      TRUE,        ON,2,         OFF,0,         ENABLE,2,      DISABLE,0
RX,          0,  INL,      4,   TRUE,        BYTE,      TRUE,        ON,4,         OFF,0,         ENABLE,4,      DISABLE,0
TX,          0,  INL,      8,   TRUE,        BYTE,      TRUE,        ON,8,         OFF,0,         ENABLE,8,      DISABLE,0
SPEED,       1,  INL,      ,    TRUE,        WORD,      TRUE,        9600,1667,    14400,1111,    19200,833,     38400,417,     57600,278,     115200,139
PINS,        2,  INL,      ,    TRUE,        BYTE,      TRUE,        DEFAULT,0
NEWLINE,     3,  CALL,     ,    TRUE,        STRING,    FALSE
MARGIN,      4,  INL,      ,    TRUE,        BYTE,      FALSE
ZONEWIDTH,   5,  INL,      ,    TRUE,        BYTE,      FALSE
START,       6,  CALL,     ,    FALSE
STOP,        7,  CALL,     ,    FALSE
ENABLE,      8,  INL,      ,    FALSE
DISABLE,     9,  INL,      ,    FALSE

[CPU]
INTERRUPTS,  0,  INL,      ,    TRUE,        BYTE,      TRUE,        ON,1,         OFF,0,         ENABLE,1,      DISABLE,0
CLOCKSOURCE, 1,  CALL,     ,    TRUE,        WORD,      TRUE,        HSI,0xE100,   HSI16,0xE100,  LSI,0xD203,    HSE16,0xB400,  HSE8,0xB401
WAIT,        2,  INL,      ,    TRUE,        BYTE,      TRUE,        INTERRUPT,0

[TIM2]
PRESCALER,   0,  INL,      ,    TRUE,        BYTE,      TRUE,        DIV1,0, DIV2,1, DIV4,2, DIV8,3, DIV16,4, DIV32,5, DIV64,6, DIV128,7, DIV256,8, DIV512,9, DIV1024,10, DIV2048,11, DIV4096,12, DIV8192,13, DIV16384,14, DIV32768,15
VALUE,       1,  INL,      ,    TRUE,        WORD,      FALSE
PERIODMS,    2,  CALL,     ,    TRUE,        WORD,      FALSE
INTERRUPT,   3,  INL,      ,    TRUE,        BYTE,      TRUE,        ON,1,         OFF,0,         ENABLE,1,      DISABLE,0
CLRINTFLAG,  4,  INL,      ,    FALSE
START,       5,  CALL,     ,    FALSE
STOP,        6,  INL,      ,    FALSE
ENABLE,      7,  INL,      ,    FALSE
DISABLE,     8,  INL,      ,    FALSE

[PA]
; name       id  call_type mask       accept_data  data_type  predef_only  [val0_name  val0_value...]
PIN0,        0,  INL,      0x010101,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,1, IN_PULLUP_NOEXTI,0x100, IN_PULLUP_EXTI,0x101, OUT_OPENDRAIN_SLOW,0x10000, OUT_OPENDRAIN_FAST,0x10001, OUT_PUSHPULL_SLOW,0x10100, OUT_PUSHPULL_FAST,0x10101
PIN1,        0,  INL,      0x020202,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,2, IN_PULLUP_NOEXTI,0x200, IN_PULLUP_EXTI,0x202, OUT_OPENDRAIN_SLOW,0x20000, OUT_OPENDRAIN_FAST,0x20002, OUT_PUSHPULL_SLOW,0x20200, OUT_PUSHPULL_FAST,0x20202
PIN2,        0,  INL,      0x040404,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,4, IN_PULLUP_NOEXTI,0x400, IN_PULLUP_EXTI,0x404, OUT_OPENDRAIN_SLOW,0x40000, OUT_OPENDRAIN_FAST,0x40004, OUT_PUSHPULL_SLOW,0x40400, OUT_PUSHPULL_FAST,0x40404
PIN3,        0,  INL,      0x080808,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,8, IN_PULLUP_NOEXTI,0x800, IN_PULLUP_EXTI,0x808, OUT_OPENDRAIN_SLOW,0x80000, OUT_OPENDRAIN_FAST,0x80008, OUT_PUSHPULL_SLOW,0x80800, OUT_PUSHPULL_FAST,0x80808
PIN4,        0,  INL,      0x101010,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x10, IN_PULLUP_NOEXTI,0x1000, IN_PULLUP_EXTI,0x1010, OUT_OPENDRAIN_SLOW,0x100000, OUT_OPENDRAIN_FAST,0x100010, OUT_PUSHPULL_SLOW,0x101000, OUT_PUSHPULL_FAST,0x101010
PIN5,        0,  INL,      0x202020,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x20, IN_PULLUP_NOEXTI,0x2000, IN_PULLUP_EXTI,0x2020, OUT_OPENDRAIN_SLOW,0x200000, OUT_OPENDRAIN_FAST,0x200020, OUT_PUSHPULL_SLOW,0x202000, OUT_PUSHPULL_FAST,0x202020
PIN6,        0,  INL,      0x404040,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x40, IN_PULLUP_NOEXTI,0x4000, IN_PULLUP_EXTI,0x4040, OUT_OPENDRAIN_SLOW,0x400000, OUT_OPENDRAIN_FAST,0x400040, OUT_PUSHPULL_SLOW,0x404000, OUT_PUSHPULL_FAST,0x404040
PIN7,        0,  INL,      0x808080,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x80, IN_PULLUP_NOEXTI,0x8000, IN_PULLUP_EXTI,0x8080, OUT_OPENDRAIN_SLOW,0x800000, OUT_OPENDRAIN_FAST,0x800080, OUT_PUSHPULL_SLOW,0x808000, OUT_PUSHPULL_FAST,0x808080
ENABLE,      1,  INL,      ,          FALSE
DISABLE,     2,  INL,      ,          FALSE

[PB]
; name       id  call_type mask       accept_data  data_type  predef_only  [val0_name  val0_value...]
PIN0,        0,  INL,      0x10101,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,1, IN_PULLUP_NOEXTI,0x100, IN_PULLUP_EXTI,0x101, OUT_OPENDRAIN_SLOW,0x10000, OUT_OPENDRAIN_FAST,0x10001, OUT_PUSHPULL_SLOW,0x10100, OUT_PUSHPULL_FAST,0x10101
PIN1,        0,  INL,      0x20202,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,2, IN_PULLUP_NOEXTI,0x200, IN_PULLUP_EXTI,0x202, OUT_OPENDRAIN_SLOW,0x20000, OUT_OPENDRAIN_FAST,0x20002, OUT_PUSHPULL_SLOW,0x20200, OUT_PUSHPULL_FAST,0x20202
PIN2,        0,  INL,      0x40404,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,4, IN_PULLUP_NOEXTI,0x400, IN_PULLUP_EXTI,0x404, OUT_OPENDRAIN_SLOW,0x40000, OUT_OPENDRAIN_FAST,0x40004, OUT_PUSHPULL_SLOW,0x40400, OUT_PUSHPULL_FAST,0x40404
PIN3,        0,  INL,      0x80808,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,8, IN_PULLUP_NOEXTI,0x800, IN_PULLUP_EXTI,0x808, OUT_OPENDRAIN_SLOW,0x80000, OUT_OPENDRAIN_FAST,0x80008, OUT_PUSHPULL_SLOW,0x80800, OUT_PUSHPULL_FAST,0x80808
PIN4,        0,  INL,      0x101010,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x10, IN_PULLUP_NOEXTI,0x1000, IN_PULLUP_EXTI,0x1010, OUT_OPENDRAIN_SLOW,0x100000, OUT_OPENDRAIN_FAST,0x100010, OUT_PUSHPULL_SLOW,0x101000, OUT_PUSHPULL_FAST,0x101010
PIN5,        0,  INL,      0x202020,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x20, IN_PULLUP_NOEXTI,0x2000, IN_PULLUP_EXTI,0x2020, OUT_OPENDRAIN_SLOW,0x200000, OUT_OPENDRAIN_FAST,0x200020, OUT_PUSHPULL_SLOW,0x202000, OUT_PUSHPULL_FAST,0x202020
PIN6,        0,  INL,      0x404040,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x40, IN_PULLUP_NOEXTI,0x4000, IN_PULLUP_EXTI,0x4040, OUT_OPENDRAIN_SLOW,0x400000, OUT_OPENDRAIN_FAST,0x400040, OUT_PUSHPULL_SLOW,0x404000, OUT_PUSHPULL_FAST,0x404040
PIN7,        0,  INL,      0x808080,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x80, IN_PULLUP_NOEXTI,0x8000, IN_PULLUP_EXTI,0x8080, OUT_OPENDRAIN_SLOW,0x800000, OUT_OPENDRAIN_FAST,0x800080, OUT_PUSHPULL_SLOW,0x808000, OUT_PUSHPULL_FAST,0x808080
ENABLE,      1,  INL,      ,          FALSE
DISABLE,     2,  INL,      ,          FALSE

[PC]
; name       id  call_type mask       accept_data  data_type  predef_only  [val0_name  val0_value...]
PIN0,        0,  INL,      0x10101,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,1, IN_PULLUP_NOEXTI,0x100, IN_PULLUP_EXTI,0x101, OUT_OPENDRAIN_SLOW,0x10000, OUT_OPENDRAIN_FAST,0x10001, OUT_PUSHPULL_SLOW,0x10100, OUT_PUSHPULL_FAST,0x10101
PIN1,        0,  INL,      0x20202,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,2, IN_PULLUP_NOEXTI,0x200, IN_PULLUP_EXTI,0x202, OUT_OPENDRAIN_SLOW,0x20000, OUT_OPENDRAIN_FAST,0x20002, OUT_PUSHPULL_SLOW,0x20200, OUT_PUSHPULL_FAST,0x20202
PIN2,        0,  INL,      0x40404,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,4, IN_PULLUP_NOEXTI,0x400, IN_PULLUP_EXTI,0x404, OUT_OPENDRAIN_SLOW,0x40000, OUT_OPENDRAIN_FAST,0x40004, OUT_PUSHPULL_SLOW,0x40400, OUT_PUSHPULL_FAST,0x40404
PIN3,        0,  INL,      0x80808,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,8, IN_PULLUP_NOEXTI,0x800, IN_PULLUP_EXTI,0x808, OUT_OPENDRAIN_SLOW,0x80000, OUT_OPENDRAIN_FAST,0x80008, OUT_PUSHPULL_SLOW,0x80800, OUT_PUSHPULL_FAST,0x80808
PIN4,        0,  INL,      0x101010,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x10, IN_PULLUP_NOEXTI,0x1000, IN_PULLUP_EXTI,0x1010, OUT_OPENDRAIN_SLOW,0x100000, OUT_OPENDRAIN_FAST,0x100010, OUT_PUSHPULL_SLOW,0x101000, OUT_PUSHPULL_FAST,0x101010
PIN5,        0,  INL,      0x202020,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x20, IN_PULLUP_NOEXTI,0x2000, IN_PULLUP_EXTI,0x2020, OUT_OPENDRAIN_SLOW,0x200000, OUT_OPENDRAIN_FAST,0x200020, OUT_PUSHPULL_SLOW,0x202000, OUT_PUSHPULL_FAST,0x202020
PIN6,        0,  INL,      0x404040,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x40, IN_PULLUP_NOEXTI,0x4000, IN_PULLUP_EXTI,0x4040, OUT_OPENDRAIN_SLOW,0x400000, OUT_OPENDRAIN_FAST,0x400040, OUT_PUSHPULL_SLOW,0x404000, OUT_PUSHPULL_FAST,0x404040
PIN7,        0,  INL,      0x808080,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x80, IN_PULLUP_NOEXTI,0x8000, IN_PULLUP_EXTI,0x8080, OUT_OPENDRAIN_SLOW,0x800000, OUT_OPENDRAIN_FAST,0x800080, OUT_PUSHPULL_SLOW,0x808000, OUT_PUSHPULL_FAST,0x808080
ENABLE,      1,  INL,      ,          FALSE
DISABLE,     2,  INL,      ,          FALSE

[PD]
; name       id  call_type mask       accept_data  data_type  predef_only  [val0_name  val0_value...]
PIN0,        0,  INL,      0x10101,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,1, IN_PULLUP_NOEXTI,0x100, IN_PULLUP_EXTI,0x101, OUT_OPENDRAIN_SLOW,0x10000, OUT_OPENDRAIN_FAST,0x10001, OUT_PUSHPULL_SLOW,0x10100, OUT_PUSHPULL_FAST,0x10101
PIN1,        0,  INL,      0x20202,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,2, IN_PULLUP_NOEXTI,0x200, IN_PULLUP_EXTI,0x202, OUT_OPENDRAIN_SLOW,0x20000, OUT_OPENDRAIN_FAST,0x20002, OUT_PUSHPULL_SLOW,0x20200, OUT_PUSHPULL_FAST,0x20202
PIN2,        0,  INL,      0x40404,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,4, IN_PULLUP_NOEXTI,0x400, IN_PULLUP_EXTI,0x404, OUT_OPENDRAIN_SLOW,0x40000, OUT_OPENDRAIN_FAST,0x40004, OUT_PUSHPULL_SLOW,0x40400, OUT_PUSHPULL_FAST,0x40404
PIN3,        0,  INL,      0x80808,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,8, IN_PULLUP_NOEXTI,0x800, IN_PULLUP_EXTI,0x808, OUT_OPENDRAIN_SLOW,0x80000, OUT_OPENDRAIN_FAST,0x80008, OUT_PUSHPULL_SLOW,0x80800, OUT_PUSHPULL_FAST,0x80808
PIN4,        0,  INL,      0x101010,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x10, IN_PULLUP_NOEXTI,0x1000, IN_PULLUP_EXTI,0x1010, OUT_OPENDRAIN_SLOW,0x100000, OUT_OPENDRAIN_FAST,0x100010, OUT_PUSHPULL_SLOW,0x101000, OUT_PUSHPULL_FAST,0x101010
PIN5,        0,  INL,      0x202020,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x20, IN_PULLUP_NOEXTI,0x2000, IN_PULLUP_EXTI,0x2020, OUT_OPENDRAIN_SLOW,0x200000, OUT_OPENDRAIN_FAST,0x200020, OUT_PUSHPULL_SLOW,0x202000, OUT_PUSHPULL_FAST,0x202020
PIN6,        0,  INL,      0x404040,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x40, IN_PULLUP_NOEXTI,0x4000, IN_PULLUP_EXTI,0x4040, OUT_OPENDRAIN_SLOW,0x400000, OUT_OPENDRAIN_FAST,0x400040, OUT_PUSHPULL_SLOW,0x404000, OUT_PUSHPULL_FAST,0x404040
PIN7,        0,  INL,      0x808080,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x80, IN_PULLUP_NOEXTI,0x8000, IN_PULLUP_EXTI,0x8080, OUT_OPENDRAIN_SLOW,0x800000, OUT_OPENDRAIN_FAST,0x800080, OUT_PUSHPULL_SLOW,0x808000, OUT_PUSHPULL_FAST,0x808080
ENABLE,      1,  INL,      ,          FALSE
DISABLE,     2,  INL,      ,          FALSE

[PE]
; name       id  call_type mask       accept_data  data_type  predef_only  [val0_name  val0_value...]
PIN0,        0,  INL,      0x10101,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,1, IN_PULLUP_NOEXTI,0x100, IN_PULLUP_EXTI,0x101, OUT_OPENDRAIN_SLOW,0x10000, OUT_OPENDRAIN_FAST,0x10001, OUT_PUSHPULL_SLOW,0x10100, OUT_PUSHPULL_FAST,0x10101
PIN1,        0,  INL,      0x20202,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,2, IN_PULLUP_NOEXTI,0x200, IN_PULLUP_EXTI,0x202, OUT_OPENDRAIN_SLOW,0x20000, OUT_OPENDRAIN_FAST,0x20002, OUT_PUSHPULL_SLOW,0x20200, OUT_PUSHPULL_FAST,0x20202
PIN2,        0,  INL,      0x40404,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,4, IN_PULLUP_NOEXTI,0x400, IN_PULLUP_EXTI,0x404, OUT_OPENDRAIN_SLOW,0x40000, OUT_OPENDRAIN_FAST,0x40004, OUT_PUSHPULL_SLOW,0x40400, OUT_PUSHPULL_FAST,0x40404
PIN3,        0,  INL,      0x80808,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,8, IN_PULLUP_NOEXTI,0x800, IN_PULLUP_EXTI,0x808, OUT_OPENDRAIN_SLOW,0x80000, OUT_OPENDRAIN_FAST,0x80008, OUT_PUSHPULL_SLOW,0x80800, OUT_PUSHPULL_FAST,0x80808
PIN4,        0,  INL,      0x101010,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x10, IN_PULLUP_NOEXTI,0x1000, IN_PULLUP_EXTI,0x1010, OUT_OPENDRAIN_SLOW,0x100000, OUT_OPENDRAIN_FAST,0x100010, OUT_PUSHPULL_SLOW,0x101000, OUT_PUSHPULL_FAST,0x101010
PIN5,        0,  INL,      0x202020,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x20, IN_PULLUP_NOEXTI,0x2000, IN_PULLUP_EXTI,0x2020, OUT_OPENDRAIN_SLOW,0x200000, OUT_OPENDRAIN_FAST,0x200020, OUT_PUSHPULL_SLOW,0x202000, OUT_PUSHPULL_FAST,0x202020
PIN6,        0,  INL,      0x404040,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x40, IN_PULLUP_NOEXTI,0x4000, IN_PULLUP_EXTI,0x4040, OUT_OPENDRAIN_SLOW,0x400000, OUT_OPENDRAIN_FAST,0x400040, OUT_PUSHPULL_SLOW,0x404000, OUT_PUSHPULL_FAST,0x404040
PIN7,        0,  INL,      0x808080,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x80, IN_PULLUP_NOEXTI,0x8000, IN_PULLUP_EXTI,0x8080, OUT_OPENDRAIN_SLOW,0x800000, OUT_OPENDRAIN_FAST,0x800080, OUT_PUSHPULL_SLOW,0x808000, OUT_PUSHPULL_FAST,0x808080
ENABLE,      1,  INL,      ,          FALSE
DISABLE,     2,  INL,      ,          FALSE

[PF]
; name       id  call_type mask       accept_data  data_type  predef_only  [val0_name  val0_value...]
PIN0,        0,  INL,      0x10101,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,1, IN_PULLUP_NOEXTI,0x100, IN_PULLUP_EXTI,0x101, OUT_OPENDRAIN_SLOW,0x10000, OUT_OPENDRAIN_FAST,0x10001, OUT_PUSHPULL_SLOW,0x10100, OUT_PUSHPULL_FAST,0x10101
PIN1,        0,  INL,      0x20202,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,2, IN_PULLUP_NOEXTI,0x200, IN_PULLUP_EXTI,0x202, OUT_OPENDRAIN_SLOW,0x20000, OUT_OPENDRAIN_FAST,0x20002, OUT_PUSHPULL_SLOW,0x20200, OUT_PUSHPULL_FAST,0x20202
PIN2,        0,  INL,      0x40404,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,4, IN_PULLUP_NOEXTI,0x400, IN_PULLUP_EXTI,0x404, OUT_OPENDRAIN_SLOW,0x40000, OUT_OPENDRAIN_FAST,0x40004, OUT_PUSHPULL_SLOW,0x40400, OUT_PUSHPULL_FAST,0x40404
PIN3,        0,  INL,      0x80808,   TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,8, IN_PULLUP_NOEXTI,0x800, IN_PULLUP_EXTI,0x808, OUT_OPENDRAIN_SLOW,0x80000, OUT_OPENDRAIN_FAST,0x80008, OUT_PUSHPULL_SLOW,0x80800, OUT_PUSHPULL_FAST,0x80808
PIN4,        0,  INL,      0x101010,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x10, IN_PULLUP_NOEXTI,0x1000, IN_PULLUP_EXTI,0x1010, OUT_OPENDRAIN_SLOW,0x100000, OUT_OPENDRAIN_FAST,0x100010, OUT_PUSHPULL_SLOW,0x101000, OUT_PUSHPULL_FAST,0x101010
PIN5,        0,  INL,      0x202020,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x20, IN_PULLUP_NOEXTI,0x2000, IN_PULLUP_EXTI,0x2020, OUT_OPENDRAIN_SLOW,0x200000, OUT_OPENDRAIN_FAST,0x200020, OUT_PUSHPULL_SLOW,0x202000, OUT_PUSHPULL_FAST,0x202020
PIN6,        0,  INL,      0x404040,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x40, IN_PULLUP_NOEXTI,0x4000, IN_PULLUP_EXTI,0x4040, OUT_OPENDRAIN_SLOW,0x400000, OUT_OPENDRAIN_FAST,0x400040, OUT_PUSHPULL_SLOW,0x404000, OUT_PUSHPULL_FAST,0x404040
PIN7,        0,  INL,      0x808080,  TRUE,        LONG,      TRUE,        IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x80, IN_PULLUP_NOEXTI,0x8000, IN_PULLUP_EXTI,0x8080, OUT_OPENDRAIN_SLOW,0x800000, OUT_OPENDRAIN_FAST,0x800080, OUT_PUSHPULL_SLOW,0x808000, OUT_PUSHPULL_FAST,0x808080
ENABLE,      1,  INL,      ,          FALSE
DISABLE,     2,  INL,      ,          FALSE
