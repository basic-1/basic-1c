[CPU]
; name         id  call_type placement file_name            mask             accept_data  data_type  extra_data  predef_only  val_num  val0_name,val0_value...
INTERRUPTS,    0,  INL,      ,         ,                    ,                TRUE,        BYTE,      ,           TRUE,        4,       ON,1,         OFF,0,         ENABLE,1,      DISABLE,0
CLOCKSOURCE,   1,  CALL,     ,         ,                    ,                TRUE,        LONG,      ,           TRUE,        7,       HSI,0x20100, HSI24,0x20100, HSI48,0x30700, HSI8,0x120, HSI16,0x10320, HSE24,0x20005, HSE48,0x30605
WAIT,          2,  INL,      ,         ,                    ,                TRUE,        BYTE,      ,           TRUE,        1,       INTERRUPT,0
DELAYMS,       3,  CALL,     ,         ,                    ,                TRUE,        LONG,      ,           FALSE
