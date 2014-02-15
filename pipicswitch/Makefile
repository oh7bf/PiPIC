# -*- mode: makefile -*-
# 
# Makefile for compiling 'pipicswitch' on Raspberry Pi. 
#
# Sat Feb 15 18:49:41 CET 2014
# Edit: 
# Jaakko Koivuniemi

CXX           = gcc
CXXFLAGS      = -g -O -Wall 
LD            = gcc
LDFLAGS       = -O

%.o : %.c
	$(CXX) $(CXXFLAGS) -c $<

all: pipicswitch 

pipicswitch: pipicswitch.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o

