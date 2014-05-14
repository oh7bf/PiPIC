
; Test code written by Giorgio Vazzana modified to fit 12f675. Program 
; blinks led connected to GP0 (pin 7). The internal 4MHz oscillator is
; used.
; Jaakko Koivuniemi
;
; compile: gpasm -a inhx16 led.asm
; program 12f675: sudo ./rpp-tlc -w -i led.hex
;
; Configuration:
; - data code protection disabled
; - program memory code disabled
; - brown-out detect disabled
; - MCLR tied to VDD, pin used as I/O
; - power-up time enabled
; - watch-dog time disabled
; - internal oscillator with CLKOUT 
		processor	12F675
		radix		dec
		include		p12f675.inc
		errorlevel	-302
		__config	_CPD_OFF & _CP_OFF & _BODEN_OFF & _MCLRE_OFF &  _PWRTE_ON & _WDT_OFF & _INTRC_OSC_CLKOUT


; constants
led		equ	0

; variables in ram
; can use 20-5F, use STATUS bit 5 to select Bank 0 or 1
i		equ     H'20'
j		equ     H'21'

; reset vector
		org	H'00'
		goto	setup

; interrupt vector, general interrupt handling routine goes here
		org	H'04'
                goto    setup

; initial condition of registers
;
; OPTION_REG options, bank 1
; 7      6        5      4      3     2     1     0
; GPPU | INTEDG | T0CS | T0SE | PSA | PS2 | PS1 | PS0
; 1      1        1      1      1     1     1     1
; - GPIO pull ups disabled
; - interrupt rising edge GP2/INT pin
; - TMR0 clock source transition on GP2/T0CKI pin
; - TMR0 source edge increment on high-to-low transition on GP2/T0CKI pin
; - prescaler assigned to WDT
; - WDT rate 1:128 (TMR0 rate 1:256)
;
; INTCON interrupt control, bank 1
; 7     6      5      4      3      2      1      0
; GIE | PEIE | T0IE | INTE | GPIE | T0IF | INTF | GPIF
; 0     0      0      0      0      0      0      0
; - global interrupts disabled
; - peripheral interrupts disabled
; - TMR0 overflow interrupt disabled
; - GP2/INT external interrupt disabled
; - GPIO port change interrupt disabled
; - TMR0 overflow interrupt flag
; - GP2/INT external interrupt flag
; - GP5:GP0 port change interrupt flag
; 
; PIE1 peripheral interrupt enable, bank 1
; 7      6      5   4   3      2   1   0
; EEIE | ADIE | - | - | CMIE | - | - | TMR1IE
; 0      0      -   -   0      -   -   0
; - EE write complete interrupt disabled
; - ADIE A/D converter interrupt disabled
; - CMIE comparator interrupt disabled
; - TMR1IE TMR1 overflow interrupt disabled
; 
; PIR1 peripheral interrupts, bank 0
; 7      6      5   4   3      2   1   0
; EEIF | ADIF | - | - | CMIF | - | - | TMR1IF
; 0      0      -   -   0      -   -   0
; - EEPROM write operation interrupt flag bit
; - A/D converter interrupt flag bit
; - comparator interrupt flag bit
; - TMR1 overflow interrupt flag bit
;
; PCON power control, bank 1
; 7   6   5   4   3   2   1     0
; - | - | - | - | - | - | POR | BOD
; -   -   -   -   -   -   0     x
; - power-on reset occurred
; - brown-out detect occurred?
;
; OSCCAL oscillator calibration, bank 1
; 7      6      5      4      3      2      1   0
; CAL5 | CAL4 | CAL3 | CAL2 | CAL1 | CAL0 | - | -
; 1      0      0      0      0      0      -   -
; - center frequency
;
; PCL and PCLATH program counter, bank 0 or 1
; - both at zero
;
; FSR file select, bank 0 or 1 
; - unknown
;
; GPIO general purpose I/O, bank 0
; 7   6   5       4       3       2       1       0
; - | - | GPIO5 | GPIO4 | GPIO3 | GPIO2 | GPIO1 | GPIO0
; x   x   x       x       x       x       x       x
; 
; TRISIO GPIO tri-state, bank 1
; 7   6   5         4         3         2         1         0
; - | - | TRISIO5 | TRISIO4 | TRISIO3 | TRISIO2 | TRISIO1 | TRISIO0
; -   -   1         1         1         1         1         1
; - GP0-5 all inputs
;
; WPU weak pull-up, bank 1
; 7   6   5      4      3   2      1      0
; - | - | WPU5 | WPU4 | - | WPU2 | WPU1 | WPU0
; -   -   1      1      -   1      1      1
; - GP0-5 weak pull ups all enabled
;
; IOC interrupt-on-change, bank 1
; 7   6   5      4      3      2      1      0 
; - | - | IOC5 | IOC4 | IOC3 | IOC2 | IOC1 | IOC0
; -   -   0      0      0      0      0      0
; - GP0-5 interrupt on change all disabled
;
; T1CON timer1 control, bank 0
; 7   6        5         4         3         2        1        0
; - | TMR1GE | T1CKPS1 | T1CKPS0 | T1OSCEN | T1SYNC | TMR1CS | TMR1ON
; -   0        0         0         0         0        0        0
; - timer1 is on
; - timer1 input clock prescale 1:1
; - LP oscillator off
; - timer1 external clock input synchronization to external clock input
; - timer1 clock source internal clock (FOSC/4)
; - timer1 stopped
;
; CMCON comparator control, bank 0
; 7   6      5   4      3     2     1     0
; - | COUT | - | CINV | CIS | CM2 | CM1 | CM0
; -   0      -   0      0     0     0     0
; - comparator output bit
; - comparator output not inverted
; - comparator input switch VIN- connects to CIN
; - comparator output off
;
; VRCON voltage reference control, bank 1
; 7      6   5     4   3     2     1     0
; VREN | - | VRR | - | VR3 | VR2 | VR1 | VR0
; 0      -   0     -   0     0     0     0
; - CVREF powered down
; - CVREF high range
; - CVREF = 0
;
; ADCON0 A/D control, bank 0
; 7      6      5   4   3      2      1         0
; ADFM | VCFG | - | - | CHS1 | CHS0 | GO/DONE | ADON
; 0      0      -   -   0      0      0         0
; - A/D result left justified
; - voltage reference VDD
; - analog channel AN0
; - A/D conversion completed/not in progress
; - A/D converter shut-off
; 
; ANSEL analog select, bank 1
; 7   6       5       4       3      2      1      0
; - | ADCS2 | ADCS1 | ADCS0 | ANS3 | ANS2 | ANS1 | ANS0
; -   0       0       0       1      1      1      1
; - A/D conversion clock Fosc/2
; - AN0-3 analog inputs, need to be set digital
;
; EEDAT eeprom data, bank 1
; - all zeros
;
; EEADR eeprom address, bank 1
; - all zeros
;
; EECON1 eeprom control, bank 1
; 7   6   5   4   3       2      1    0
; - | - | - | - | WRERR | WREN | WR | RD
; -   -   -   -   x       0      0    0
; - eeprom write error flag?
; - eeprom write inhibited
; - write cycle to the data eeprom is complete
; - eeprom read is not initiated


setup		movlw	H'07'		;Turn comparators off and enable
		movwf	CMCON		;pins for I/O functions
		bsf	STATUS, RP0
                clrf    ANSEL           ;AN0-AN3 disabled, digital I/O	
		movlw	B'00111110'
		movwf	TRISIO
		bcf	STATUS, RP0

start		bcf	GPIO, led
		movlw	234		;180183us
		call	delay
		movlw	234		;180183us
		call	delay
		movlw	181		;139373us -> total = 499739us
		call	delay
		bsf	GPIO, led
		movlw	234
		call	delay
		movlw	234
		call	delay
		movlw	181
		call	delay
		goto	start

; -----------------delay-----------------------------------
; Software delay variable with w
; Data:      w
; Variables: i, j

delay		movwf	i		;1 us
		clrf	j		;1 us

dloop		decfsz	j, F		;(j-1) x 1 us + 2 us	(j==0 -> j=256)
		goto	dloop		;(j-1) x 2 us

		decfsz	i, F		;(i-1) x 1 us + 2 us
		goto	dloop		;(i-1) x 2 us

		return			;2 us

; cycles =     (3j-1)*i + (3i-1) + 4
; j=256 ->  (3*256-1)*i + (3i-1) + 4
; total  = 770*i + 3


		end
