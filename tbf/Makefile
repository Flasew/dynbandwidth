# SPDX-License-Identifier: GPL-2.0

all: 
	gcc -O3 tbf_direct.c -L. -l:libnetlink.a -l:libtc.a -l:libutil.a -o tbfd
	gcc -O3 tbf.c -L. -l:libnetlink.a -l:libtc.a -l:libutil.a -o tbf
	gcc -O3 tbf_peak.c -L. -l:libnetlink.a -l:libtc.a -l:libutil.a -o tbfp
	
clean:
	rm tbf tbfp

.EXPORT_ALL_VARIABLES:
