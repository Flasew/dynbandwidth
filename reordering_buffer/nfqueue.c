#include "nfqueue.h"

#define DEBUG 1

struct nfq_config nfq;
struct ev_loop *loop;

int main(int argc, char * argv[]) {
  
  int len;
  char buffer[PBUF_SIZ];
  struct nfq_config nfq;

  if (nfq_init(&nfq) < 0) 
    exit(1);

  // pthread_create(&(n->verd_thread), &(n->attr), run_verd, &nfq);
  // pthread_create(&(n->tout_thread), &(n->attr), run_tout, &nfq);

  int pid = getpid();
  struct sched_param param;
  memset(&param, 0, sizeof(param));
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  sched_setscheduler(pid, SCHED_FIFO, &param);
  // pthread_setschedparam(n->verd_thread, SCHED_FIFO, &param);
  // 
  loop = EV_DEFAULT;
  ev_run (loop, 0);

  memset(buffer, 0, PBUF_SIZ);

  while ((len = read(nfq.fd, buffer, PBUF_SIZ)) > 0) {
    nfq_handle_packet(nfq.handler, buffer, len);
  }



  return 0;

}

int nfq_init(struct nfq_config * n) {

  int ret;

  memset(n, 0, sizeof(struct nfq_config));

  n->handler = nfq_open();    
  if (!n->handler) {
    fprintf(stderr, "Error: cannot open nfq\n");
    return -1;
  }
  
  n->queue = nfq_create_queue(n->handler, 0, nfq_cb, NULL);
  if (!n->queue) {
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

  // pthread_attr_init(&(n->attr));
  // pthread_attr_setdetachstate(&(n->attr), PTHREAD_CREATE_JOINABLE);
  // pthread_attr_setscope(&(n->attr), PTHREAD_SCOPE_SYSTEM);

  // pthread_mutexattr_init(&(n->l_attr));
  // pthread_mutexattr_settype(&(n->l_attr), PTHREAD_MUTEX_RECURSIVE);
  // pthread_mutex_init(&(n->lock), &(n->l_attr));

  return 0;
}


// void run_verd(void * data) {
// }
// void run_tout(void * data){
// }

int nfq_cb(struct nfq_q_handle *queue, struct nfgenmsg *nfmsg, struct nfq_data *nfad, void *data) {

  struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfad);
  uint32_t id = ntohl(ph->packet_id);

  if (DEBUG) 
    fprintf(stderr, "Received packet with id %u\n", id);

  unsigned char * packet = NULL;
  int len = nfq_get_payload(nfad, &packet);

  // struct nfq_config * nfq = (struct nfq_config *)data;

  struct iphdr * ip = (struct iphdr*)(packet);
  struct tcphdr * tcp = (struct tcphdr*)(packet + 4*(unsigned int)ip->ihl);
  
  if (DEBUG) {
    print_iphdr((unsigned char *)ip);
    print_tcphdr((unsigned char *)tcp);
  }

  /*
    if (DEBUG) {
      fprintf(stderr, "Verdicted packet %u\n", id);
    }

  return  nfq_set_verdict(queue, id, NF_ACCEPT, 0, NULL);
  */

  uint16_t sport = ntohs(tcp->th_sport);
  uint16_t dport = ntohs(tcp->th_dport);
  uint16_t psize = ntohs(ip->tot_len) - ((unsigned int)ip->ihl + tcp->th_off) * 4;
  uint32_t seq   = ntohl(tcp->th_seq);

  struct nfq_flowbuf * fbuf = NULL;

  if (nfq.reorder_buf[sport] == NULL) {
     
    fbuf = calloc(sizeof(struct nfq_flowbuf), 1);
    nfq.reorder_buf[sport] = fbuf;
    fbuf->sport = sport; 
    fbuf->dport = dport; 
    fbuf->root = RB_ROOT;
    fbuf->expected_next = seq;
    fbuf->last_activity = ev_now (EV_A);
    ev_init(&fbuf->timer, timer_cb);
    fbuf->timer.data = fbuf;
    timer_cb(EV_A_ &fbuf->timer, 0);
  }
  else {
    struct nfq_flowbuf * curr = nfq.reorder_buf[sport];
    struct nfq_flowbuf * tail = curr; 
    while (curr) {
      if (curr->dport == dport) {
        fbuf = curr;
        break;
      }
      tail = curr;
      curr = curr->next; 
    }
    if (!fbuf) {
      fbuf = calloc(sizeof(struct nfq_flowbuf), 1);
      fbuf->prev = tail;
      tail->next = fbuf;
      fbuf->sport = sport; 
      fbuf->dport = dport; 
      fbuf->root = RB_ROOT;
    fbuf->expected_next = seq;
      fbuf->last_activity = ev_now (EV_A);
      ev_init(&fbuf->timer, timer_cb);
    fbuf->timer.data = fbuf;
      timer_cb(EV_A_ &fbuf->timer, 0);
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
      fprintf(stderr, "Verdicted packet %u\n", id);
    }

    nfq_set_verdict(queue, id, NF_ACCEPT, 0, NULL);
    if (tcp->th_flags & TH_SYN)
      fbuf->expected_next = seq + 1;
    else
      fbuf->expected_next = seq + psize;
    fbuf->last_activity = ev_now(EV_A);

    struct rb_node * node = rb_first(&(fbuf->root));
    while (node && rb_entry(node, struct nfq_flowdata, node)->seq == fbuf->expected_next) {
      send_packet_at(queue, fbuf, node);
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
    
    return insert_or_send_packet(queue, fbuf, seq, id, psize);

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
    uint32_t lowest_seq = rb_entry(first_node, struct nfq_flowdata, node)->seq;

    if (DEBUG) {
      fprintf(stderr, "CASE buffer full, in seq %u, stored lowest seq %u\n", seq, lowest_seq);
    }

    if (lowest_seq <= seq) {

      send_packet_at(queue, fbuf, first_node);
      insert_or_send_packet(queue, fbuf, seq, id, psize);
      
    }

    else {

      if (DEBUG) 
        fprintf(stderr, "Verdicted packet %u\n", id);

      nfq_set_verdict(queue, id, NF_ACCEPT, 0, NULL);
      fbuf->last_activity = ev_now(EV_A);

    }
  }

  // print_iphdr(ip);
  // print_tcphdr(tcp);

  return -1;

}

static void timer_cb (EV_P_ ev_timer *w, int revents) {
  // calculate when the timeout would happen
  struct nfq_flowbuf * fbuf = (struct nfq_flowbuf *)w->data;
  ev_tstamp after = fbuf->last_activity - ev_now (EV_A) + fbuf->timeout;
 
  // if negative, it means we the timeout already occurred
  if (after < 0.) {
    // timeout occurred, take action
    empty_and_destroy_buf(nfq.queue, fbuf);
  }

  else {
    // callback was invoked, but there was some recent 
    // activity. simply restart the timer to time out
    // after "after" seconds, which is the earliest time
    // the timeout can occur.
    ev_timer_set (w, after, 0.);
    ev_timer_start (EV_A_ w);
  }
}

static inline int insert_or_send_packet(struct nfq_q_handle * queue, 
                             struct nfq_flowbuf * fbuf, 
                             uint32_t seq, 
                             uint32_t packet_id,
                             uint16_t seg_size) 
{
  struct rb_node **new_node = &(fbuf->root.rb_node), *parent = NULL;

  // Figure out where to put new_node node 
  // if this sequence number somehow collide with an existing one, 
  // let this one out. 
  while (*new_node) {
    struct nfq_flowdata *curr = container_of(*new_node, struct nfq_flowdata, node);

    parent = *new_node;
    if (before(seq, curr->seq))
      new_node = &((*new_node)->rb_left);
    else if (after(seq, curr->seq))
      new_node = &((*new_node)->rb_right);

    // retransmit. leave the larger or newer packet...
    else {
      int ret;
      if (curr->seg_size > seg_size) {
        if (DEBUG) 
          fprintf(stderr, "Verdicted packet %u\n", packet_id);
        ret = nfq_set_verdict(queue, packet_id, NF_DROP, 0, NULL);
      }
      else {
        uint32_t oldid = curr->packet_id;
        curr->packet_id = packet_id;
        curr->seg_size =  seg_size;
        if (DEBUG) 
          fprintf(stderr, "Verdicted packet %u\n", oldid);
        ret = nfq_set_verdict(queue, oldid, NF_DROP, 0, NULL);
      }
      fbuf->last_activity = ev_now(EV_A);
      return ret;
    }
  }

  // do insertion

  fbuf->size++;
  struct nfq_flowdata * newdata = calloc(sizeof(struct nfq_flowdata), 1);
  newdata->seq = seq;
  newdata->packet_id = packet_id;
  newdata->seg_size = seg_size;

  struct rb_node * currfirst = rb_first(&(fbuf->root));
  /* Add new_node node and rebalance tree. */
  rb_link_node(&newdata->node, parent, new_node);
  rb_insert_color(&newdata->node, &(fbuf->root));
  fbuf->last_activity = ev_now(EV_A);

  return 0;

}

static inline int send_packet_at(struct nfq_q_handle * queue, 
                                 struct nfq_flowbuf * fbuf, 
                                 struct rb_node * rb_node) 
{
  struct nfq_flowdata * fdata = rb_entry(rb_node, struct nfq_flowdata, node);
  
  if (DEBUG) 
    fprintf(stderr, "Verdicted packet %u\n", fdata->packet_id);
  int ret = nfq_set_verdict(queue,
                            fdata->packet_id,
                            NF_ACCEPT, 0, NULL);

  if (fdata->seq + fdata->seg_size > fbuf->expected_next) 
    fbuf->expected_next = fdata->seq + fdata->seg_size;

  rb_erase(rb_node, &(fbuf->root));
  free(container_of(rb_node, struct nfq_flowdata, node));
  fbuf->last_activity = ev_now(EV_A);
  fbuf->size--;
  return ret;
}

static int empty_and_destroy_buf(struct nfq_q_handle * queue, struct nfq_flowbuf * fbuf) {

  struct rb_node * node = rb_first(&(fbuf->root));
  int ret = 1;

  while (node) {
    ret &= send_packet_at(queue, fbuf, node);
    node = rb_first(&(fbuf->root));
  }

  struct nfq_flowbuf * myprev = fbuf->prev;
  struct nfq_flowbuf * mynext = fbuf->next;

  if (myprev) 
    myprev->next = mynext;
  else 
    nfq.reorder_buf[fbuf->sport] = mynext;

  if (mynext)
    mynext->prev = myprev;

  return ret;

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
