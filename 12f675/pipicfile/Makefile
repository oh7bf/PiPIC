# -*- mode: makefile -*-
# 
# Makefile for compiling 'pipicfile' on Raspberry Pi. 
#
# Fri Aug  2 21:10:34 CEST 2013
# Edit: 
# Jaakko Koivuniemi

CXX           = gcc
CXXFLAGS      = -g -O -Wall 
LD            = gcc
LDFLAGS       = -O

%.o : %.c
	$(CXX) $(CXXFLAGS) -c $<

all: pipicfile 

pipicfile: pipicfile.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o

