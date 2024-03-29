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

#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <libnetfilter_queue/pktbuff.h>
#include <libnetfilter_queue/libnetfilter_queue_ipv4.h>
#include <libnetfilter_queue/libnetfilter_queue_tcp.h>

#include <pthread.h>
#include <ev.h>
#include "rbtree.h"

#define PBUF_SIZ 4194304
#define MAX_QL 1048756
#define FBUF_SIZ 256
#define FBUF_TOUT 1.0

static inline bool before(__u32 seq1, __u32 seq2)
{
        return (__s32)(seq1-seq2) < 0;
}
#define after(seq2, seq1)   before(seq1, seq2)

struct nfq_flowbuf {
  uint16_t sport;
  uint16_t dport;
  uint32_t size;
  uint32_t expected_next;
  struct nfq_flowbuf * prev;
  struct nfq_flowbuf * next;
  struct rb_root root;
  ev_tstamp timeout;
  ev_tstamp last_activity; // time of last activity
  ev_timer timer;
};

struct nfq_flowdata {
  struct rb_node node;
  uint32_t seq;
  uint32_t packet_id;
  uint16_t seg_size;
};

// struct nfq_flowbuf_head {
//   struct nfq_flowbuf * next;
// };

struct nfq_config {
  int fd;
  struct nfq_handle * handler; 
  struct nfq_q_handle * queue;

  struct nfq_flowbuf * reorder_buf[65536];
  
  // pthread_mutex_t lock;                   
  // pthread_mutexattr_t l_attr;
  // pthread_attr_t attr;
  // pthread_t verd_thread;
  // pthread_t tout_thread;
};

int nfq_init(struct nfq_config * n);
int nfq_cb(struct nfq_q_handle *queue, struct nfgenmsg *nfmsg, struct nfq_data *nfad, void *data);

static inline int send_packet_at(struct nfq_q_handle * queue, 
                                 struct nfq_flowbuf * fbuf, 
                                 struct rb_node * rb_node);

static inline int insert_or_send_packet(struct nfq_q_handle * queue, 
                             struct nfq_flowbuf * fbuf, 
                             uint32_t seq, 
                             uint32_t packet_id,
                             uint16_t seg_size);

static int empty_and_destroy_buf(struct nfq_q_handle * queue, struct nfq_flowbuf * fbuf);

// void run_verd(void * data);
// void run_tout(void * data);
static void timer_cb(EV_P_ ev_timer *w, int revents);
static void print_iphdr(unsigned char * buffer);
static void print_tcphdr(unsigned char * buffer);

#endif
