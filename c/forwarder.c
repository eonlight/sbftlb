#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h> //sigterm
#include <arpa/inet.h>
#include <netinet/tcp.h> //Provides declarations for tcp header
#include <netinet/ip.h>	//Provides declarations for ip header
#include <sys/socket.h>

#define BUFLEN 65535 	//Max IP datagram length
#define LPORT 5555	//The port on which to listen for incoming data
#define TOPORT 5555
#define LBNUMBER 1

//vmhost, andre
const char* LBS[] = {"10.10.5.153", "10.10.5.172"};

int end = 0;

void term(int signum) {
    printf("Exiting...\n");
    end = 1;
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


void die(char *s) {
	perror(s);
	exit(1);
}

void printPacket(struct iphdr *iph, struct tcphdr *tcph){
	struct sockaddr_in source, dest;
	
	memset(&source, 0, sizeof(source));
	source.sin_addr.s_addr = iph->saddr;
	
	memset(&dest, 0, sizeof(dest));
	dest.sin_addr.s_addr = iph->daddr;

	printf("\n*********************** Packet *************************\n");	
	
	/* print IP fields */
	printf("IP Total Length   : %d  Bytes(Size of Packet)\n",ntohs(iph->tot_len));
	printf("Source IP        : %s\n",inet_ntoa(source.sin_addr));
	printf("Destination IP   : %s\n",inet_ntoa(dest.sin_addr));

	/* print TCP fields */
	printf("Source Port      : %u\n",ntohs(tcph->source));
	printf("Destination Port : %u\n",ntohs(tcph->dest));
	
	/* print flags */
	printf("Flags:\n");
	printf("   |-Urgent Flag          : %d\n",(unsigned int)tcph->urg);
	printf("   |-Acknowledgement Flag : %d\n",(unsigned int)tcph->ack);
	printf("   |-Push Flag            : %d\n",(unsigned int)tcph->psh);
	printf("   |-Reset Flag           : %d\n",(unsigned int)tcph->rst);
	printf("   |-Synchronise Flag     : %d\n",(unsigned int)tcph->syn);
	printf("   |-Finish Flag          : %d\n",(unsigned int)tcph->fin);	
}

int main(void) {
	signal(SIGTERM, term);
 	signal(SIGHUP, term);
	signal(SIGINT, term);
	signal(SIGKILL, term);

	struct sockaddr_in si_other, to;
    	to.sin_family = AF_INET;

	int s, recv_len, i, err, on = 1;
	unsigned int slen = sizeof(si_other);

	/* create a buffer to receive the requests */
	char * buffer;
	if((buffer = (char *) malloc((BUFLEN+1)*sizeof(char))) == NULL)
		die("buffer");
	
	/* create a socket */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_TCP)) == -1)
		die("socket");

	/* set sock options to send packets */
	if(setsockopt(s, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) == -1)
        	die("setsockopt");

	/* keep listening for data */
	while(!end) {
		
		/*try to receive some data*/
		if ((recv_len = recvfrom(s, buffer, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
			die("recvfrom()");

		/*create struct for ip header */
		struct iphdr *iph = (struct iphdr*) buffer;

		/* se o packet n for de saida */
		unsigned short iphdrlen = iph->ihl*4;
			
		/*create struct for tcp header */
		struct tcphdr *tcph = (struct tcphdr *) (buffer + iphdrlen);

		/* se for o port a escutar */
		if(ntohs(tcph->dest) == LPORT){

			/* Erase checksums */
			iph->check = 0;
			tcph->check = 0;

			/* Change dport */
			tcph->dest = htons(TOPORT);

			for(i = 0; i < LBNUMBER; i++){

				/* Change dest */
				iph->daddr = inet_addr(LBS[i]);

				/* Compute TCP checksum */
				compute_tcp_checksum(iph, (unsigned short*)tcph);

				/* Create addr to dest */
				to.sin_addr.s_addr = iph->daddr;
    				to.sin_port = tcph->dest;

				if ((err = sendto(s,buffer,ntohs(iph->tot_len),0,(struct sockaddr *)&to, sizeof(to))) < 0)
					die("send");
			}
			
		}

	}

	free(buffer);
	close(s);
	return 0;
}
