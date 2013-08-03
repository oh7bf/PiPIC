# -*- mode: makefile -*-
# 
# Makefile for compiling 'pipic' on Raspberry Pi. 
#
# Fri Jul 26 21:19:33 CEST 2013
# Edit: Sat Aug  3 17:18:10 CEST 2013
# Jaakko Koivuniemi

CXX           = gcc
CXXFLAGS      = -g -O -Wall 
LD            = gcc
LDFLAGS       = -O

%.o : %.c
	$(CXX) $(CXXFLAGS) -c $<

all: pipic 

pipic: pipic.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o

