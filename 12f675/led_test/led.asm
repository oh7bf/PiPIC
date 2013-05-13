
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
i		equ     H'20'
j		equ     H'21'

; reset vector
		org	H'00'
		goto	setup

; interrupt vector
		org	H'04'
                goto    setup

; initial condition of registers
; OPTION_REG
; - GPIO pull ups disabled
; - interrupt rising edge GP2/INT pin
; - timer/prescaler
; INTCON
; - disable global, PE, TO, INT interrupts
; PIE1
; - disable EE, AD, CM, TMR interrupts  
; PIR1
; - all cleared
; PCON
; - check and set POR and BOD
; OSCCAL
; - center frequency
; PCL and PCLATH
; - both at zero
; FSR 
; - unknown
; GPIO
; - unknown
; TRISIO
; - GP0-5 all inputs
; WPU
; - GP0-5 weak pull ups all enabled
; IOC
; - GP0-5 interrupt on change all disabled
; T1CON
; - LP oscillator off, internal clock, timer1 stopped,  
; CMCON
; - comparator output off
; VRCON
; - CVREF powered down
; ADCON0
; - A/D converter shut-off
; ANSEL
; - AN0-3 analog inputs, need to be set digital
; EEDAT
; - all zeros
; EEADR
; - all zeros
; EECON1
; - inhibit wiriting EEPROM 


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
