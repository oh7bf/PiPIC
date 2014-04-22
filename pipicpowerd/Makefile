# -*- mode: makefile -*-
# 
# Makefile for compiling 'pipicpowerd' on Raspberry Pi. 
#
# Mon Sep 30 19:17:13 CEST 2013
# Edit: 
# Jaakko Koivuniemi

CXX           = gcc
CXXFLAGS      = -g -O -Wall 
LD            = gcc
LDFLAGS       = -O -lm

%.o : %.c
	$(CXX) $(CXXFLAGS) -c $<

all: pipicpowerd 

pipicpowerd: pipicpowerd.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o

