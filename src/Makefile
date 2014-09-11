# -*- mode: makefile -*-
# 
# Makefile for compiling PiPIC programs on Raspberry Pi. 
#
# Wed May 14 22:55:55 CEST 2014
# Edit: Sun Aug 10 20:31:46 CEST 2014
# Jaakko Koivuniemi

CXX           = gcc
CXXFLAGS      = -g -O -Wall 
LD            = gcc
LDFLAGS       = -O 

%.o : %.c
	$(CXX) $(CXXFLAGS) -c $<

all: pipic pipicfile pipicpowerd pipicswd pipicsw pipictest

pipic: pipic.o
	$(LD) $(LDFLAGS) $^ -o $@

pipicfile: pipicfile.o
	$(LD) $(LDFLAGS) $^ -o $@

pipicpowerd: pipicpowerd.o
	$(LD) $(LDFLAGS) -lm $^ -o $@

pipicswd: pipicswd.o
	$(LD) $(LDFLAGS) $^ -o $@

pipicsw: pipicsw.o
	$(LD) $(LDFLAGS) $^ -o $@

pipicswitch: pipicswitch.o
	$(LD) $(LDFLAGS) $^ -o $@

pipictest: pipictest.o
	$(LD) $(LDFLAGS) $^ -o $@

pipichbd: pipichbd.o logmessage.o writecmd.o readdata.o testi2c.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o

