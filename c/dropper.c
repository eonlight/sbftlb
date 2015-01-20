#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>        /* for NF_ACCEPT */

#include<netinet/ip.h>	//Provides declarations for ip header
#include<netinet/tcp.h>	//Provides declarations for tcp header

#include<stdio.h>
#include<string.h> //memset
#include<arpa/inet.h>

#include <libnetfilter_queue/libnetfilter_queue.h>

#define ME 1
#define NUMLB 2

static int select_lb(char *host, int port){
	int hash = 0, i = 0;
	while(host[i] != '\0')
		hash+=host[i++];
	hash+=port;
	return hash%NUMLB;
}

static int cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg,
	struct nfq_data *nfa, void *data) {
	
	char *buffer;
	nfq_get_payload(nfa, &buffer);
	
	struct iphdr *iph = (struct iphdr*) buffer;
	struct sockaddr_in source;	
	memset(&source, 0, sizeof(source));
	source.sin_addr.s_addr = iph->saddr;
	
	/* Get source */
	char *host = inet_ntoa(source.sin_addr);
			
	/*create struct for tcp header */
	unsigned short iphdrlen = iph->ihl*4;
	struct tcphdr *tcph = (struct tcphdr *) (buffer + iphdrlen);

	/* Get sport */
	int sport = ntohs(tcph->source);

	int lb = select_lb(host, sport);

	int id = 0;
	struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfa);
	id = ntohl(ph->packet_id);
	
	if(lb == ME){
		//printf("ME\n");
		return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
	}else
		return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
}

int main(int argc, char **argv){
      struct nfq_handle *h;
      struct nfq_q_handle *qh;

      int fd, rv;
      char buf[4096] __attribute__ ((aligned));

      printf("opening library handle\n");
      h = nfq_open();
      if (!h) {
            fprintf(stderr, "error during nfq_open()\n");
            exit(1);
      }

      printf("unbinding existing nf_queue handler for AF_INET (if any)\n");
      if (nfq_unbind_pf(h, AF_INET) < 0) {
            fprintf(stderr, "error during nfq_unbind_pf()\n");
            exit(1);
      }

      printf("binding nfnetlink_queue as nf_queue handler for AF_INET\n");
      if (nfq_bind_pf(h, AF_INET) < 0) {
            fprintf(stderr, "error during nfq_bind_pf()\n");
            exit(1);
      }

      printf("binding this socket to queue '1'\n");
      qh = nfq_create_queue(h,  1, &cb, NULL);
      if (!qh) {
            fprintf(stderr, "error during nfq_create_queue()\n");
            exit(1);
      }

      printf("setting copy_packet mode\n");
      if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
            fprintf(stderr, "can't set packet_copy mode\n");
            exit(1);
      }

      fd = nfq_fd(h);

      while ((rv = recv(fd, buf, sizeof(buf), 0)) && rv >= 0)
            nfq_handle_packet(h, buf, rv);

      printf("unbinding from queue 1\n");
      nfq_destroy_queue(qh);

#ifdef INSANE
      /* normally, applications SHOULD NOT issue this command, since
       * it detaches other programs/sockets from AF_INET, too ! */
      printf("unbinding from AF_INET\n");
      nfq_unbind_pf(h, AF_INET);
#endif
      printf("closing library handle\n");
      nfq_close(h);

      exit(0);
}

