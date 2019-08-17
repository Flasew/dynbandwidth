#include "reorder_counter.h"

#define DEBUG 1

struct rc_config rc;

int main(int argc, char * argv[]) {
  
  int len;
  uint8_t buffer[PBUF_SIZ];

  int in_sfd, out_sfd;
  struct sockaddr saddr;
  int saddr_len = sizeof (saddr);

  in_sfd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));

  struct sockaddr_ll sll;
  struct ifreq ifr; 
  bzero(&sll , sizeof(sll));
  bzero(&ifr , sizeof(ifr));
  strncpy((char *)ifr.ifr_name ,"enp1s0" , IFNAMSIZ);
  //copy device name to ifr
  if((ioctl(in_sfd , SIOCGIFINDEX , &ifr)) == -1)
  {
    perror("Unable to find interface index");
    exit(-1);
  }
  sll.sll_family = PF_PACKET;
  sll.sll_ifindex = ifr.ifr_ifindex;
  sll.sll_protocol = htons(ETH_P_IP);
  if((bind(in_sfd , (struct sockaddr *)&sll , sizeof(sll))) ==-1)
  {
    perror("bind: ");
    exit(-1);
  }

  if (signal(SIGINT, catch_function) == SIG_ERR) {
    fputs("An error occurred while setting a signal handler.\n", stderr);
    return EXIT_FAILURE;
  }

  memset(buffer, 0, PBUF_SIZ);

  while ((len = recvfrom(in_sfd, buffer, PBUF_SIZ, 0, &saddr, (socklen_t *)&saddr_len)) > 0) {
    rc_handle_packet(buffer, len);
  }

  return 0;

}

int rc_handle_packet(uint8_t * data, int len) {

  struct iphdr * ip = (struct iphdr*)(data);
  struct tcphdr * tcp = (struct tcphdr*)(data + 4*(unsigned int)ip->ihl);
  
  if (DEBUG) {
    print_iphdr((unsigned char *)ip);
    print_tcphdr((unsigned char *)tcp);
  }

  uint16_t sport = ntohs(tcp->th_sport);
  uint16_t dport = ntohs(tcp->th_dport);
  uint16_t psize = ntohs(ip->tot_len) - ((unsigned int)ip->ihl + tcp->th_off) * 4;
  uint32_t seq   = ntohl(tcp->th_seq);

  struct rc_flowbuf * fbuf = NULL;

  if (rc.reorder_buf[sport] == NULL) {
     
    fbuf = calloc(sizeof(struct rc_flowbuf), 1);
    rc.reorder_buf[sport] = fbuf;
    fbuf->sport = sport; 
    fbuf->dport = dport; 
    fbuf->root = RB_ROOT;
    fbuf->expected_next = seq;
  }
  else {
    struct rc_flowbuf * curr = rc.reorder_buf[sport];
    struct rc_flowbuf * tail = curr; 
    while (curr) {
      if (curr->dport == dport) {
        fbuf = curr;
        break;
      }
      tail = curr;
      curr = curr->next; 
    }
    if (!fbuf) {
      fbuf = calloc(sizeof(struct rc_flowbuf), 1);
      fbuf->prev = tail;
      tail->next = fbuf;
      fbuf->sport = sport; 
      fbuf->dport = dport; 
      fbuf->root = RB_ROOT;
      fbuf->expected_next = seq;
    }
  }

  // Lowest First Resequencing Algorithm
  
  // IF (expected_num equals current_packet_num)
  // begin
  //   Release packet into output queue.
  //   Increment expected_num by 1
  //   While (expected_num in resequencing buffer)
  //     {Release that packet into output queue
  //     Increment expected_num by 1}
  // end

  if (before(seq, fbuf->expected_next) || seq == fbuf->expected_next) {

    if (DEBUG) {
      fprintf(stderr, "CASE seq <= expected, seq = %u, exp = %u\n", seq, fbuf->expected_next);
    }

    if (tcp->th_flags & TH_SYN)
      fbuf->expected_next = seq + 1;
    else
      fbuf->expected_next = seq + psize;

    struct rb_node * node = rb_first(&(fbuf->root));
    while (node && rb_entry(node, struct rc_flowdata, node)->seq == fbuf->expected_next) {
      send_packet_at(fbuf, node);
      node = rb_first(&(fbuf->root));
    }

    if (DEBUG) 
      fprintf(stderr, "CASE seq <= expected END \n");

    return 1;
  }

  // ELSE IF(re-sequencing buffer is not full)
  // begin
  //   Store packet in re-sequencing buffer
  // end
  else if (fbuf->size < FBUF_SIZ) {

    if (DEBUG) 
      fprintf(stderr, "CASE can buffer, seq = %u, exp = %u\n", seq, fbuf->expected_next);
    
    return insert_or_send_packet(fbuf, seq, psize);

    if (DEBUG) 
      fprintf(stderr, "CASE can buffer END\n");

  }

  // ELSE // re-sequencing buffer is full
  // begin
  //   Select packet in buffer with lowest sequence number.
  //   IF (selected_packet_num less than current_packet_num) 
  //   begin
  //     Release selected packet into the output queue.
  //     Store current packet in the buffer.
  //   end
  //   ELSE
  //   begin
  //     Release current packet into the output queue.
  //   end 
  // end
  
  else {

    struct rb_node * first_node = rb_first(&(fbuf->root));
    uint32_t lowest_seq = rb_entry(first_node, struct rc_flowdata, node)->seq;

    if (DEBUG) {
      fprintf(stderr, "CASE buffer full, in seq %u, stored lowest seq %u\n", seq, lowest_seq);
    }

    if (lowest_seq <= seq) {

      send_packet_at(fbuf, first_node);
      insert_or_send_packet(fbuf, seq, psize);
      
    }

    if (DEBUG) {
      fprintf(stderr, "CASE buffer full END\n");
    }
  }

  return -1;

}


