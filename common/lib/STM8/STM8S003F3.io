[UART1]
; name         id  call_type placement file_name mask accept_data  data_type  extra_data predef_only  val_num  val0_name,val0_value...                                                                    def_val
INPUTECHO,     0,  INL,      ,         ,         2,   TRUE,        BYTE,      ,          TRUE,        4,       ON,2,         OFF,0,         ENABLE,2,      DISABLE,0
TRANSMODE,     0,  INL,      ,         ,         16,  TRUE,        BYTE,      ,          TRUE,        2,       DUPLEX,0,     SIMPLEX,16
CFGPINS,       0,  INL,      ,         ,         64,  TRUE,        BYTE,      ,          TRUE,        4,       ON,64,        OFF,0,         ENABLE,64,     DISABLE,0,                                     ON
SPEED,         1,  INL,      ,         ,         ,    TRUE,        WORD,      ,          TRUE,        7,       1200,13333,   9600,1667,     14400,1111,    19200,833,     38400,417,     57600,278,     115200,139
NEWLINE,       3,  CALL,     ,         ,         ,    TRUE,        STRING,    ,          FALSE
MARGIN,        4,  INL,      ,         ,         ,    TRUE,        BYTE,      IMR,       FALSE
ZONEWIDTH,     5,  INL,      ,         ,         ,    TRUE,        BYTE,      IMR,       FALSE
START,         6,  CALL,     ,         ,         ,    TRUE,        BYTE,      ,          TRUE,        2,       RX,4,         TX,8,                                                                        TX
STOP,          7,  CALL,     ,         ,         ,    FALSE
ENABLE,        8,  INL,      ,         ,         ,    FALSE
DISABLE,       9,  INL,      ,         ,         ,    FALSE

[CPU]
INTERRUPTS,    0,  INL,      ,         ,         ,    TRUE,        BYTE,      ,          TRUE,        4,       ON,1,         OFF,0,         ENABLE,1,      DISABLE,0
CLOCKSOURCE,   1,  CALL,     ,         ,         ,    TRUE,        WORD,      ,          TRUE,        6,       HSI,0xE100,   HSI16,0xE100,  HSI8,0xE111,   LSI,0xD203,    HSE16,0xB400,  HSE8,0xB401
WAIT,          2,  INL,      ,         ,         ,    TRUE,        BYTE,      ,          TRUE,        1,       INTERRUPT,0
DELAYMS,       3,  CALL,     ,         ,         ,    TRUE,        BYTE,      ,          FALSE

[TIM2]
PRESCALER,     0,  INL,      ,         ,         ,    TRUE,        BYTE,      ,          TRUE,        16,      DIV1,0, DIV2,1, DIV4,2, DIV8,3, DIV16,4, DIV32,5, DIV64,6, DIV128,7, DIV256,8, DIV512,9, DIV1024,10, DIV2048,11, DIV4096,12, DIV8192,13, DIV16384,14, DIV32768,15
VALUE,         1,  INL,      ,         ,         ,    TRUE,        WORD,      IMR,       FALSE
PERIODMS,      2,  CALL,     ,         ,         ,    TRUE,        WORD,      ,          FALSE
INTERRUPT,     3,  INL,      ,         ,         ,    TRUE,        BYTE,      ,          TRUE,        4,       ON,1,         OFF,0,         ENABLE,1,      DISABLE,0
CLRINTFLAG,    4,  INL,      ,         ,         ,    FALSE
START,         5,  CALL,     ,         ,         ,    FALSE
STOP,          6,  INL,      ,         ,         ,    FALSE
ENABLE,        7,  INL,      ,         ,         ,    FALSE
DISABLE,       8,  INL,      ,         ,         ,    FALSE
ONUPDOVF,      9,  INL,      END,      ,         ,    TRUE,        LABEL,     ,          FALSE

