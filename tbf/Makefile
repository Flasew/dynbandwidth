# SPDX-License-Identifier: GPL-2.0

all: 
	gcc -O3 tbf.c -L. -l:libnetlink.a -l:libtc.a -l:libutil.a -o tbf
	gcc -O3 netem.c -L. -l:libnetlink.a -l:libtc.a -l:libutil.a -o netem
	
clean:
	rm tbf tbfp

.EXPORT_ALL_VARIABLES:
