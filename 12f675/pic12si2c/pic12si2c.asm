;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Small helper PIC 12f675 I/O processor for Raspberry Pi. The slave i2c is 
; done in software. This limits the clock frequency or baudrate to 10 - 20 kHz
; when using the internal 4MHz oscillator. The SCL is connected to GP3 and SDA 
; to GP2/INT. The GP0, GP1, GP4 and GP5 are free to be used for I/O.
; Falling edge on SDA generates interrupt and the interrupt service
; routine takes care of the serial bit transfer. Command bytes can be send
; to the PIC with parameter data. The result data can be read from the PIC.
; There is a separate command line utility pipic(1) for this. The i2c chip
; address can be modified from line "sublw   H'4C'" below. 
;
; Copyright (C) 2013 Jaakko Koivuniemi.
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program. If not, see <http://www.gnu.org/licenses/>.
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Wed Jul 31 22:32:01 CEST 2013
; Jaakko Koivuniemi
;
; compile: gpasm -a inhx16 pic12si2c.asm
; program 12f675: sudo ./rpp-tlc -w -i pic12si2c.hex
; hardware: 
; GPO      AN0 analog in 
; GP1      AN1 analog in
; GP2/INT--SDA
; GP3------SCL
; GP4      I/O
; GP5      I/O
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
SDA             equ     2    ; slave SDA=GPIO2
SCL             equ     3    ; slave SCL=GPIO3

;LED             equ     0    ; green=address matched
;LED2            equ     5    ; red=WDT occured

; variables in ram
; can use 20-5F, use STATUS bit 5 to select Bank 0 or 1
; 
; i2cstate
; 7        6        5   4   3        2       1       0
; I2ADDR | I2DATA | - | - | I2RCVD | I2BN2 | I2BN1 | I2BN0 
; 0        0        0   0   0        0       0       0
; - I2ADDR=1 chip address matched
; - I2DATA=1 new command/data available in receiver buffer
; - I2BN0:2 number of bits received/transmitted
; - I2RCVD=1 8 bits have been received/transmitted

I2BN0           equ     0
I2BN1           equ     1
I2BN2           equ     2
I2RCVD          equ     3
I2DATA          equ     6
I2ADDR          equ     7

; task1
; 7         6      5       4       3       2       1       0
; TACTIVE | TRPT | TCNT5 | TCNT4 | TCNT3 | TCNT2 | TCNT1 | TCNT0
; 0         0      0       0       0       0       0       0
; - TACTIVE=1 task is active
; - TRPT=1 repeate task, otherwise do it only once 
; - TCNT0:5 number of times to repeate
;

i2cstate        equ     H'20'    ; state of bit transfer
i2cdata         equ     H'21'    ; received data byte
i2ctxdata       equ     H'22'    ; data byte to transmit

; 32-bit time counter is increased every 0.524288 seconds 
time1           equ     H'23'
time2           equ     H'24'
time3           equ     H'25'
time4           equ     H'26'

; timed task in future;
task1           equ     H'27'   ; task1 state
task1tm1        equ     H'28'   ; task1 initial count down values
task1tm2        equ     H'29'
task1tm3        equ     H'2A'
task1cmd1       equ     H'2B'   ; task1 command byte
task1cmd2       equ     H'2C'   ; optional task1 command parameter
task1cnt1       equ     H'2D'   ; task1 down counter
task1cnt2       equ     H'2E'   ; task1 down counter
task1cnt3       equ     H'2F'   ; task1 down counter

; temporary files
w_temp          equ     H'55'    ; temperorary W storage in interrupt service
w_temp2         equ     H'D5'    ; reserve also W storage from bank 1
status_temp     equ     H'56'    ; temperorary storage in interrupt service

; four byte buffer for data to transmit
i2ctx1          equ     H'57'
i2ctx2          equ     H'58'
i2ctx3          equ     H'59'
i2ctx4          equ     H'5A'

