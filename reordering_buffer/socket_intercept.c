#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h> 
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h>
#include <linux/ip.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>

int main(int argc, char* argv[]) {

  int in_sfd, out_sfd;
  unsigned char *buffer = (unsigned char *) malloc(65536); 
  struct sockaddr saddr;
  int saddr_len = sizeof (saddr);
  int rlen;

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


  memset(buffer,0,65536);

  rlen = recvfrom(in_sfd, buffer, 65536, 0, &saddr, (socklen_t *)&saddr_len);

  if (rlen < 0) {
    fprintf(stderr, "error in reading recvfrom function\n");
    return -1;
  }

  fprintf(stderr, "Received %d bytes from %s\n", rlen, "enp1s0");

  struct sockaddr_in source, dest;

  unsigned short iphdrlen;
  struct iphdr *ip = (struct iphdr*)(buffer);
  memset(&source, 0, sizeof(source));
  source.sin_addr.s_addr = ip->saddr;
  memset(&dest, 0, sizeof(dest));
  dest.sin_addr.s_addr = ip->daddr;

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

  close(in_sfd);
  return 0;

}

