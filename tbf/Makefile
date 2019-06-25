# SPDX-License-Identifier: GPL-2.0
# Top level Makefile for iproute2

all: 
	gcc -O3 tbf_direct.c -L. -l:libnetlink.a -l:libtc.a -l:libutil.a -o tbf
	
clean:
	rm tbf

.EXPORT_ALL_VARIABLES:
