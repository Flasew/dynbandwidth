# SPDX-License-Identifier: GPL-2.0

all: 
	gcc -O3 tbf.c -L. -l:libnetlink.a -l:libtc.a -l:libutil.a -o tbf
	gcc -O3 netem.c -L. -l:libnetlink.a -l:libtc.a -l:libutil.a -o netem
	gcc -O3 tbfnetem.c -L. -l:libnetlink.a -l:libtc.a -l:libutil.a -o tn
	gcc -O3 tbfifb.c -L. -l:libnetlink.a -l:libtc.a -l:libutil.a -o ti
	gcc -O3 tbfenp.c -L. -l:libnetlink.a -l:libtc.a -l:libutil.a -o te
	
clean:
	rm tbf tbfp

.EXPORT_ALL_VARIABLES:
