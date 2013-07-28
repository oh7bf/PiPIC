# -*- mode: makefile -*-
# 
# Makefile for compiling 'pipic' on Raspberry Pi. 
#
# Fri Jul 26 21:19:33 CEST 2013
# Edit: 
# Jaakko Koivuniemi

CXX           = g++
CXXFLAGS      = -g -O -Wall 
LD            = g++
LDFLAGS       = -O

%.o : %.cxx
	$(CXX) $(CXXFLAGS) -c $<

all: pipic 

pipic: pipic.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o

