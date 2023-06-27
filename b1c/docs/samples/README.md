# samples directory  
  
- `blink.bsc` - simple blinking LED sample  
- `blink1.bsc` - another blinking LED sample  
- `blink2.bsc` - blinking LED sample using timer, changing MCU clock source  
- `blink2_tm.bsc` - timer interrupt handler for `blink2.bsc` sample  
- `blink3.bsc` - another blinking LED sample using timer  
- `blink3_tm.bsc` - timer interrupt handler for `blink3.bsc` sample  
- `uart.bsc` - write a string to default UART (8 data bits, no parity check, 1 stop bit, baudrate: 9600)  
- `uart1.bsc` - read string from UART (8 data bits, no parity check, 1 stop bit, baudrate: 9600)  
- `uart2.bsc` - another read/write UART sample (8 data bits, no parity check, 1 stop bit, baudrate: 9600)  
  
# building the samples from command line  
`b1c -d -s -m STM8S103F3 samples/blink.bsc`  
`b1c -d -s -m STM8S103F3 samples/blink1.bsc`  
  
For interrupt handlers specify interrupt name before the source file name separated with colon:  
`b1c -d -s -m STM8S103F3 samples/blink2.bsc tim2_updovf:samples/blink2_tm.bsc`  
`b1c -d -s -m STM8S103F3 samples/blink3.bsc timer_updovf:samples/blink3_tm.bsc`  
  
`b1c -d -s -m STM8S103F3 samples/uart.bsc`  
`b1c -d -s -m STM8S103F3 samples/uart1.bsc`  
`b1c -d -s -m STM8S103F3 samples/uart2.bsc`  
  