[PA,PB,PC,PD]
; name         id  call_type placement file_name            mask       accept_data  data_type  extra_data predef_only  val_num  val0_name,val0_value...
CFGPIN0,       0,  INL,      ,         __LIB_GPIO_CFG_INL,  0x010101,  TRUE,        LONG,      ,          TRUE,        8,       IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,1, IN_PULLUP_NOEXTI,0x100, IN_PULLUP_EXTI,0x101, OUT_OPENDRAIN_SLOW,0x10000, OUT_OPENDRAIN_FAST,0x10001, OUT_PUSHPULL_SLOW,0x10100, OUT_PUSHPULL_FAST,0x10101
CFGPIN1,       0,  INL,      ,         __LIB_GPIO_CFG_INL,  0x020202,  TRUE,        LONG,      ,          TRUE,        8,       IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,2, IN_PULLUP_NOEXTI,0x200, IN_PULLUP_EXTI,0x202, OUT_OPENDRAIN_SLOW,0x20000, OUT_OPENDRAIN_FAST,0x20002, OUT_PUSHPULL_SLOW,0x20200, OUT_PUSHPULL_FAST,0x20202
CFGPIN2,       0,  INL,      ,         __LIB_GPIO_CFG_INL,  0x040404,  TRUE,        LONG,      ,          TRUE,        8,       IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,4, IN_PULLUP_NOEXTI,0x400, IN_PULLUP_EXTI,0x404, OUT_OPENDRAIN_SLOW,0x40000, OUT_OPENDRAIN_FAST,0x40004, OUT_PUSHPULL_SLOW,0x40400, OUT_PUSHPULL_FAST,0x40404
CFGPIN3,       0,  INL,      ,         __LIB_GPIO_CFG_INL,  0x080808,  TRUE,        LONG,      ,          TRUE,        8,       IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,8, IN_PULLUP_NOEXTI,0x800, IN_PULLUP_EXTI,0x808, OUT_OPENDRAIN_SLOW,0x80000, OUT_OPENDRAIN_FAST,0x80008, OUT_PUSHPULL_SLOW,0x80800, OUT_PUSHPULL_FAST,0x80808
CFGPIN4,       0,  INL,      ,         __LIB_GPIO_CFG_INL,  0x101010,  TRUE,        LONG,      ,          TRUE,        8,       IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x10, IN_PULLUP_NOEXTI,0x1000, IN_PULLUP_EXTI,0x1010, OUT_OPENDRAIN_SLOW,0x100000, OUT_OPENDRAIN_FAST,0x100010, OUT_PUSHPULL_SLOW,0x101000, OUT_PUSHPULL_FAST,0x101010
CFGPIN5,       0,  INL,      ,         __LIB_GPIO_CFG_INL,  0x202020,  TRUE,        LONG,      ,          TRUE,        8,       IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x20, IN_PULLUP_NOEXTI,0x2000, IN_PULLUP_EXTI,0x2020, OUT_OPENDRAIN_SLOW,0x200000, OUT_OPENDRAIN_FAST,0x200020, OUT_PUSHPULL_SLOW,0x202000, OUT_PUSHPULL_FAST,0x202020
CFGPIN6,       0,  INL,      ,         __LIB_GPIO_CFG_INL,  0x404040,  TRUE,        LONG,      ,          TRUE,        8,       IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x40, IN_PULLUP_NOEXTI,0x4000, IN_PULLUP_EXTI,0x4040, OUT_OPENDRAIN_SLOW,0x400000, OUT_OPENDRAIN_FAST,0x400040, OUT_PUSHPULL_SLOW,0x404000, OUT_PUSHPULL_FAST,0x404040
CFGPIN7,       0,  INL,      ,         __LIB_GPIO_CFG_INL,  0x808080,  TRUE,        LONG,      ,          TRUE,        8,       IN_FLOAT_NOEXTI,0, IN_FLOAT_EXTI,0x80, IN_PULLUP_NOEXTI,0x8000, IN_PULLUP_EXTI,0x8080, OUT_OPENDRAIN_SLOW,0x800000, OUT_OPENDRAIN_FAST,0x800080, OUT_PUSHPULL_SLOW,0x808000, OUT_PUSHPULL_FAST,0x808080
ENABLE,        1,  INL,      ,         __LIB_GPIO_ENA_INL,  ,          FALSE
DISABLE,       2,  INL,      ,         __LIB_GPIO_DIS_INL,  ,          FALSE
SETPIN0,       3,  INL,      ,         __LIB_GPIO_SET_INL,  ,          FALSE
SETPIN1,       3,  INL,      ,         __LIB_GPIO_SET_INL,  ,          FALSE
SETPIN2,       3,  INL,      ,         __LIB_GPIO_SET_INL,  ,          FALSE
SETPIN3,       3,  INL,      ,         __LIB_GPIO_SET_INL,  ,          FALSE
SETPIN4,       3,  INL,      ,         __LIB_GPIO_SET_INL,  ,          FALSE
SETPIN5,       3,  INL,      ,         __LIB_GPIO_SET_INL,  ,          FALSE
SETPIN6,       3,  INL,      ,         __LIB_GPIO_SET_INL,  ,          FALSE
SETPIN7,       3,  INL,      ,         __LIB_GPIO_SET_INL,  ,          FALSE
CLRPIN0,       4,  INL,      ,         __LIB_GPIO_CLR_INL,  ,          FALSE
CLRPIN1,       4,  INL,      ,         __LIB_GPIO_CLR_INL,  ,          FALSE
CLRPIN2,       4,  INL,      ,         __LIB_GPIO_CLR_INL,  ,          FALSE
CLRPIN3,       4,  INL,      ,         __LIB_GPIO_CLR_INL,  ,          FALSE
CLRPIN4,       4,  INL,      ,         __LIB_GPIO_CLR_INL,  ,          FALSE
CLRPIN5,       4,  INL,      ,         __LIB_GPIO_CLR_INL,  ,          FALSE
CLRPIN6,       4,  INL,      ,         __LIB_GPIO_CLR_INL,  ,          FALSE
CLRPIN7,       4,  INL,      ,         __LIB_GPIO_CLR_INL,  ,          FALSE
INVPIN0,       5,  INL,      ,         __LIB_GPIO_INV_INL,  ,          FALSE
INVPIN1,       5,  INL,      ,         __LIB_GPIO_INV_INL,  ,          FALSE
INVPIN2,       5,  INL,      ,         __LIB_GPIO_INV_INL,  ,          FALSE
INVPIN3,       5,  INL,      ,         __LIB_GPIO_INV_INL,  ,          FALSE
INVPIN4,       5,  INL,      ,         __LIB_GPIO_INV_INL,  ,          FALSE
INVPIN5,       5,  INL,      ,         __LIB_GPIO_INV_INL,  ,          FALSE
INVPIN6,       5,  INL,      ,         __LIB_GPIO_INV_INL,  ,          FALSE
INVPIN7,       5,  INL,      ,         __LIB_GPIO_INV_INL,  ,          FALSE