static inline int insert_or_send_packet(struct rc_flowbuf * fbuf, 
                                        uint32_t seq, 
                                        uint16_t seg_size) {

  struct rb_node **new_node = &(fbuf->root.rb_node), *parent = NULL;

  // Figure out where to put new_node node 
  // if this sequence number somehow collide with an existing one, 
  // let this one out. 
  while (*new_node) {
    struct rc_flowdata *curr = container_of(*new_node, struct rc_flowdata, node);

    parent = *new_node;
    if (before(seq, curr->seq))
      new_node = &((*new_node)->rb_left);
    else if (after(seq, curr->seq))
      new_node = &((*new_node)->rb_right);

    // retransmit. leave the larger or newer packet...
    else {
      if (curr->seg_size > seg_size) {
      }
      else {
        curr->seg_size =  seg_size;
      }
      return 0;
    }
  }

  // do insertion

  fbuf->size++;
  struct rc_flowdata * newdata = calloc(sizeof(struct rc_flowdata), 1);
  newdata->seq = seq;
  newdata->seg_size = seg_size;

  struct rb_node * currfirst = rb_first(&(fbuf->root));
  /* Add new_node node and rebalance tree. */
  rb_link_node(&newdata->node, parent, new_node);
  rb_insert_color(&newdata->node, &(fbuf->root));
  rc.reorder_count++;

  return 0;

}

static void catch_function(int signo) {
  //FILE * filp = fopen("/root/reorder_count.txt", "w");
  fprintf(stdout, "%lu", rc.reorder_count);
    
  exit(0);
}

static inline int send_packet_at( struct rc_flowbuf * fbuf, 
                                 struct rb_node * rb_node) 
{
  struct rc_flowdata * fdata = rb_entry(rb_node, struct rc_flowdata, node);
  
  fbuf->expected_next = fdata->seq + fdata->seg_size;

  rb_erase(rb_node, &(fbuf->root));
  free(container_of(rb_node, struct rc_flowdata, node));
  fbuf->size--;
  return 0;
}

static void print_iphdr(unsigned char * buffer) {

  struct sockaddr_in source, dest;
  unsigned short iphdrlen;
  struct iphdr *ip = (struct iphdr*)(buffer);
  memset(&source, 0, sizeof(source));
  source.sin_addr.s_addr = ip->saddr;
  memset(&dest, 0, sizeof(dest));
  dest.sin_addr.s_addr = ip->daddr;

  fprintf(stderr, "IP header\n");
  fprintf(stderr, "\t|-Version : %d\n",(unsigned int)ip->version); 
  fprintf(stderr , "\t|-Internet Header Length : %d DWORDS or %d Bytes\n",
      (unsigned int)ip->ihl,((unsigned int)(ip->ihl))*4); 
  fprintf(stderr , "\t|-Type Of Service : %d\n",(unsigned int)ip->tos); 
  fprintf(stderr , "\t|-Total Length : %d Bytes\n",ntohs(ip->tot_len)); 
  fprintf(stderr , "\t|-Identification : %d\n",ntohs(ip->id)); 
  fprintf(stderr , "\t|-Time To Live : %d\n",(unsigned int)ip->ttl); 
  fprintf(stderr , "\t|-Protocol : %d\n",(unsigned int)ip->protocol); 
  fprintf(stderr , "\t|-Header Checksum : %d\n",ntohs(ip->check)); 
  fprintf(stderr , "\t|-Source IP : %s\n", inet_ntoa(source.sin_addr)); 
  fprintf(stderr , "\t|-Destination IP : %s\n",inet_ntoa(dest.sin_addr));
}

static void print_tcphdr(unsigned char * buffer) {

  struct tcphdr * tcp = (struct tcphdr*)(buffer);

  fprintf(stderr, "TCP header\n");
  fprintf(stderr, "\t|-Source Port : %u\n", ntohs(tcp->th_sport));
  fprintf(stderr, "\t|-Destination Port : %u\n", ntohs(tcp->th_dport));
  fprintf(stderr, "\t|-Sequence Number : %u\n", ntohl(tcp->th_seq));
  fprintf(stderr, "\t|-ACK Number : %u\n", ntohl(tcp->th_ack));
  fprintf(stderr, "\t|-Header length: %u\n", (unsigned int)(tcp->th_off));

}
