all:
	gcc -O3 -g -c rbtree.c -o rbtree.o
	gcc -O3 -g nfqueue.c rbtree.o -lnetfilter_queue -lev -o rbuf
	gcc -O3 -g reorder_counter.c rbtree.o -o rc
	gcc -O3 tcp_reordercounter.c rbtree.o -o rc
