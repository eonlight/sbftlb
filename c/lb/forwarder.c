#include <stdlib.h> //exit()
#include <stdio.h>

#include <signal.h> //SIGINT
#include <string.h> //sprintf

#include <netinet/in.h> //required by netfilter.h
#include <linux/netfilter.h> // for NF_RULES
#include <libnetfilter_queue/libnetfilter_queue.h> //NFQUEUE

#include <netinet/ip.h>	//Provides declarations for ip header
#include <netinet/tcp.h> // Provides declarations for tcp header
#include <netinet/udp.h> // Provides declarations for udp header

#include <pthread.h> //threads

int mainID = -1;

#include "forwarder.h"
#include "algorithm.c"

void die(char *error){

	if(mainID == (int) pthread_self()){
		printf("\nError: %s\n", error);
		cleanNFQueue();
		cleanAlgorithm();
	}
	pthread_exit(0);
	exit(0);
}

int handlePacket(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data) {

	unsigned char *buffer;
	nfq_get_payload(nfa, (char **) &buffer);

	// Get package queue id
	struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfa);
	int id = ntohl(ph->packet_id);

	// create struct for ip header
	struct iphdr *iph = (struct iphdr*) buffer;
	unsigned short iphdrlen = iph->ihl*4;
	int rule = NF_DROP;

	if(iph->protocol == UDP_PROTO)
		// ip header, udp header, payload
		rule = handleUPDPacket(iph, (struct udphdr *)(buffer + iphdrlen), (char *) buffer + iphdrlen + sizeof(struct udphdr));
	else if(iph->protocol == TCP_PROTO)
		rule = handleTCPPacket(iph, (struct tcphdr *) (buffer + iphdrlen));
	else
		rule = handleOtherProto(iph, (char *) buffer + iphdrlen);

	return nfq_set_verdict(qh, id, rule, 0, NULL);
}

int main(int argc, char **argv){

	signal(SIGINT, terminate);
	signal(SIGTERM, terminate);
	
	mainID = (int) pthread_self();

	initAlgorithm();

	int fd, rv;
	char buf[4096] __attribute__ ((aligned));

	printf("main: Setting up nfqueue...\n");
	// opening library handle
	h = nfq_open();
	if(!h)
		die("forwarder - main: error during nfq_open()");

	// binding nfnetlink_queue as nf_queue handler for AF_INET
	if(nfq_bind_pf(h, AF_INET) < 0)
		die("forwarder - main: error during nfq_bind_pf()");
		
	// binding this socket to queue '1'
	qh = nfq_create_queue(h,  1, &handlePacket, NULL);
	if(!qh)
		die("forwarder - main: error during nfq_create_queue()");

	// setting copy_packet mode
	if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0)
		die("forwarder - main: can't set packet_copy mode");

	fd = nfq_fd(h);

	printf("main: All set!\n");
	while(1){
		rv = recv(fd, buf, sizeof(buf), 0);
		nfq_handle_packet(h, buf, rv);
	}

	return 0;
}

void terminate(int sig){
	die(strsignal(sig));
}

void cleanNFQueue(){
	printf("cleanNFQueue: Cleaning NFQueue...\n");
	if(qh != NULL)
		nfq_destroy_queue(qh);	
	qh = NULL;

	if(h != NULL){
		nfq_unbind_pf(h, AF_INET);
		nfq_close(h);
	}
	h = NULL;
}