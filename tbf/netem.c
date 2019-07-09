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

#define BW 0
#define LAT 1
#define PER 2

// #define PRINT_TIME

struct bw_param {
  __u64 bandwidth;
  __u64 delay;
  struct timespec period;
};

struct rtnl_handle rth;

int main(int argc, char * argv[]) {

  int pid = getpid();
  struct sched_param param;
  memset(&param, 0, sizeof(param));
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  sched_setscheduler(pid, SCHED_FIFO, &param);

  int nbws;

  __u64 rate64;
  struct bw_param * bws;
  unsigned int index = 0;

  __u64 ql = 100;

  __u64 dt_s = 0;
  __u64 dt_ns = 0;

  int ret = 0;
  int ignore;

  char  d1[IFNAMSIZ] = "macvlan0";
  char  d2[IFNAMSIZ] = "macvlan1";

  int idx1, idx2;

  char  k[FILTER_NAMESZ] = "netem";

  struct {
    struct nlmsghdr n;
    struct tcmsg    t;
    char      buf[TCA_BUF_MAX];
  } req = {
    .n.nlmsg_flags = NLM_F_REQUEST,
    .n.nlmsg_type = RTM_NEWQDISC,
    .t.tcm_family = AF_UNSPEC,
  };

  argc--; argv++;
  ql = strtoull(argv, NULL, 10);   argc--; argv++;
  nbws = strtoull(argv, NULL, 10); argc--; argv++;

  if ((unsigned)argc != bws * 3) {
    fprintf(stderr, "Wrong number of arguments. \n");
    exit(1);
  }

  bws = calloc(nbws, sizeof(struct bw_param));
  for (unsigned int i = 0; i < nbws; i++) {
    
    __u64 t_ns; 
    bw_param[i][BW] = strtoull(argv, NULL, 10);    argc--; argv++;
    bw_param[i][BW] >>= 3;
    bw_param[i][LAT] = strtoull(argv, NULL, 10);   argc--; argv++;
    t_ns = strtoull(argv, NULL, 10) * 1000;        argc--; argv++;
    
    bw_param[i][PER].tv_sec = t_ns / 1000000000ULL;
    bw_param[i][PER].tv_nsec = t_ns - dt_s * 1000000000ULL;
    // printf("s: %llu, ns: %llu\n", dt_s, dt_ns);

  }

#ifdef PRINT_TIME
  struct timespec tss, tsf;
#endif
  struct timespec outtime;

  ignore = system("/sbin/tc qdisc del dev macvlan0 root");
  ignore = system("/sbin/tc qdisc del dev macvlan1 root");

  ignore = system("tc qdisc add dev macvlan0 root netem rate 5.5Gbit limit 100");
  ignore = system("tc qdisc add dev macvlan1 root netem rate 5.5Gbit limit 100");
  req.t.tcm_parent = TC_H_ROOT;

  tc_core_init();

  if (rtnl_open(&rth, 0) < 0) {
    fprintf(stderr, "Cannot open rtnetlink\n");
    exit(1);
  }

  // tc_qdisc.c
  ll_init_map(&rth);
  idx1 = ll_name_to_index(d1);
  idx2 = ll_name_to_index(d2);

  int dist_size = 0;
  int slot_dist_size = 0;
  struct rtattr *tail;
  struct tc_netem_qopt opt = { .limit = ql };
  struct tc_netem_rate rate = {};
  __u64 rate64 = 0;
  
  while (1) {
    
#ifdef PRINT_TIME
    clock_gettime(CLOCK_REALTIME, &tss);
#endif

    req.t.tcm_ifindex = idx1;
    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
    addattr_l(n, sizeof(req), TCA_KIND, k, strlen(k)+1);
    tail = NLMSG_TAIL(n);

    index = (index + 1) % nbws;
    opt.latency = tc_core_time2tick(bws[index][LAT]);

    rate64 = bws[index][BW] >> 3;
    if (rate64 >= (1ULL << 32)) {
      addattr_l(n, 1024, TCA_NETEM_RATE64, &rate64, sizeof(rate64));
      rate.rate = ~0U;
    } else {
      rate.rate = rate64;
    }
    addattr_l(n, 1024, TCA_NETEM_RATE, &rate, sizeof(rate));

    tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;

    // if (tc_calc_rtable(&opt.rate, rtab, Rcell_log, mtu, linklayer) < 0) {
    //   fprintf(stderr, "tbf: failed to calculate rate table.\n");
    //   ret = -1;
    //   goto exit;
    // }

    // opt.buffer = tc_calc_xmittime(opt.rate.rate, buffer);

    // tail = addattr_nest(n, 1024, TCA_OPTIONS);
    // addattr_l(n, 2024, TCA_TBF_PARMS, &opt, sizeof(opt));
    // addattr_l(n, 2124, TCA_TBF_BURST, &buffer, sizeof(buffer));
    // if (rate64 >= (1ULL << 32))
    //   addattr_l(n, 2124, TCA_TBF_RATE64, &rate64, sizeof(rate64));
    // addattr_l(n, 3024, TCA_TBF_RTAB, rtab, 1024);
    // addattr_nest_end(n, tail);

    if (rtnl_talk(&rth, &req.n, NULL) < 0) {
      ret = 2;
      goto exit;
    }

    req.t.tcm_ifindex = idx2;

    if (rtnl_talk(&rth, &req.n, NULL) < 0) {
      ret = 2;
      goto exit;
    }

#ifdef PRINT_TIME
    clock_gettime(CLOCK_REALTIME, &tsf);
    printf("Time: %ld\n", tsf.tv_nsec - tss.tv_nsec);
#endif

    nanosleep(&(bws[index][PER]), &outtime);

  }

  // end q_tbf.c

exit: 
  rtnl_close(&rth);
  return ret;
}