; five byte buffer for received data
i2crec1         equ     H'5B'    ; first received byte    
i2crec2         equ     H'5C'    ; second received byte    
i2crec3         equ     H'5D'    ; third received byte   
i2crec4         equ     H'5E'    ; fourth received byte    
i2crec5         equ     H'5F'    ; fifth received byte   


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
sdaint          clrf    i2cstate           ; clear i2cstate=0    1 us  
sclwait         btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    sclwait            ;                     2 us
sclow           btfss   GPIO, SCL          ; wait until SCL=1    1-2 us
                goto    sclow              ;                     2 us
                movlw   B'11111011'        ;                     1 us
                iorwf   GPIO, W            ;                     1 us
                addlw   B'00000001'        ; carry C=SDA now     1 us
                rlf     i2cdata, F         ; carry to i2cdata    1 us

;                call    debug4             ; show bit on GPIO4 output

sclhigh         btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    sclhigh            ;                     2 us

                incf    i2cstate, F        ; i2cstate++          1 us
                btfss   i2cstate, I2RCVD   ; 8 bits received?    1-2 us
                goto    sclow              ;                     2 us

; 8 bits received, check if address matches
                bsf     i2cstate, I2ADDR   ; set I2ADDR=1        1 us
                movlw   B'11111110'        ;                     1 us
                andwf   i2cdata, W         ;                     1 us
; address 38 or 0x26 shifted one bit left  
                sublw   H'4C'              ;                     1 us
                btfss   STATUS, Z          ;                     1-2 us
                goto    noack              ;                     2 us

                call    posack             ; positive acknowledgement 15-17 us

;                bsf     GPIO, LED          ; led on              1 us
                btfss   i2cdata, 0         ; if R/_W=0 go to receive byte 1 us
                goto    breceive           ;                      2 us

; transmit bytes from tx buffer here
                movlw   i2ctx1             ; intialize FSR
                movwf   FSR                ; 
 
txloop          movf    INDF, W            ; copy byte from tx buffer
                movwf   i2ctxdata          ;
                clrf    i2cstate           ; clear i2cstate=0    1 us
tathigh         btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    tathigh            ;                     2 us

                rlf     i2ctxdata, F       ; MSB to carry        1 us
                btfsc   STATUS, C          ; if carry=0
                goto    setsda
                bcf     GPIO, SDA          ; pull down SDA
                bsf     STATUS, RP0        ; bank 1
                bcf     TRISIO, SDA        ; 
                bcf     STATUS, RP0        ; bank 0
                goto    tatlow
setsda          bsf     STATUS, RP0        ; bank 1
                bsf     TRISIO, SDA        ; else let SDA float high 
                bcf     STATUS, RP0        ; bank 0

tatlow          btfss   GPIO, SCL          ; wait until SCL=1    1-2 us
                goto    tatlow

                incf    i2cstate, F        ; i2cstate++          1 us
                btfss   i2cstate, I2RCVD   ; 8 bits transmitted? 1-2 us
                goto    tathigh            ;                     2 us

                incf    FSR, F             ; pointer to next byte

tathigh2        btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    tathigh2           ;                     2 us

                bsf     STATUS, RP0        ; bank 1
                bsf     TRISIO, SDA        ; let SDA float high
                bcf     STATUS, RP0        ; bank 0

tatlow2         btfss   GPIO, SCL          ; wait until SCL=1    1-2 us
                goto    tatlow2

                btfss   GPIO, SDA          ; if postive acknowledgement
                goto    txloop             ; from master continue sending

tathigh3        btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    tathigh3           ;                     2 us

tatlow3         btfss   GPIO, SCL          ; wait until SCL=1    1-2 us
                goto    tatlow3

                goto    reinit             ;                     2 us

noack           call    negack             ; negative acknowledgement
                bcf     i2cstate, I2ADDR   ; clear I2ADDR=0

; wait for stop bit here and read first data bit
breceive        movlw   i2crec1            ; intialize FSR 
                movwf   FSR                ; 
 
rxloop          movlw   B'11000000'
                andwf   i2cstate, F        ; clear i2cstate bits except I2ADDR 

atlow           btfss   GPIO, SCL          ; wait until SCL=1    1-2 us
                goto    atlow
                movlw   B'11111011'        ;                     1 us
                iorwf   GPIO, W            ;                     1 us
                addlw   B'00000001'        ; carry C=SDA now     1 us
                rlf     i2cdata, F         ; carry to i2cdata    1 us
                incf    i2cstate, F        ; i2cstate++          1 us

;                call    debug4             ; show bit on GPIO4 output

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

;                call    debug4

sclhigh2        btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    sclhigh2           ;                     2 us

                incf    i2cstate, F        ; i2cstate++          1 us
                btfss   i2cstate, I2RCVD   ; 8 bits received?    1-2 us
                goto    sclow2             ;                     2 us

                call    posack             ; positive acknowledgement

                movf    i2cdata, W         ; move received byte to buffer
                btfsc   i2cstate, I2ADDR   ; save data to buffer if chip
                movwf   INDF               ; address matched
                incf    FSR, F             ; pointer to next
                bsf     i2cstate, I2DATA   ; I2DATA=1
                
                goto    rxloop 

stopb           btfss   GPIO, SCL          ; check that SCL is still 1
                goto    sclow2             ; otherwise return to reading bits 

reinit          bcf     INTCON, INTF       ;                     1 us
                return                     ;                     2 us

; positive acknowledgement by pulling SDA down 15 - 17 us
posack          bcf     GPIO, SDA          ;                     1 us 
                bsf     STATUS, RP0        ; bank 1              1 us
                bcf     TRISIO, SDA        ;                     1 us
                bcf     STATUS, RP0        ; bank 0              1 us
asclow          btfss   GPIO, SCL          ; wait until SCL=1    1-2 us
                goto    asclow             ;                     2 us
asclhigh        btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    asclhigh           ;                     2 us
                bsf     STATUS, RP0        ; bank 1              1 us
                bsf     TRISIO, SDA        ;                     1 us
                bcf     STATUS, RP0        ; bank 0              1 us
                return                     ;                     2 us

; negative acknowledgement: follow acknowledgement cycle on SCL, do not act 
; on SDA
negack          btfss   GPIO, SCL          ; wait until SCL=1    1-2 us
                goto    negack             ;                     2 us
noackhigh       btfsc   GPIO, SCL          ; wait until SCL=0    1-2 us
                goto    noackhigh          ;                     2 us
;                bcf     GPIO, LED          ; led off             1 us
                return

; for debugging reflect received bit on GPIO4
;debug4          bsf     GPIO, GPIO4        ; received bit to GPIO4 1 us
;                btfss   i2cdata, 0         ;                     1-2 us
;                bcf     GPIO, GPIO4        ;                     1 us
;                return



; turn comparators off and enable pins for I/O functions
setup		bcf     STATUS, RP0     ; bank 0
                movlw	H'07'		
		movwf	CMCON		

; initial data on GP0=0, GP1=0, GP4=0, GP5=0, GP2=0, GP3=1
		movlw   B'00001000'	
		movwf   GPIO	

; LED2 on if WDT reset
;                btfss   STATUS, NOT_TO
;                bsf     GPIO, LED2         

; right justified, ref VDD, A/D on
                movlw   B'10000001'
                movwf   ADCON0

; AN0-AN1 enabled, AN2-AN3 disabled and used for digital I/O, FOSC/8
		bsf	STATUS, RP0     ; bank 1
                movlw   B'00010011'
                movwf   ANSEL

; GP4, GP5 digital output, GP0, GP1 analog inputs, GP2, GP3 digital inputs 
		movlw   B'00001111'	
		movwf	TRISIO

; clear TMR1H:TTMR1L register pair 
                bcf     STATUS, RP0  ; bank 0
                clrf    TMR1H
                clrf    TMR1L 
; clear TMR1IF flag 
                bcf    PIR1, TMR1IF 
; timer1 on, prescaler 1:8, internal clock 1MHz, time1 enabled
                movlw   B'00110001'
                movwf   T1CON

; weak pull up on GP1
;                bsf     WPU, MSDA
;                bcf     OPTION_REG, NOT_GPPU

; clear i2cstate bits and received data buffer i2rec1:3
                clrf    i2cstate
                clrf    i2crec1
                clrf    i2crec2
                clrf    i2crec3

; test data to i2ctx1
                movlw   H'C5'
                movwf   i2ctx1 
                movlw   H'5C'
                movwf   i2ctx2 

; if no prescaler for WDT typical reset after 128x18 ms (10-30 ms)=2.3 s 
                clrwdt
                clrf    TMR0
                bsf     STATUS, RP0         ; bank 1

; this is needed only when PS2:PS0 is 000 or 001, otherwise comment out
;                movlw   B'00101111'         ; prescaler used for WDT
;                movwf   OPTION_REG 
;                clrwdt
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

; main command loop 
loop            clrwdt
                btfsc   i2cstate, I2DATA
                goto    i2cmd
                btfsc   PIR1, TMR1IF 
                goto    nxtime
                goto    loop

i2cmd           bcf     i2cstate, I2DATA

; command 0x01 copy data memory byte at given address to transmit buffer
                movf    i2crec1, W
                sublw   H'01'              
                btfss   STATUS, Z
                goto    cmd2
                movf    i2crec2, W
                movwf   FSR
                movf    INDF, W 
                movwf   i2ctx1
                goto    loop

; command 0x02 write received four bytes to transmit buffer
cmd2            movf    i2crec1, W
                sublw   H'02'              
                btfss   STATUS, Z
                goto    cmd3
                movf    i2crec2, W
                movwf   i2ctx1
                movf    i2crec3, W
                movwf   i2ctx2
                movf    i2crec4, W
                movwf   i2ctx3
                movf    i2crec5, W
                movwf   i2ctx4
                goto    loop

; command 0x03 read byte from EEPROM
cmd3            movf    i2crec1, W
                sublw   H'03'              
                btfss   STATUS, Z
                goto    cmd4
                movf    i2crec2, W
                bsf     STATUS, RP0         ; bank 1
                movwf   EEADR
                bsf     EECON1, RD          ; read EEPROM byte  
                movf    EEDATA, W
                bcf     STATUS, RP0         ; bank 0
                movwf   i2ctx1              ; copy byte to tx buffer
                goto    loop

; command 0x04 write byte to EEPROM
cmd4            movf    i2crec1, W
                sublw   H'04'              
                btfss   STATUS, Z
                goto    cmd5
                movf    i2crec2, W
                bsf     STATUS, RP0         ; bank 1
                movwf   EEADR
                bcf     STATUS, RP0         ; bank 0
                movf    i2crec3, W
                bsf     STATUS, RP0         ; bank 1
                movwf   EEDATA
                bsf     EECON1, WREN        ; write enable
                bcf     INTCON, GIE         ; disable interrupts
                movlw   H'55'               ; unlock write
                movwf   EECON2
                movlw   H'AA'
                movwf   EECON2
                bsf     EECON1, WR          ; start write
                bsf     INTCON, GIE         ; enable interrupts
                bcf     EECON1, WREN        ; write disabled
                bcf     STATUS, RP0         ; bank 0
                goto    loop

; command 0x10 clear GPIO0=0 output 
cmd5            movf    i2crec1, W
                sublw   H'10'
                btfss   STATUS, Z
                goto    cmd6
                bcf     GPIO, 0 
                goto    loop

; command 0x20 set GPIO0=1 output 
cmd6            movf    i2crec1, W
                sublw   H'20'
                btfss   STATUS, Z
                goto    cmd7
                bsf     GPIO, 0 
                goto    loop

; command 0x11 clear GPIO1=0 output 
cmd7            movf    i2crec1, W
                sublw   H'11'
                btfss   STATUS, Z
                goto    cmd8
                bcf     GPIO, 1 
                goto    loop

; command 0x21 set GPIO1=1 output 
cmd8            movf    i2crec1, W
                sublw   H'21'
                btfss   STATUS, Z
                goto    cmd9
                bsf     GPIO, 1 
                goto    loop

; command 0x14 clear GPIO4=0 output 
cmd9            movf    i2crec1, W
                sublw   H'14'
                btfss   STATUS, Z
                goto    cmd10
                bcf     GPIO, 4 
                goto    loop

; command 0x24 set GPIO4=1 output 
cmd10           movf    i2crec1, W
                sublw   H'24'
                btfss   STATUS, Z
                goto    cmd11
                bsf     GPIO, 4 
                goto    loop

; command 0x15 clear GPIO5=0 output 
cmd11           movf    i2crec1, W
                sublw   H'15'
                btfss   STATUS, Z
                goto    cmd12
                bcf     GPIO, 5 
                goto    loop

; command 0x25 set GPIO5=1 output 
cmd12           movf    i2crec1, W
                sublw   H'25'
                btfss   STATUS, Z
                goto    cmd13
                bsf     GPIO, 5 
                goto    loop

; command 0x30 write byte to GPIO 
cmd13           movf    i2crec1, W
                sublw   H'30'
                btfss   STATUS, Z
                goto    cmd14
                movf    i2crec2, W
                movwf   GPIO 
                goto    loop

