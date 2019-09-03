#ifndef _REORDER_COUNTER_H_
#define _REORDER_COUNTER_H_

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/socket.h>

#include "rbtree.h"

#define PBUF_SIZ 0xffff
#define FBUF_SIZ 1024

static inline bool before(__u32 seq1, __u32 seq2)
{
        return (__s32)(seq1-seq2) < 0;
}
#define after(seq2, seq1)   before(seq1, seq2)

struct rc_flowbuf {
  uint16_t sport;
  uint16_t dport;
  uint32_t size;
  uint32_t expected_next;
  struct rc_flowbuf * prev;
  struct rc_flowbuf * next;
  struct rb_root root;
};

struct rc_flowdata {
  struct rb_node node;
  uint32_t seq;
  uint16_t seg_size;
};

struct rc_config {

  uint64_t reorder_count;
  struct rc_flowbuf * reorder_buf[65536];
  
};

int rc_handle_packet(uint8_t * data, int len);

static inline int send_packet_at(struct rc_flowbuf * fbuf, 
                                 struct rb_node * rb_node);

static inline int insert_or_send_packet(struct rc_flowbuf * fbuf, 
                                        uint32_t seq, 
                                        uint16_t seg_size);


static void print_iphdr(unsigned char * buffer);
static void print_tcphdr(unsigned char * buffer);
static void catch_function(int signo);

#endif
