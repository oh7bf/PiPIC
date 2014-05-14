# -*- mode: makefile -*-
# 
# Makefile for compiling 'pipicswitch' on Raspberry Pi. 
#
# Sat Feb 15 18:49:41 CET 2014
# Edit: Wed Feb 19 21:44:24 CET 2014
# Jaakko Koivuniemi

CXX           = gcc
CXXFLAGS      = -g -O -Wall 
LD            = gcc
LDFLAGS       = -O

%.o : %.c
	$(CXX) $(CXXFLAGS) -c $<

all: pipicswitch pipicswd pipicsw 

pipicswitch: pipicswitch.o
	$(LD) $(LDFLAGS) $^ -o $@

pipicswd: pipicswd.o
	$(LD) $(LDFLAGS) $^ -o $@

pipicsw: pipicsw.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o

