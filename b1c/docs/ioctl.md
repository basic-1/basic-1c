# IOCTL statement description  
  
## Description    
  
`IOCTL` statement allows performing some low level operations and sending commands to MCU peripherals.  
  
**Syntax:**  
`IOCTL <device_name>, <command_name>[, <command_data>]`  
`<device_name>` is a name of MCU's peripheral device to send command to. A special device name `CPU` is used to manage MCU core settings and run some low-level commands.  
  
## Available devices and commands  
  
### CPU  
  
- `IOCTL CPU, INTERRUPTS, ON | OFF` - enable or disable interrupts, by default interrups are disabled.  
- `IOCTL CPU, CLOCKSOURCE, HSI | HSI16 | LSI | HSE16 | HSE8` - select MCU clock source generator, default is `HSI`. `HSI` stands for the maximum possible frequence available with internal RC oscillator, `HSI16` - 16 MHz with internal oscillator, `HSE8` - 8 MHz with 8 MHz external crystal oscillator, `HSE16` - 16 MHz with 16 MHz external crystal oscillator.  
- `IOCTL CPU, WAIT, INTERRUPT` - wait for interrupt command  
  
### GPIO  
  
- `IOCTL PX, CFGPINY, IN_FLOAT_NOEXTI | IN_FLOAT_EXTI | IN_PULLUP_NOEXTI | IN_PULLUP_EXTI | OUT_OPENDRAIN_SLOW | OUT_OPENDRAIN_FAST | OUT_PUSHPULL_SLOW | OUT_PUSHPULL_FAST` - configure single GPIO port pin.  
- `IOCTL PX, SETPINY` - set single pin (set to high logic level)  
- `IOCTL PX, CLRPINY` - clear single pin (set to low logic level)  
- `IOCTL PX, INVPINY` - invert pin state  
- `IOCTL PX, ENABLE` - enable GPIO port  
- `IOCTL PX, DISABLE` - disable GPIO port  
  
Here `PX` stands for GPIO port name (e.g. `PA` for port A) and command names should specify the port pin (pin number should be the last character of the command, instead of Y character in the patterns above).  
  
**Example:**  
`REM configure pin 5 of port B as output push-pull and write 0 to the pin`  
`IOCTL PB, ENABLE`  
`IOCTL PB, CFGPIN5, OUT_PUSHPULL_FAST`  
`IOCTL PB, CLRPIN5`  
  
### UART  
  
- `IOCTL UART, INPUTECHO, ON | OFF` - enable or disable input echo (enabled by default)  
- `IOCTL UART, RX, ON | OFF` - enable or disable RX (enabled by default)  
- `IOCTL UART, TX, ON | OFF` - enable or disable TX (enabled by default)  
- `IOCTL UART, SPEED, 9600 | 14400 | 19200 | 38400 | 57600 | 115200` - set UART baud rate (default is 9600)  
- `IOCTL UART, PINS, DEFAULT` - initialize UART pins  
- `IOCTL UART, NEWLINE, <string_value>` - set new-line sequence (default is CRLF)  
- `IOCTL UART, MARGIN, <numeric_value>` - set margin (print area width, default is 80 characters)  
- `IOCTL UART, ZONEWIDTH, <numeric_value>` - set print zone width (default is 10 characters)  
- `IOCTL UART, START` - start communication  
- `IOCTL UART, STOP` - stop communication  
- `IOCTL UART, ENABLE` - enable UART  
- `IOCTL UART, DISABLE` - disable UART  
  
**Example:**  
`REM the simplest UART configuration, 8N1 9600 baud`  
`IOCTL UART, PINS, DEFAULT`  
`IOCTL UART, ENABLE`  
`IOCTL UART, START`  
  
### TIMER  
  
- `IOCTL TIMER, PRESCALER, DIV1 | DIV2 | DIV4 | DIV8 | DIV16 | DIV32 | DIV64 | DIV128 | DIV256 | DIV512 | DIV1024 | DIV2048 | DIV4096 | DIV8192 | DIV16384 | DIV32768` - set timer clock prescaler (frequency divisor)  
- `IOCTL TIMER, VALUE, <numeric_value>` - set timer counter  
- `IOCTL TIMER, PERIODMS, <numeric_value>` - set timer period in ms (tries setting prescaler and value automatically)  
- `IOCTL TIMER, INTERRUPT, ON | OFF` - enable or disable timer update interrupt (disabled by default)  
- `IOCTL TIMER, CLRINTFLAG` - clear update interrupt flag  
- `IOCTL TIMER, ONUPDOVF, <line_number>` - use BASIC subroutine starting from the specifide line number as a handler for timer's update/overflow interrupt, the handler automatically clears update interrupt flag (see previous command description)  
- `IOCTL TIMER, START` - start timer  
- `IOCTL TIMER, STOP` - stop timer  
- `IOCTL TIMER, ENABLE` - enable timer  
- `IOCTL TIMER, DISABLE` - disable timer  
  
**Example:**  
`IOCTL CPU, INTERRUPTS, ON`  
`IOCTL TIMER, ENABLE`  
`IOCTL TIMER, PERIODMS, 100`  
`IOCTL TIMER, ONUPDOVF, 1000`  
`IOCTL TIMER, INTERRUPT, ENABLE`  
`IOCTL TIMER, START`  
`...`  
`END`  
  
`1000 REM timer's update interrupt handler`  
`...`  
`RET`  