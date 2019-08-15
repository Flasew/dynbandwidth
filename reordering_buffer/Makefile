all:
	gcc -O3 -g -c rbtree.c -o rbtree.o
	gcc -O3 -g nfqueue.c rbtree.o -lnetfilter_queue -lev -o rbuf