; command 0x31 write byte to TRISIO 
cmd14           movf    i2crec1, W
                sublw   H'31'
                btfss   STATUS, Z
                goto    cmd15
                movf    i2crec2, W
                bsf     STATUS, RP0         ; bank 1
                movwf   TRISIO
                bcf     STATUS, RP0         ; bank 0
                goto    loop

; command 0x32 AND byte with GPIO 
cmd15           movf    i2crec1, W
                sublw   H'32'
                btfss   STATUS, Z
                goto    cmd16
                movf    i2crec2, W
                andwf   GPIO, F 
                goto    loop

; command 0x33 OR byte with GPIO 
cmd16           movf    i2crec1, W
                sublw   H'33'
                btfss   STATUS, Z
                goto    cmd17
                movf    i2crec2, W
                iorwf   GPIO, F 
                goto    loop

; command 0x34 XOR byte with GPIO 
cmd17           movf    i2crec1, W
                sublw   H'34'
                btfss   STATUS, Z
                goto    cmd18
                movf    i2crec2, W
                xorwf   GPIO, F 
                goto    loop

; command 0x40 read AN0 analog input voltage
cmd18           movf    i2crec1, W
                sublw   H'40'
                btfss   STATUS, Z
                goto    cmd19
                movlw   B'10000001' ; switch on A/D, VREF=VDD, AN0
                movwf   ADCON0
                bsf     ADCON0, GO  ; start conversion
wadc0           btfss   ADCON0, NOT_DONE
                goto    wadc0
                movf    ADRESH, W 
                movwf   i2ctx1 
                bsf     STATUS, RP0         ; bank 1
                movf    ADRESL, W
                movwf   i2ctx2
                bcf     STATUS, RP0         ; bank 0
                goto    loop

; command 0x41 read AN1 analog input voltage
cmd19           movf    i2crec1, W
                sublw   H'41'
                btfss   STATUS, Z
                goto    cmd20
                movlw   B'10000101' ; switch on A/D, VREF=VDD, AN1
                movwf   ADCON0
                bsf     ADCON0, GO  ; start conversion
wadc1           btfss   ADCON0, NOT_DONE
                goto    wadc1
                movf    ADRESH, W 
                movwf   i2ctx1 
                bsf     STATUS, RP0         ; bank 1
                movf    ADRESL, W
                movwf   i2ctx2
                bcf     STATUS, RP0         ; bank 0
                goto    loop

; command 0x43 read AN3 analog input voltage
cmd20           movf    i2crec1, W
                sublw   H'43'
                btfss   STATUS, Z
                goto    cmd21
                movlw   B'10001101' ; switch on A/D, VREF=VDD, AN3
                movwf   ADCON0
                bsf     ADCON0, GO  ; start conversion
wadc3           btfss   ADCON0, NOT_DONE
                goto    wadc3
                movf    ADRESH, W 
                movwf   i2ctx1 
                bsf     STATUS, RP0         ; bank 1
                movf    ADRESL, W
                movwf   i2ctx2
                bcf     STATUS, RP0         ; bank 0
                goto    loop

; command 0x50 reset internal timer
cmd21           movf    i2crec1, W
                sublw   H'50'
                btfss   STATUS, Z
                goto    cmd22
                clrf    time1
                clrf    time2
                clrf    time3
                clrf    time4
                goto    loop 

; command 0x51 read internal timer
cmd22           movf    i2crec1, W
                sublw   H'51'
                btfss   STATUS, Z
                goto    cmd23
                movf    time1, W
                movwf   i2ctx1
                movf    time2, W
                movwf   i2ctx2
                movf    time3, W
                movwf   i2ctx3
                movf    time4, W
                movwf   i2ctx4
                goto    loop 

cmd23           nop
                goto    loop

; increase internal timer every 0.524288 seconds (assuming 1:8 prescaler) 
nxtime          bcf     PIR1, TMR1IF
                incf    time4, F            ; time4++ 
                btfss   STATUS, Z
                goto    timedone               
                incf    time3, F            ; time3++
                btfss   STATUS, Z 
                goto    timedone               
                incf    time2, F            ; time2++
                btfss   STATUS, Z
                goto    timedone
                incf    time1, F            ; time1++
 
timedone        nop       
                goto    loop

                end
