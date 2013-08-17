#include <stdio.h>
#include <stdlib.h>

#define SIZE_ETHERNET 14
#define SNAP_LEN 1512

#include <pcap.h>

pcap_t * handle = NULL;	
struct bpf_program fp; //compiled filter

void got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet) {
	unsigned char * buffer = (unsigned char *)(packet + SIZE_ETHERNET);
	//cb(buffer, header->caplen);
	return;
}

void die(char * s){
	exit(0);
}

int main(int argc, char **argv){

	// TODO: change to CONFIG
	char *dev = "eth1";
	char errbuf[PCAP_ERRBUF_SIZE];

	// TODO: change to configs
	char filter_exp[] = "tcp port 8080 and dst 192.168.7.233";// or udp port 33000";

	bpf_u_int32 mask; //subnet mask
	bpf_u_int32 net; //ip
	int num_packets = -1; // until error

	// get network number and mask associated with capture device
	printf("init pcap protocol...\n");
	if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {
		fprintf(stderr, "Couldn't get netmask for device %s: %s\n", dev, errbuf);
		net = 0;
		mask = 0;
	}

	handle = pcap_open_live(dev, SNAP_LEN, 1, 1000, errbuf);
	if (handle == NULL)
		die("forwarder - main: fail to open pcap");

	// make sure we're capturing on an Ethernet device
	if (pcap_datalink(handle) != DLT_EN10MB)
		die("forwarder - main: fail to set interface");

	//compile the filter expression
	if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1)
		die("forwarder - main: couldn't parse filter");

	//apply the compiled filter
	if (pcap_setfilter(handle, &fp) == -1)
		die("forwarder - main: couldn't install filter");

	printf("starting to capture...\n");
	//now we can set our callback function
	pcap_loop(handle, num_packets, got_packet, NULL);

	return 0;
}
