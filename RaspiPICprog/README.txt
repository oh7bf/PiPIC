
Raspberry Pi PIC programming interface
======================================

The authors of the programming interface and rpp/rpp-tlc can be found from 
following links

http://holdenc.altervista.org/rpp/

http://www.volny.cz/tlc/rpp/

This directory documents one way to implement PCB for this interface.

Parts
-----

C1,C2                100 nF/25 V 5 mm 
C3                   4.7 uF/25 V 5 mm	
CONN1                screw terminal	
D1                   LED 3 mm	
D2                   LED 3 mm	
D3	             1N4004 
J1                   2 pins header + PCB jumper	
J2,J3                5 pins header
Q1,Q3,Q5,Q7          2N3904	
Q2,Q4,Q6             2N3906	
R1-R8,R11-R13,R15    100k	
R9,R14               10k 
R10                  68k 
R16	             820 
R17                  1k8 
U1                   78L05


Files
-----

attribs                - used by 'gnetlist -g bom -o bom.txt file.sch'
board_component.pdf    - PCB component placement
board_mirror.pdf       - PCB for toner transfer production, A4 size
board.pcb              - PCB file
board_silk_mirror.pdf  - silk screen for toner transfer printing
board_silk.pdf         - silk screen
raspipicprog.pdf       - circuit diagram
raspipicprog.png
raspipicprog.sch
README.txt             - this file

Note: two jumper wires need to be soldered on this one sided board. 
