#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_tun.h>
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
/**************************************************************************
 * tun_alloc: allocates or reconnects to a tun/tap device. The caller     *
 *            must reserve enough space in *dev.                          *
 **************************************************************************/
int tun_alloc(char *dev, int flags) {

  struct ifreq ifr;
  int fd, err;
  char *clonedev = "/dev/net/tun";

  if( (fd = open(clonedev , O_RDWR)) < 0 ) {
    perror("Opening /dev/net/tun");
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = flags;

  if (*dev) {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  if( (err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
    perror("ioctl(TUNSETIFF)");
    close(fd);
    return err;
  }

  strcpy(dev, ifr.ifr_name);

  return fd;
}

int main(int argc, char * argv[]) {
  char tun_name[IFNAMSIZ];
  char buffer[1500];

  /* Connect to the device */
  strcpy(tun_name, "tun0");
  int tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);  /* tun interface */

  if(tun_fd < 0){
    perror("Allocating interface");
    exit(1);
  }

  /* Now read data coming from the kernel */
  while(1) {
    /* Note that "buffer" should be at least the MTU size of the interface, eg 1500 bytes */
    int nread = read(tun_fd,buffer,sizeof(buffer));
    if(nread < 0) {
      perror("Reading from interface");
      close(tun_fd);
      exit(1);
    }

    /* Do whatever with the data */
    printf("Read %d bytes from device %s\n", nread, tun_name);
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
  }
}
