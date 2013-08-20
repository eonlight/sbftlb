#include <stdlib.h> //exit()
#include <stdio.h>

#include <signal.h> //SIGINT
#include <string.h> //sprintf

#include <netinet/ip.h>	//Provides declarations for ip header
#include <netinet/tcp.h> // Provides declarations for tcp header
#include <netinet/udp.h> // Provides declarations for udp header

#include <arpa/inet.h> // sockets / inet_*

#include <pthread.h> //threads

#include <pcap.h>

int mainID = -1;

#include "bagger.h"
#include "algorithm.c"

void die(char *error){

	if(mainID == (int) pthread_self()){
		printf("\nError: %s\n", error);
		pcap_freecode(&fp);
		pcap_close(handle);
		cleanAlgorithm();
	}
	pthread_exit(0);
	exit(0);
}

void handlePacket(u_char *args, const struct pcap_pkthdr *header, const u_char *packet) {
	unsigned char *buffer = (unsigned char *)(packet + SIZE_ETHERNET);

	// create struct for ip header
	struct iphdr *iph = (struct iphdr*) buffer;
	unsigned short iphdrlen = iph->ihl*4;

	if(iph->protocol == UDP_PROTO)
		// ip header, udp header, payload
		handleUPDPacket(iph, (struct udphdr *)(buffer + iphdrlen), (char *) buffer + iphdrlen + sizeof(struct udphdr));
	else if(iph->protocol == TCP_PROTO)
		handleTCPPacket(iph, (struct tcphdr *) (buffer + iphdrlen));
	else
		handleOtherProto(iph, (char *) buffer + iphdrlen);

}

int main(int argc, char **argv){

	signal(SIGINT, terminate);
	signal(SIGTERM, terminate);
	
	mainID = (int) pthread_self();

	initAlgorithm();

	char errbuf[PCAP_ERRBUF_SIZE];
	
	bpf_u_int32 mask; //subnet mask
	bpf_u_int32 net; //ip
	int num_packets = -1; // until error

	// get network number and mask associated with capture device
	printf("init pcap protocol...\n");
	if (pcap_lookupnet(DEV, &net, &mask, errbuf) == -1) {
		fprintf(stderr, "Couldn't get netmask for device %s: %s\n", DEV, errbuf);
		net = 0;
		mask = 0;
	}

	handle = pcap_open_live(DEV, SNAP_LEN, 1, 1000, errbuf);
	if (handle == NULL)
		die("bagger - main: fail to open pcap");

	// make sure we're capturing on an Ethernet device
	if (pcap_datalink(handle) != DLT_EN10MB)
		die("bagger - main: fail to set interface");

	//compile the filter expression
	if (pcap_compile(handle, &fp, filter, 0, net) == -1)
		die("bagger - main: couldn't parse filter");

	//apply the compiled filter
	if (pcap_setfilter(handle, &fp) == -1)
		die("bagger - main: couldn't install filter");

	printf("main: Starting to capture...\n");
	//now we can set our callback function
	pcap_loop(handle, num_packets, handlePacket, NULL);

	return 0;
}

void terminate(int sig){
	die(strsignal(sig));
}