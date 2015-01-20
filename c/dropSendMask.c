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

#define PORT 5555

#define FRONTEND "10.10.5.100"
#define FRONTPORT 5555

#define ME 0
#define NUMLB 1

/* LBS */
const char* LBS[] = {"10.10.5.116", "10.10.5.201"};

/* socket */
int s, err, on = 1;

static unsigned short compute_checksum(unsigned short *addr, unsigned int count);
void compute_ip_checksum(struct iphdr* iphdrp);
void compute_tcp_checksum(struct iphdr *pIph, unsigned short *ipPayload);

void die(char *s) {
	perror(s);
	exit(1);
}

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

	/*create struct for ip header */	
	struct iphdr *iph = (struct iphdr*) buffer;
	
	/*create struct for tcp header */
	unsigned short iphdrlen = iph->ihl*4;
	struct tcphdr *tcph = (struct tcphdr *) (buffer + iphdrlen);

	int id = 0;

	if(ntohs(tcph->dest) == PORT) {

		struct sockaddr_in source, to;	
		memset(&source, 0, sizeof(source));
		source.sin_addr.s_addr = iph->saddr;
		to.sin_family = AF_INET;

		/* Get source */
		char *host = inet_ntoa(source.sin_addr);

		/* Get sport */
		int sport = ntohs(tcph->source);

		int lb = select_lb(host, sport);

		struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfa);
		id = ntohl(ph->packet_id);
	
		if(lb == ME) {
			/* Change dport */
			tcph->dest = htons(PORT);

			/* Change dest */
			iph->daddr = inet_addr(LBS[lb]);

			/* Compute TCP checksum */
			compute_tcp_checksum(iph, (unsigned short*)tcph);

			/* Create addr to dest */
			to.sin_addr.s_addr = iph->daddr;
	    	to.sin_port = tcph->dest;

			if ((err = sendto(s,buffer,ntohs(iph->tot_len),0,(struct sockaddr *)&to, sizeof(to))) < 0)
				die("send");
		}

	}
	else if(ntohs(tcph->source) == PORT){
		/* Change sport */
		tcph->source = htons(FRONTPORT);
		/* Change source */
		iph->saddr = inet_addr(FRONTEND);

		compute_tcp_checksum(iph, (unsigned short*)tcph);
		compute_ip_checksum(iph);

		struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfa);
		id = ntohl(ph->packet_id);
	
		return nfq_set_verdict(qh, id, NF_ACCEPT, ntohs(iph->tot_len), (unsigned char *)buffer);
	}
	
	return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
}
	
int main(int argc, char **argv){
	struct nfq_handle *h;
	struct nfq_q_handle *qh;

	int fd, rv;
	char buf[4096] __attribute__ ((aligned));

	/* create a socket */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_TCP)) == -1)
		die("socket");

	/* set socket options to send packets */
	if(setsockopt(s, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) == -1)
        	die("setsockopt IP");

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

	close(s);

	exit(0);
}

/* Compute checksum for count bytes starting at addr, using one's complement of one's complement sum*/
static unsigned short compute_checksum(unsigned short *addr, unsigned int count) {
	register unsigned long sum = 0;
	while (count > 1) {
		sum += * addr++;
		count -= 2;
	}

	//if any bytes left, pad the bytes and add
	if(count > 0)
		sum += ((*addr)&htons(0xFF00));

	//Fold sum to 16 bits: add carrier to result
	while (sum>>16)
  		sum = (sum & 0xffff) + (sum >> 16);

  	//one's complement
  	sum = ~sum;
	return ((unsigned short)sum);
}

/* set ip checksum of a given ip header*/
void compute_ip_checksum(struct iphdr* iphdrp){
  iphdrp->check = 0;
  iphdrp->check = compute_checksum((unsigned short*)iphdrp, iphdrp->ihl<<2);
}

/* set tcp checksum: given IP header and tcp segment */
void compute_tcp_checksum(struct iphdr *pIph, unsigned short *ipPayload) {
	register unsigned long sum = 0;
	unsigned short tcpLen = ntohs(pIph->tot_len) - (pIph->ihl<<2);
	struct tcphdr *tcphdrp = (struct tcphdr*)(ipPayload);
	
	//add the pseudo header 
	//the source ip
	sum += (pIph->saddr>>16)&0xFFFF;
	sum += (pIph->saddr)&0xFFFF;

	//the dest ip
	sum += (pIph->daddr>>16)&0xFFFF;
	sum += (pIph->daddr)&0xFFFF;

	//protocol and reserved: 6
	sum += htons(IPPROTO_TCP);
	//the length
	sum += htons(tcpLen);

	//add the IP payload
	//initialize checksum to 0
	tcphdrp->check = 0;
	while (tcpLen > 1) {
		sum += * ipPayload++;
		tcpLen -= 2;
	}

	//if any bytes left, pad the bytes and add
	if(tcpLen > 0) {
		//printf("+++++++++++padding, %d\n", tcpLen);
        	sum += ((*ipPayload)&htons(0xFF00));
    	}
  	
	//Fold 32-bit sum to 16 bits: add carrier to result
  	while (sum>>16) {
  		sum = (sum & 0xffff) + (sum >> 16);
  	}
  	
	sum = ~sum;
	//set computation result
	tcphdrp->check = (unsigned short)sum;
}

