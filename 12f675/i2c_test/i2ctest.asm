
; Attempt to do slow software i2c on 12f675. This works but needs external
; transistor to pull down the SDA line. The internal 4MHz oscillator is used.
;
; Sun Jul 21 19:36:52 CEST 2013
; Jaakko Koivuniemi
;
; compile: gpasm -a inhx16 i2ctest.asm
; program 12f675: sudo ./rpp-tlc -w -i i2ctest.hex
; hardware: 
; GPO--1kohm--LED--GND
; GP1--100kohm--2N3904 base
; GP2/INT--SDA--2N3904 collector
; GP3--SCL
; GP4--received "0" or "1"
; GP5--1kohm--LED--GND
; GND--2N3904 emmitter
;
; Configuration:
; - data code protection disabled
; - program memory code disabled
; - brown-out detect disabled
; - MCLR tied to VDD, GP3 pin used as digital input 
; - power-up time enabled
; - watch-dog timer enabled
; - internal oscillator, I/O on GP4/OSC2/CLKOUT and GP5/OSC1/CLKIN 
		processor	12F675
		radix		dec
		include		p12f675.inc
		errorlevel	-302
		__config	_CPD_OFF & _CP_OFF & _BODEN_OFF & _MCLRE_OFF &  _PWRTE_ON & _WDT_ON & _INTRC_OSC_NOCLKOUT

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
; INTCON interrupt control, bank 0 or 1
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


; constants
LED             equ     0    ; green=address matched
LED2            equ     5    ; red=WDT occured
SDA             equ     2    ; slave SDA=GPIO2, check also TRISIO
SDACK           equ     1    ; GPIO1 1=SDA acknowledge
SCL             equ     3    ; slave SCL=GPIO3, check also IOC

; variables in ram
; can use 20-5F, use STATUS bit 5 to select Bank 0 or 1
; 
; i2cstate
; 7   6   5   4   3        2       1       0
; - | - | - | - | I2RCVD | I2BN2 | I2BN1 | I2BN0 
; 0   0   0   0   0        0       0       0
; - I2BN0:2 number of bits received
; - I2RCVD=1 8 bits have been received

I2BN0           equ     H'0000'
I2BN1           equ     H'0001'
I2BN2           equ     H'0002'
I2RCVD          equ     H'0003'

i2cstate        equ     H'20'    ; state of bit transfer
i2cdata         equ     H'21'    ; received data byte

; three byte buffer for received data
i2crec1         equ     H'5D'    ; first received byte    
i2crec2         equ     H'5E'    ; second received byte    
i2crec3         equ     H'5F'    ; third received byte   
 
i               equ     H'30'
w_temp          equ     H'33'    ; temperorary W storage in interrupt service
w_temp2         equ     H'B3'    ; reserve also W storage from bank 1
status_temp     equ     H'34'    ; temperorary storage in interrupt service

; reset vector
		org	H'00'
		goto	setup

; interrupt vector, general interrupt handling routine goes here
		org	H'04'

; takes 3 - 4 us after falling edge on SDA or GP2/INT before the program
; counter is here
;
; 3-4 us  INT latency
; 4   us  save W and STATUS
; 7   us  check that INT happened
; -------------------------------
; 14-15  us  total before starting to follow SCL
;
; 2-4 us  check that SCL LOW
; 2-4 us  to detect SCL LOW-HIGH edge
; 4   us  read SDA bit to i2cdata
; 2-4 us  to detect SLC HIGH-LOW edge 
; 4   us  check that less than 8 bits received
; -------------------------------------
; 14-20 us for one bit reading
; 
; 6   us  check that address matches and pull SDA down
; 2-4 us  to detect SCL LOW-HIGH edge 
; 2-4 us  to detect SCL HIGH-LOW edge
; 2   us  let SDA float high
; --------------------------------------
; 12-16 us acknowledgement bit
; 
; 6   us  return from sdaint service
; 7   us  recover W and STATUS
; ------------------------------------
; 13  us  ready for next start bit
;

