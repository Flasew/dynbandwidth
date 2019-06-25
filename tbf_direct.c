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

#include "utils.h"
#include "tc_core.h"
#include "tc_util.h"
#include "tc_common.h"

struct rntl_handle rth;

int main(int argc, char * argv[]) {

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
    .n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg)),
    .n.nlmsg_flags = NLM_F_REQUEST,
    .n.nlmsg_type = RTM_NEWQDISC,
    .t.tcm_family = AF_UNSPEC,
  };

  req.t.tcm_parent = TC_H_ROOT;
  addattr_l(&req.n, sizeof(req), TCA_KIND, k, strlen(k)+1);

  // q_tbf.c
  struct nlmsghdr * n = &req.n;

  int ok = 0;
  struct tc_tbf_qopt opt = {};
  __u32 rtab[256];
  __u32 ptab[256];
  unsigned buffer = 0, mtu = 0, mpu = 0, latency = 0;
  int Rcell_log =  -1, Pcell_log = -1;
  unsigned short overhead = 0;
  unsigned int linklayer = LINKLAYER_ETHERNET; /* Assume ethernet */
  struct rtattr *tail;
  __u64 prate64 = 0;

  __u64 rate64 = (10000000000ULL >> 3);
  opt.limit = 1500 * 60;
  buffer = (10000000UL >> 3);

  opt.rate.rate = (rate64 >= (1ULL << 32)) ? ~0U : rate64;
  opt.peakrate.rate = (prate64 >= (1ULL << 32)) ? ~0U : prate64;

  opt.rate.mpu      = mpu;
  opt.rate.overhead = overhead;
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
    if (prate64 >= (1ULL << 32))
      addattr_l(n, 3124, TCA_TBF_PRATE64, &prate64, sizeof(prate64));
    addattr_l(n, 3224, TCA_TBF_PBURST, &mtu, sizeof(mtu));
    addattr_l(n, 4096, TCA_TBF_PTAB, ptab, 1024);
  }
  addattr_nest_end(n, tail);

  // end q_tbf.c
  
  if (d[0])  {
    int idx;

    ll_init_map(&rth);

    idx = ll_name_to_index(d);
    if (!idx)
      return -nodev(d);
    req.t.tcm_ifindex = idx;
  }

  if (rtnl_talk(&rth, &req.n, NULL) < 0) {
    ret = 2;
    goto exit;
  }

  rtnl_close(&rth);

exit: 
  return ret;
}
