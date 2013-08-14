# -*- mode: makefile -*-
# 
# Makefile for compiling 'pipictest' on Raspberry Pi. 
#
# Wed Aug 14 22:31:50 CEST 2013
# Edit: 
# Jaakko Koivuniemi

CXX           = gcc
CXXFLAGS      = -g -O -Wall 
LD            = gcc
LDFLAGS       = -O

%.o : %.c
	$(CXX) $(CXXFLAGS) -c $<

all: pipictest

pipicfile: pipictest.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o