; Note that bcf and bsf commands on GPIO will change output data on input pins 
; to reflect input at the time of execution. This will then be the output
; when pin is changed from input to output. 

; save W and STATUS
                movwf   w_temp          ; could be bank 0 or 1     1 us
                swapf   STATUS, W       ;                          1 us
                bcf     STATUS, RP0     ; bank 0                   1 us
                movwf   status_temp     ;                          1 us

; I2C SDA or GP2/INT down edge, takes 2+2+1+2= 7 us to reach 'sdaint'
                btfss   GPIO, SCL       ; check that SCL=1         1-2 us
                goto    endint          ;                          2 us
                btfss   INTCON, INTE    ;                          1-2 us
                goto    endint          ;                          2 us
                btfsc   INTCON, INTF    ;                          1-2 us
                call    sdaint          ;                          2 us 

; recover W and STATUS
endint          bcf     STATUS, RP0     ; bank 0                   1 us
                swapf   status_temp, W  ;                          1 us
                movwf   STATUS          ; bank to original state   1 us
                swapf   w_temp, F       ;                          1 us
                swapf   w_temp, W       ;                          1 us

                retfie                  ;                          2 us

; I2C SDA changed from 1 to 0 if INTEDG=0, could be start bit 1->0 
sdaint          btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    sdaint             ;                     2 us
sclow           btfss   GPIO, SCL          ; wait until SCL=1    1-2 us
                goto    sclow              ;                     2 us
                movlw   B'11111011'        ;                     1 us
                iorwf   GPIO, W            ;                     1 us
                addlw   B'00000001'        ; carry C=SDA now     1 us
                rlf     i2cdata, F         ; carry to i2cdata    1 us

                call    debug4             ; show bit on GPIO4 output

sclhigh         btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    sclhigh            ;                     2 us

                incf    i2cstate, F        ; i2cstate++          1 us
                btfss   i2cstate, I2RCVD   ; 8 bits received?    1-2 us
                goto    sclow              ;                     2 us

; 8 bits received, check if address matches
                movlw   B'11111110'        ;                     1 us
                andwf   i2cdata, W         ;                     1 us
; address 38 or 0x26 shifted one bit left  
                sublw   H'4C'              ;                     1 us
                btfss   STATUS, Z          ;                     1-2 us
                goto    noack              ;                     2 us

                call    posack             ; positive acknowledgement

                bsf     GPIO, LED          ; led on              1 us
                btfss   i2cdata, 0         ; if R/_W=0 go to receive byte
                goto    breceive           ;

; transmit byte here

                goto    reinit             ;                     2 us

noack           call    negack             ; negative acknowledgement

; wait for stop bit here and read first data bit
breceive        clrf    i2cstate           ; clear i2cstate=0    1 us
                movlw   i2crec1            ; intialize FSR
                movwf   FSR                ;  
atlow           btfss   GPIO, SCL          ; wait until SCL=1    1-2 us
                goto    atlow
                movlw   B'11111011'        ;                     1 us
                iorwf   GPIO, W            ;                     1 us
                addlw   B'00000001'        ; carry C=SDA now     1 us
                rlf     i2cdata, F         ; carry to i2cdata    1 us
                incf    i2cstate, F        ; i2cstate++          1 us

                call    debug4             ; show bit on GPIO4 output

; if SDA=1 start waiting SCL=0
                btfsc   i2cdata, 0
                goto    athigh 
; otherwise wait for SDA=0->1 or SCL=0
wstopb          btfsc   GPIO, SDA
                goto    stopb              ; stop bit received probably
                btfss   GPIO, SCL
                goto    sclow2
                goto    wstopb

athigh          btfsc   GPIO, SCL          ; wait until SCL=0
                goto    athigh

; no stop bit received, continue reading bits(subroutine?)
sclow2          btfss   GPIO, SCL          ; wait until SCL=1    1-2 us
                goto    sclow2             ;                     2 us
                movlw   B'11111011'        ;                     1 us
                iorwf   GPIO, W            ;                     1 us
                addlw   B'00000001'        ; carry C=SDA now     1 us
                rlf     i2cdata, F         ; carry to i2cdata    1 us

                call    debug4

