#include "nfqueue.h"

int main(int argc, char * argv[]) {
  
  int ret, len;

  // Deal with netfilter queue
  handler = nfq_open();    
  if (!handler) {
    fprintf(stderr, "Error: cannot open nfq\n");
    exit(1);
  }
  
  queue = nfq_create_queue(handler, 0, nf_cb, NULL);
  if (!queue) {
    fprintf(stderr, "Error: cannot create queue\n");
    exit(1);
  }

  ret = nfq_set_mode(queue, NFQNL_COPY_PACKET, 40); // TCP IP header
  if (ret < 0) {
    fprintf(stderr, "Error: cannot set mode\n");
    exit(1);
  }

  ret = nfq_set_queue_flags(queue, NFQA_CFG_F_FAIL_OPEN, NFQA_CFG_F_FAIL_OPEN);
  if (ret < 0) {
    fprintf(stderr, "Error: cannot set flags\n");
    exit(1);
  }

  ret = nfq_set_queue_maxlen(queue, MAX_QL);
  if (ret < 0) {
    fprintf(stderr, "Error: cannot set queue length\n");
    exit(1);
  }

  memset(buffer, 0, PBUF_SIZ);
  fd = nfq_fd(handler);

  while ((len = read(fd, buffer, PBUF_SIZ)) > 0) {
    nfq_handle_packet(handler, buffer, len);
  }

  return 0;

}

static int nf_cb(struct nfq_q_handle *queue, struct nfgenmsg *nfmsg, struct nfq_data *nfad, void *data) {

  struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfad);

  unsigned char *buffer = NULL;
  int len = nfq_get_payload(nfad, &buffer);

  print_iphdr(buffer);
  print_tcphdr(buffer + sizeof(struct iphdr));

  return nfq_set_verdict(queue, ntohl(ph->packet_id), NF_ACCEPT, 0, NULL);
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
  fprintf(stderr , "\t|-Internet Header Length : %d DWORDS or %d Bytes\n",(unsigned int)ip->ihl,((unsigned int)(ip->ihl))*4); 
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
}
