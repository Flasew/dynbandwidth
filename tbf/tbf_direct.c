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

#define DT_NS 0
#define DT_S 0

#define PRINT_TIME

struct rtnl_handle rth;

int main(int argc, char * argv[]) {

  int pid = getpid();
  struct sched_param param;
  memset(&param, 0, sizeof(param));
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  sched_setscheduler(pid, SCHED_FIFO, &param);

  __u64 rate64 = HBW;
  __u64 bws[] = {HBW, LBW};
  unsigned int index = 1;

  __u64 dt_s = DT_S;
  __u64 dt_ns = DT_NS;
  __u64 lat_ms = 0;

  int ret = 0;
  int ignore;

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

  if (argc == 2) {
    dt_s = 0;
    dt_ns = 1000 * strtoull(argv[1], NULL, 10);
  }
  else if (argc == 3) {
    dt_s = 0;
    dt_ns = 1000 * strtoull(argv[1], NULL, 10);
    lat_ms = strtoull(argv[2], NULL, 10);
  }
  else if (argc == 4) {
    dt_s = strtoull(argv[1], NULL, 10);
    dt_ns = strtoull(argv[2], NULL, 10);
    lat_ms = strtoull(argv[3], NULL, 10);
  }
  else if (argc != 1) {
    fprintf(stderr, "Wrong number of arguments. \n");
    exit(1);
  }

#ifdef PRINT_TIME
  struct timespec tss, tsf;
#endif
  struct timespec intime = {dt_s, dt_ns};
  struct timespec outtime;

  ignore = system("/sbin/tc qdisc del dev enp1s0 root");

  if (lat_ms != 0) {
    char buf[1024] = {};
    snprintf(buf, 1024, "/sbin/tc qdisc add dev enp1s0 root handle 1:0 netem delay %llums", lat_ms);
    ignore = system(buf);
    ignore = system("tc qdisc add dev enp1s0 parent 1:1 handle 10: tbf rate 5.5Gbit burst 48750b lat 66us");
    req.t.tcm_parent = 0x00010001U;
    req.t.tcm_handle = 0x00100000U;
  }
  else {
    ignore = system("tc qdisc add dev enp1s0 root tbf rate 5.5Gbit burst 48750b lat 66us");
    req.t.tcm_parent = TC_H_ROOT;
  }

  tc_core_init();

  if (rtnl_open(&rth, 0) < 0) {
    fprintf(stderr, "Cannot open rtnetlink\n");
    exit(1);
  }

  // tc_qdisc.c

  if (d[0])  {
    int idx;

    ll_init_map(&rth);

    idx = ll_name_to_index(d);
    if (!idx)
      return -nodev(d);
    req.t.tcm_ifindex = idx;
  }

  // q_tbf.c
  struct nlmsghdr * n = &req.n;

  struct tc_tbf_qopt opt = {};
  __u32 rtab[256];
  __u32 ptab[256];
  unsigned buffer = (50000) , mtu = 0, latency = 0;
  int Rcell_log =  -1, Pcell_log = -1;
  unsigned int linklayer = LINKLAYER_ETHERNET; /* Assume ethernet */
  struct rtattr *tail;
  __u64 prate64 = 0;
  opt.limit = 1514 * 60;
  opt.peakrate.rate = 0;

  opt.rate.mpu      = 0;
  opt.rate.overhead = 0;
  
  while (1) {
    
#ifdef PRINT_TIME
    clock_gettime(CLOCK_REALTIME, &tss);
#endif

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
    addattr_l(n, sizeof(req), TCA_KIND, k, strlen(k)+1);

    rate64 = bws[(index^=1)] >> 3;
    opt.rate.rate = (rate64 >= (1ULL << 32)) ? ~0U : rate64;

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
    addattr_nest_end(n, tail);

    if (rtnl_talk(&rth, &req.n, NULL) < 0) {
      ret = 2;
      goto exit;
    }

#ifdef PRINT_TIME
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