sclhigh2        btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    sclhigh2           ;                     2 us

                incf    i2cstate, F        ; i2cstate++          1 us
                btfss   i2cstate, I2RCVD   ; 8 bits received?    1-2 us
                goto    sclow2             ;                     2 us

                call    posack             ; positive acknowledgement

                movf    i2cdata, W         ; move received byte to buffer
                movwf   INDF
                incf    FSR, F             ; pointer to next
                
                goto    breceive

stopb           btfss   GPIO, SCL          ; check that SCL is still 1
                goto    sclow2             ; otherwise return to reading bits 
reinit          clrf    i2cstate           ; clear i2cstate=0    1 us 
                bcf     INTCON, INTF       ;                     1 us
                return                     ;                     2 us

; positive acknowledgement by pulling SDA down
posack          bsf     GPIO, SDACK        ; set SDACK=1         1 us 
asclow          btfss   GPIO, SCL          ; wait until SCL=1    1-2 us
                goto    asclow             ;                     2 us
asclhigh        btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    asclhigh           ;                     2 us
                bcf     GPIO, SDACK        ; set SDACK=0         1 us 
                return

; negative acknowledgement: follow acknowledgement cycle on SCL, do not act 
; on SDA
negack          btfss   GPIO, SCL          ; wait until SCL=1    1-2 us
                goto    negack             ;                     2 us
noackhigh       btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    noackhigh          ;                     2 us
                bcf     GPIO, LED          ; led off             1 us
                return

; for debugging reflect received bit on GPIO4
debug4          bsf     GPIO, GPIO4        ; received bit to GPIO4 1 us
                btfss   i2cdata, 0         ;                     1-2 us
                bcf     GPIO, GPIO4        ;                     1 us
                return



; turn comparators off and enable pins for I/O functions
setup		bcf     STATUS, RP0     ; bank 0
                movlw	H'07'		
		movwf	CMCON		

; initial data on GP0=0, GP1=0, GP4=0, GP5=0, GP2=0, GP3=1
		movlw   B'00001000'	
		movwf   GPIO	

; LED2 on if WDT reset
                btfss   STATUS, NOT_TO
                bsf     GPIO, LED2         

; AN0-AN3 disabled, used for digital I/O	
		bsf	STATUS, RP0     ; bank 1
                clrf    ANSEL           

; GP0, GP1, GP4, GP5 digital output, GP2, GP3 inputs 
		movlw   B'00001100'	
		movwf	TRISIO

; weak pull up on GP1
;                bsf     WPU, MSDA
;                bcf     OPTION_REG, NOT_GPPU

; clear i2cstate bits and received data buffer i2rec1:3
                bcf     STATUS, RP0  ; bank 0
                clrf    i2cstate
                clrf    i2crec1
                clrf    i2crec2
                clrf    i2crec3

; if no prescaler for WDT typical reset after 128x18 ms (10-30 ms)=2.3 s 
                clrwdt
                clrf    TMR0
                bsf     STATUS, RP0         ; bank 1

; this is needed only when PS2:PS0 is 000 or 001, otherwise comment out
                movlw   B'00101111'         ; prescaler used for WDT
                movwf   OPTION_REG 
                clrwdt
; comment out until here

                movlw   B'00101100'         ; try 16x18 ms (10-30 ms)=288 ms
                movwf   OPTION_REG

; enable SDA or GP2/INT interrupt on falling edge 
                bsf     STATUS, RP0         ; bank 1
                bcf     OPTION_REG, INTEDG  ; 1->0 edge
                bcf     INTCON, INTF        ; clear INT flag
                bsf     INTCON, INTE        ; enable INT

; global interrupts enable
                bsf     INTCON, GIE
 
loop            nop                         ; wait for start bit INT 
                clrwdt                      
                goto    loop

                end
