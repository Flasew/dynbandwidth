#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <sched.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <sys/types.h>
#include <sys/time.h>

#include "utils.h"
#include "tc_core.h"
#include "tc_util.h"
#include "tc_common.h"
#include "libnetlink.h"

#define HBW 10000000000ULL
#define LBW 1000000000UL
#define MTU 2500
#define QL  60

#define DT_NS 101625000 
#define DT_S 0

//#define TIMING

struct rtnl_handle rth;

int main(int argc, char * argv[]) {

  __u64 rate64 = HBW;
  __u64 bws[] = {HBW, LBW};
  unsigned int index = 1;

#ifdef TIMING
  struct timespec tss, tsf;
#endif

  struct timespec intime = {DT_S, DT_NS};
  struct timespec outtime;

  int pid = getpid();
  struct sched_param param;
  memset(&param, 0, sizeof(param));
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  sched_setscheduler(pid, SCHED_FIFO, &param);

  int ret = 0;
  tc_core_init();

  if (rtnl_open(&rth, 0) < 0) {
    fprintf(stderr, "Cannot open rtnetlink\n");
    exit(1);
  }

  // tc_qdisc.c

  char  d[IFNAMSIZ] = "enp1s0";
  char  k[FILTER_NAMESZ] = "tbf";
  struct {
    struct nlmsghdr n;
    struct tcmsg    t;
    char      buf[TCA_BUF_MAX];
  } req = {
    .n.nlmsg_flags = NLM_F_REQUEST,
    .n.nlmsg_type = RTM_NEWQDISC,
    .t.tcm_family = AF_UNSPEC,
  };

  if (d[0])  {
    int idx;

    ll_init_map(&rth);

    idx = ll_name_to_index(d);
    if (!idx)
      return -nodev(d);
    req.t.tcm_ifindex = idx;
  }

  req.t.tcm_parent = TC_H_ROOT;

  // q_tbf.c
  struct nlmsghdr * n = &req.n;

  struct tc_tbf_qopt opt = {};
  __u32 rtab[256];
  __u32 ptab[256];
  unsigned buffer = (10000000 >> 3) , mtu = MTU, latency = 0;
  int Rcell_log =  -1, Pcell_log = -1;
  unsigned int linklayer = LINKLAYER_ETHERNET; /* Assume ethernet */
  struct rtattr *tail;
  opt.limit = mtu * QL;
  opt.peakrate.rate = 0;

  opt.rate.mpu      = 0;
  opt.rate.overhead = 0;
  
  while (1) {
    
#ifdef TIMING
    clock_gettime(CLOCK_REALTIME, &tss);
#endif

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
    addattr_l(n, sizeof(req), TCA_KIND, k, strlen(k)+1);

    rate64 = bws[(index^=1)] >> 3;
    opt.rate.rate = (rate64 >= (1ULL << 32)) ? ~0U : rate64;
    opt.peakrate.rate = opt.rate.rate | (opt.rate.rate >> 4); 

    if (tc_calc_rtable(&opt.rate, rtab, Rcell_log, mtu, linklayer) < 0) {
      fprintf(stderr, "tbf: failed to calculate rate table.\n");
      ret = -1;
      goto exit;
    }

    opt.buffer = tc_calc_xmittime(opt.rate.rate, buffer);

    tail = addattr_nest(n, 1024, TCA_OPTIONS);
    addattr_l(n, 2024, TCA_TBF_PARMS, &opt, sizeof(opt));
    addattr_l(n, 2124, TCA_TBF_BURST, &buffer, sizeof(buffer));
    if (rate64 >= (1ULL << 32))
      addattr_l(n, 2124, TCA_TBF_RATE64, &rate64, sizeof(rate64));
    addattr_l(n, 3024, TCA_TBF_RTAB, rtab, 1024);
    if (opt.peakrate.rate) {
      if (rate64 >= (1ULL << 32))
        addattr_l(n, 3124, TCA_TBF_PRATE64, &rate64, sizeof(rate64));
      addattr_l(n, 3224, TCA_TBF_PBURST, &mtu, sizeof(mtu));
      addattr_l(n, 4096, TCA_TBF_PTAB, ptab, 1024);
    }
    addattr_nest_end(n, tail);

    if (rtnl_talk(&rth, &req.n, NULL) < 0) {
      ret = 2;
      goto exit;
    }

#ifdef TIMING
    clock_gettime(CLOCK_REALTIME, &tsf);
    printf("Time: %ld\n", tsf.tv_nsec - tss.tv_nsec);
#endif

    nanosleep(&intime, &outtime);

  }

  // end q_tbf.c


exit: 
  rtnl_close(&rth);
  return ret;
}
