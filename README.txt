
PiPIC is a PIC processor with i2c interface that is connected to Raspberry Pi.
The PiPIC can execute some simple hardware commands on its I/O pins and
can be used as a helper I/O processor with the Raspberry Pi. The code has
been implemented on PIC 12f675 (pic12si2c.asm). On the Raspberry Pi the 
program pipic(1) is used to send commands to PiPIC and read data from PiPIC. 
In addition the memory content from PiPIC can be printed with pipicfile(1).

The PiPIC on 12f675 can be connected to the Raspberry Pi with four
wires:

Raspi            12f675
3V3 power   ---- Vdd
Ground      ---- Vss
SDA (GPIO0) ---- GP2/INT
SCL (GPIO1) ---- GP3

GP0, GP1, GP4 and GP5 can be used for digital I/O and/or as analog inputs on
GP0, GP1 and GP4.

There are also some stand alone testing programs that can be used with 
Raspberry Pi and rpp/rpp-tlc. The authors of the programming interface and 
rpp/rpp-tlc can be found from following links

http://holdenc.altervista.org/rpp/

http://www.volny.cz/tlc/rpp/

See the RaspiPICprog directory on how the programming interface could be build
on PCB.

