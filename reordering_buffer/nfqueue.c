#include "nfqueue.h"

int main(int argc, char * argv[]) {
  
  int len;
  char buffer[PBUF_SIZ];
  struct nfq_config nfq;

  if (nfq_init(nfq) < 0) 
    exit(1);

  pthread_create(&(n->verd_thread), &(n->attr), run_verd, &nfq);
  pthread_create(&(n->tout_thread), &(n->attr), run_tout, &nfq);

  int pid = getpid();
  struct sched_param param;
  memset(&param, 0, sizeof(param));
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  sched_setscheduler(pid, SCHED_FIFO, &param);
  pthread_setschedparam(n->verd_thread, SCHED_FIFO, &param);

  memset(buffer, 0, PBUF_SIZ);

  while ((len = read(fd, buffer, PBUF_SIZ)) > 0) {
    nfq_handle_packet(handler, buffer, len);
  }

  return 0;

}

int nfq_init(struct nfq_config * n) {

  int ret;

  memset(n, 0, sizeof(struct nfg_config));

  n->handler = nfq_open();    
  if (!n->handler) {
    fprintf(stderr, "Error: cannot open nfq\n");
    return -1;
  }
  
  n->queue = nfq_create_queue(handler, 0, nfq_cb, NULL);
  if (!queue) {
    fprintf(stderr, "Error: cannot create queue\n");
    return -1;
  }

  ret = nfq_set_mode(n->queue, NFQNL_COPY_PACKET, 40); // TCP IP header
  if (ret < 0) {
    fprintf(stderr, "Error: cannot set mode\n");
    return ret;
  }

  ret = nfq_set_queue_flags(n->queue, NFQA_CFG_F_FAIL_OPEN, NFQA_CFG_F_FAIL_OPEN);
  if (ret < 0) {
    fprintf(stderr, "Error: cannot set flags\n");
    return ret;
  }

  ret = nfq_set_queue_maxlen(n->queue, MAX_QL);
  if (ret < 0) {
    fprintf(stderr, "Error: cannot set queue length\n");
    return ret;
  }

  n->fd = nfq_fd(n->handler);
  if (n->fd < 0) {
    fprintf(stderr, "Error: cannot open fd\n");
    return -1;
  }

  pthread_attr_init(&(n->attr));
  pthread_attr_setdetachstate(&(n->attr), PTHREAD_CREATE_JOINABLE);
  pthread_attr_setscope(&(n->attr), PTHREAD_SCOPE_SYSTEM);

  pthread_mutexattr_init(&(n->l_attr));
  pthread_mutexattr_settype(&(n->l_attr), PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&(n->lock), &(n->l_attr));

  return 0;
}


void run_verd(void * data) {
}
void run_tout(void * data){
}

int nfq_cb(struct nfq_q_handle *queue, struct nfgenmsg *nfmsg, struct nfq_data *nfad, void *data) {

  struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfad);
  uint32_t id = ntohl(ph->packet_id);
  unsigned char * data = NULL;
  int len = nfq_get_payload(nfad, &buffer);

  struct iphdr * ip = (struct iphdr*)(buffer);
  struct tcphdr * tcp = (struct tcphdr*)(buffer + 4*(unsigned int)ip->ihl);

  uint16_t sport = ntohs(tcp->th_sport);

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
