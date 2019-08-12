#ifndef _NFQUEUE_H_
#define _NFQUEUE_H_ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>

#include <linux/rbtree.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <libnetfilter_queue/pktbuff.h>
#include <libnetfilter_queue/libnetfilter_queue_ipv4.h>
#include <libnetfilter_queue/libnetfilter_queue_tcp.h>

#include <pthread.h>

#define PBUF_SIZ 0xffff
#define MAX_QL 32768
#define FBUF_SIZ 512

struct nfq_flowbuf {
  int size;
  uint32_t expected_next;
  struct rbroot sfifo;
};

struct nfq_flowdata {
  uint32_t seq;
  uint32_t packet_id;
  struct timespec timestamp;
};

struct nfq_config {
  int fd;
  struct nfq_handle * handler; 
  struct nfq_q_handle * queue;
  
  pthread_mutex_t lock;                   
  pthread_mutexattr_t l_attr;
  pthread_attr_t attr;
  pthread_t verd_thread;
  pthread_t tout_thread;
};

int nfq_init(struct nfq_config * n);
int nfq_cb(struct nfq_q_handle *queue, struct nfgenmsg *nfmsg, struct nfq_data *nfad, void *data);
void run_verd(void * data);
void run_tout(void * data);

static void print_iphdr(unsigned char * buffer);
static void print_tcphdr(unsigned char * buffer);

#endif