[SPI]
; name         id  call_type placement file_name            mask       accept_data  data_type  extra_data predef_only  val_num  val0_name,val0_value...                                                                   def_val
MODE,          0,  INL,      ,         ,                    3,         TRUE,        BYTE,      ,          TRUE,        12,      MODE0,0, MODE1,1, MODE2,2, MODE3,3, M0,0, M1,1, M2,2, M3,3, 0,0, 1,1, 2,2, 3,3
MASTER,        0,  INL,      ,         ,                    4,         TRUE,        BYTE,      ,          TRUE,        4,       ON,4, OFF,0, ENABLE,4, DISABLE,0,                                                         ON
PRESCALER,     0,  INL,      ,         ,                    0x38,      TRUE,        BYTE,      ,          TRUE,        8,       DIV2,0, DIV4,8, DIV8,0x10, DIV16,0x18, DIV32,0x20, DIV64,0x28, DIV128,0x30, DIV256,0x38
FRAMEFMT,      0,  INL,      ,         ,                    0x80,      TRUE,        BYTE,      ,          TRUE,        2,       MSBFIRST,0, LSBFIRST,0x80
TRANSMODE,     1,  INL,      ,         ,                    3,         TRUE,        BYTE,      ,          TRUE,        3,       DUPLEX,0, HALFDUPLEX,1, SIMPLEX,2
CFGPINS,       1,  INL,      ,         ,                    4,         TRUE,        BYTE,      ,          TRUE,        4,       ON,4, OFF,0, ENABLE,4, DISABLE,0,                                                         ON
SSPIN,         2,  INL,      ,         ,                    ,          TRUE,        TEXT,      ,          FALSE,       1,       NONE,NONE
WAIT,          3,  INL,      ,         ,                    ,          TRUE,        BYTE,      ,          TRUE,        3,       RXNE,0, TXE,1, NOTBSY,2
START,         5,  CALL,     ,         ,                    ,          TRUE,        BYTE,      ,          TRUE,        2,       RX,0, TX,1,                                                                               TX
STOP,          6,  CALL,     ,         ,                    ,          FALSE
ENABLE,        7,  INL,      ,         ,                    ,          FALSE
DISABLE,       8,  INL,      ,         ,                    ,          FALSE

[ST7565_SPI]
; name         id  call_type placement file_name            mask       accept_data  data_type  extra_data predef_only  val_num  val0_name,val0_value...                                                                   def_val
RSTPIN,        0,  INL,      ,         ,                    ,          TRUE,        TEXT,      ,          FALSE,       1,       NONE,NONE
DCPIN,         1,  INL,      ,         ,                    ,          TRUE,        TEXT,      ,          FALSE
CFGPINS,       2,  INL,      ,         ,                    ,          TRUE,        BYTE,      ,          TRUE,        4,       ON,1, OFF,0, ENABLE,1, DISABLE,0,                                                         ON
INIT,          3,  INL,      ,         ,                    ,          TRUE,        VARREF,    ,          FALSE
FONT,          4,  CALL,     ,         ,                    ,          TRUE,        VARREF,    _ST7565,   FALSE
ZONEWIDTH,     5,  INL,      ,         ,                    ,          TRUE,        BYTE,      IMR,       FALSE
INVERT,        6,  INL,      ,         ,                    ,          TRUE,        BYTE,      ,          TRUE,        4,       ON,0xFF, OFF,0, ENABLE,0xFF, DISABLE,0,                                                   ON
START,         7,  CALL,     ,         ,                    ,          FALSE
STOP,          8,  INL,      ,         ,                    ,          FALSE
ENABLE,        9,  INL,      ,         ,                    ,          FALSE
DISABLE,       10, INL,      ,         ,                    ,          FALSE
RESUME,        11, INL,      ,         ,                    ,          FALSE
SCROLL,        13, CALL,     ,         ,                    ,          FALSE
CLR,           14, CALL,     ,         ,                    ,          FALSE
CONTRAST,      15, CALL,     ,         ,                    ,          TRUE,        BYTE,      ,          FALSE
COL,           16, INL,      ,         ,                    ,          TRUE,        BYTE,      IMR,       FALSE
ROW,           17, INL,      ,         ,                    ,          TRUE,        BYTE,      IMR,       FALSE
DRAWCHAR,      18, CALL,     ,         ,                    ,          TRUE,        BYTE,      ,          FALSE
