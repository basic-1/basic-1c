# samples directory  
  
- `blink.bsc` - simple blinking LED example  
- `blink1.bsc` - another blinking LED example  
- `blink2.bsc` - blinking LED example using timer, changing MCU clock source  
- `blink2_tm.bsc` - timer interrupt handler for `blink2.bsc` example  
- `blink3.bsc` - another blinking LED example using timer  
- `blink3_tm.bsc` - timer interrupt handler for `blink3.bsc` example  
- `blink4.bsc` - blinking LED with timer using BASIC subroutine as interrupt handler  
- `uart.bsc` - write a string to UART (8 data bits, no parity check, 1 stop bit, baudrate: 9600)  
- `uart1.bsc` - read string from UART (8 data bits, no parity check, 1 stop bit, baudrate: 9600)  
- `uart2.bsc` - another read/write UART example (8 data bits, no parity check, 1 stop bit, baudrate: 9600)  
- `uart3.bsc` - set 57600 baud speed (8 data bits, no parity check, 1 stop bit, baudrate: 57600)  
- `data.bsc` - `DATA` statement usage  
- `strings.bsc` - string functions example (`INSTR` function)  
- `heap.bsc` - prints heap memory structure (8 data bits, no parity check, 1 stop bit, baudrate: 9600)  
- `spimss.bsc` - read a string from UART (8N1 9600 baud) and send it over SPI (simplex TX-only master). Works in pair with `spisls.bsc`  
- `spisls.bsc` - read a string from SPI (simplex RX-only slave). Works in pair with `spimss.bsc`  
- `spimsd.bsc` - SPI master in duplex mode example. Works in pair with `spisld.bsc`  
- `spisld.bsc` - SPI slave in duplex mode example. Works in pair with `spimsd.bsc`  
- `const.bsc` - constant data usage example (declared with `DIM CONST`)  
- `st7565.bsc` - "hello world" example for ST7565-based LCD display  
- `st7565m105.bsc` - simple menu example for ST7565-based LCD display and STM8S105K4T6 MCU  
- `st7565mdata.bsc` - textual data for `st7565m105.bs` example  
  
# building the samples from command line  
`b1c -d -s -m STM8S103F3 samples/blink.bsc`  
`b1c -d -s -m STM8S103F3 samples/blink1.bsc`  
  
For interrupt handlers specify interrupt name before the source file name separated with colon:  
`b1c -d -s -m STM8S103F3 samples/blink2.bsc tim2_updovf:samples/blink2_tm.bsc`  
`b1c -d -s -m STM8S103F3 samples/blink3.bsc timer_updovf:samples/blink3_tm.bsc`  
  
`b1c -d -s -m STM8S103F3 samples/blink4.bsc`  
`b1c -d -s -m STM8S103F3 samples/uart.bsc`  
`b1c -d -s -m STM8S103F3 samples/uart1.bsc`  
`b1c -d -s -m STM8S103F3 samples/uart2.bsc`  
`b1c -d -s -m STM8S103F3 samples/uart3.bsc`  
`b1c -d -s -m STM8S103F3 samples/data.bsc`  
`b1c -d -s -m STM8S103F3 samples/strings.bsc`  
`b1c -d -s -m STM8S103F3 samples/heap.bsc`  
`b1c -d -s -m STM8S105K4 samples/spimss.bsc`  
`b1c -d -s -m STM8S103F3 samples/spisls.bsc`  
`b1c -d -s -m STM8S105K4 samples/spimsd.bsc`  
`b1c -d -s -m STM8S103F3 samples/spisld.bsc`  
`b1c -d -s -m STM8S105K4 samples/const.bsc`  
`b1c -d -s -m STM8S105K4 samples/st7565.bsc`  
`b1c -d -s -m STM8S105K4 samples/st7565m105.bsc samples/st7565mdata.bsc`  
  