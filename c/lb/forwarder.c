#include <stdio.h>
#include <stdlib.h>

#include <signal.h> // Signals
#include <pthread.h> // Threads

#include <string.h>

int count[] = {0,0,0};
int counter = 0;

#include "hashs.c"
#include "httpreqlist.c"
#include "forwarder.h"

#include "packets.c"
#include "config.c"
#include "thread.c"


int needRestart() {
	int i, total = 0;
	for(i = 0; i < state.numLB; i++)
		if(state.restart[i] > 0)
			total++;
	
	return total >= fconfig.faults+1;
}

void addNewServer(int id, char *ip){
	Server * newServers = (Server *) malloc(sizeof(Server)*(state.numServers+1));
	memcpy(newServers, state.servers, sizeof(Server)*state.numServers);
	Server * old = state.servers;
	newServers[state.numServers].ip = (char *) malloc(sizeof(char)*(strlen(ip)+1));
	memcpy(newServers[state.numServers].ip, ip, strlen(ip)*sizeof(char));
	newServers[state.numServers].ip[strlen(ip)] = '\0';
	newServers[state.numServers].id = id;
	state.servers = newServers;

	free(old);

	state.numServers++;

}

void addNewLB(int id, char *ip){
	int i;

	LoadBalancer *newLBS = (LoadBalancer *) malloc(sizeof(LoadBalancer)*(state.numLB+1));
	memcpy(newLBS, state.lbs, sizeof(LoadBalancer)*state.numLB);
	LoadBalancer * oldLBS = state.lbs;
	state.lbs = newLBS;

	HttpRequestNode ** newList = (HttpRequestNode **) malloc(sizeof(HttpRequestNode *)*(state.numLB+1));
	for(i = 0; i < state.numLB; i++)
		newList[i] = state.list[i];
	
	newList[i] = NULL;

	free(oldLBS);

	state.lbs[i].ip = (char *) malloc(sizeof(char)*(strlen(ip)+1));
	memcpy(state.lbs[i].ip, ip, strlen(ip)*sizeof(char));
	state.lbs[i].ip[strlen(ip)] = '\0';
	state.lbs[i].id = id;

	HttpRequestNode ** oldList = state.list;
	state.list = newList;
	free(oldList);

	int * newSusp, * newAsusp, * newFaulty, * newRestart;

	newSusp = (int *) calloc(sizeof(int), state.numLB+1);
	memcpy(newSusp, state.susp, state.numLB*sizeof(int));
	int * old = state.susp;
	state.susp = newSusp;
	free(old);

	newAsusp = (int *) calloc(sizeof(int), state.numLB+1);
	memcpy(newAsusp, state.asusp, state.numLB*sizeof(int));
	old = state.asusp;
	state.asusp = newAsusp;
	free(old);

	newFaulty = (int *) calloc(sizeof(int), state.numLB+1);
	memcpy(newFaulty, state.faulty, state.numLB*sizeof(int));
	old = state.faulty;
	state.faulty = newFaulty;
	free(old);

	newRestart = (int *) calloc(sizeof(int), state.numLB+1);
	memcpy(newRestart, state.restart, state.numLB*sizeof(int));
	old = state.restart;
	state.restart = newRestart;
	free(old);

	state.numLB++;
}

int cb(unsigned char * buffer, int recv_len) {

	//unsigned char *buffer;
	// buffer[recv_len] == '\0'
	/*int recv_len = nfq_get_payload(nfa, (char **) &buffer);

	// Get package queue id
	struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfa);
	int id = ntohl(ph->packet_id);*/

	// create struct for ip header
	struct iphdr *iph = (struct iphdr*) buffer;
	unsigned short iphdrlen = iph->ihl*4;
	
	if(iph->protocol == UDP_PROTO){
    	struct udphdr *udph = (struct udphdr*)(buffer + iphdrlen);
    	char * pl = (char *) buffer + iphdrlen + sizeof(udph);
    	
    	int type;
    	memcpy(&type, pl, sizeof(int));
    	printf("type: %d,", type);
    	int lbid;
		memcpy(&lbid, pl+sizeof(int), sizeof(int));
		printf("id: %d\n", lbid);
    	
    	if(type == 1){
			if(lbid >= 0 && lbid < state.numLB){
				state.faulty[lbid] = 0;
				state.susp[lbid] = 0;
				state.asusp[lbid] = 0;

				if(state.list[lbid] != NULL)
					cleanList(state.list[lbid]);
				state.list[lbid] = NULL;
			}
			else if(lbid == state.numLB){
				struct sockaddr_in source;
				memset(&source, 0, sizeof(source));
    			source.sin_addr.s_addr = iph->saddr;
    			printf("Adding new LB\n");
				//Add new LB to the list
				addNewLB(lbid, inet_ntoa(source.sin_addr));
			}
		} 
		else if(type == 3) {
			int serverID = lbid;
			if(serverID == state.numServers){
				struct sockaddr_in source;
				memset(&source, 0, sizeof(source));
    			source.sin_addr.s_addr = iph->saddr;
    			printf("Adding new Server\n");
				//Add new LB to the list
				addNewServer(serverID, inet_ntoa(source.sin_addr));
			}
			
		}
		else if(type == 2 && lbid == config.id){
		
			int sid;
			memcpy(&sid, pl+sizeof(int)*2, sizeof(int));
			printf("from id: %d\n", sid);
			state.restart[sid]++;
		
			if(needRestart()){
				printf("recovering...\n");
	
				resetState();
				
				printf("sending...\n");
				int rs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

				struct sockaddr_in all;
				all.sin_family = AF_INET;
				all.sin_addr.s_addr = inet_addr(config.frontEnd);
				all.sin_port = htons(config.helloPort);
		
				int type = 1;
				char hello[sizeof(int)*2+1];
				memcpy(hello, &type, sizeof(int));
				memcpy(hello+sizeof(int), &config.id, sizeof(int));
				hello[sizeof(int)*2] = '\0';
		
				sendto(rs, (const char*) hello, sizeof(int)*2, 0, (struct sockaddr *)&all, sizeof(struct sockaddr_in));

				close(rs);
			}
			
		}
    	return 0;
		//return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
	}
	
	// create struct for tcp header
	struct tcphdr *tcph = (struct tcphdr *) (buffer + iphdrlen);

	struct sockaddr_in source;	
	memset(&source, 0, sizeof(source));
	source.sin_addr.s_addr = iph->saddr;

	// Get source
	char *host = inet_ntoa(source.sin_addr);

	// Get sport
	int sport = ntohs(tcph->source);

	// Select wish load balancer will responde
	int lb = selectLB(host, sport, state.numLB);

	while(isFaulty(lb)) 
		lb = (lb+1)%state.numLB;

	// Alterar para algoritmo de escolha de server (eg. rr)
	int server = getDestination(host, sport, state.numServers);
	
	// Change dport
	tcph->dest = htons(config.endPort);
			
	// Change dest
	iph->daddr = inet_addr(state.servers[server].ip);

	// Compute TCP checksum
	compute_tcp_checksum(iph, (unsigned short*)tcph);
	compute_ip_checksum(iph);
	
	if(amWhatcher(lb, state.numLB))
		addToList(lb, recv_len, buffer, server);
	
	if(lb == config.id)
		sendPacket(iph, tcph, buffer, config.id);

	// Default: drop packets
	//return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
	return 1;
}

/*
	communication between lbs
	TYPE:ID:INFO...
	TYPE 1 = Hello Recover
	TYPE 2 = Request Recover
*/

int isWhatcher(int lb, int whatcher, int num){
	int i;
	for(i = (lb+1)%num; i != (lb+(2*fconfig.faults))%num ; i=(i+1)%num)
		if(whatcher == i)
			return 1;
	
	if(whatcher == i)		
		return 1;
		
	return 0;
	//return (lb+1)%num <= whatcher || whatcher <= (lb+(2*fconfig.faults))%num;
}

void got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet) {
	unsigned char * buffer = (unsigned char *)(packet + SIZE_ETHERNET);
	cb(buffer, header->caplen);
}

int main(int argc, char **argv){

	printf("starting...\n");

	if(argc < 2){
		printf("Usage: %s [config file] {1}\n", argv[0]);
		die("forwarder - main: no config file");
	}
	
	// init mutex 
	if (pthread_mutex_init(&lock, NULL) != 0)
        die("forwarder - main: mutex init failed");

	signal(SIGINT, terminate);
	signal(SIGTERM, terminate);
	signal(SIGKILL, terminate);

	//Configs
	readConfig(argv[1]);
	addIptablesRules();
	
	// Send Hello to all LBs	
	printf("sending hello protocol...\n");
	int rs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	struct sockaddr_in all;
	all.sin_family = AF_INET;
	all.sin_addr.s_addr = inet_addr(config.frontEnd);
	all.sin_port = htons(config.helloPort);

	int type = 1;
	char hello[sizeof(int)*2+1];
	memcpy(hello, &type, sizeof(int));
	memcpy(hello+sizeof(int), &config.id, sizeof(int));
	hello[sizeof(int)*2] = '\0';

	sendto(rs, (const char*) hello, sizeof(int)*2, 0, (struct sockaddr *)&all, sizeof(struct sockaddr_in));

	close(rs);
	// END

	printf("creating threads...\n");
	int ret = pthread_create(&thread, NULL, bloomChecker, NULL);
	if(ret == -1)
		die("forwarder - main: unable to create thread");
	

	to.sin_family = AF_INET;

	printf("setting up sockets...\n");
	// create a socket
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_TCP)) == -1)
		die("socket");

	// set socket options to send packets
	if(setsockopt(s, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) == -1)
        	die("setsockopt IP");

	// TODO: change to CONFIG
	char *dev = "eth1";
	char errbuf[PCAP_ERRBUF_SIZE];

	// TODO: change to configs
	char filter_exp[] = "tcp port 5555 and dst 192.168.7.100 or udp port 33000";

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

	die("forwarder - main: Done!");
	return 0;
}

int isFaulty(int lb){
	// 100 , 10
	return state.susp[lb] >= fconfig.KILL_TH_SUSP || state.asusp[lb] >= fconfig.KILL_TH_ASUSP;
}

void markFaulty(int lb){
	printf("---->>> %d says LB%d is faulty <<<----\n", config.id, lb);
	state.faulty[lb] = 1;
	
	// send restart request to faulty replica
	int rs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	struct sockaddr_in all;
	all.sin_family = AF_INET;
	all.sin_addr.s_addr = inet_addr(config.frontEnd);
	all.sin_port = htons(config.helloPort);
		
	int type = 2;
	char reset[sizeof(int)*3+1];
	memcpy(reset, &type, sizeof(int));
	memcpy(reset+sizeof(int), &lb, sizeof(int));
	memcpy(reset+sizeof(int)*2, &config.id, sizeof(int));
	reset[sizeof(int)*3] = '\0';
		
	sendto(rs, (const char*) reset, sizeof(int)*3, 0, (struct sockaddr *)&all, sizeof(struct sockaddr_in));

	close(rs);
	
	//printf("/sent\n");
}

int amWhatcher(int lb, int num){
	int i;
	for(i = (lb+1)%num; i != (lb+(2*fconfig.faults))%num ; i=(i+1)%num)
		if(config.id == i)
			return 1;
	
	if(config.id == i)		
		return 1;
		
	return 0;
	
	//return (lb+1)%num <= config.id && (lb+2*fconfig.faults)%num >= config.id;
}

void sendPacket(struct iphdr * iph, struct tcphdr * tcph, unsigned char * buffer, int lb){
	// Create addr to dest
	to.sin_addr.s_addr = iph->daddr;
	to.sin_port = tcph->dest;
	
	int addlen = sizeof(int);
	int tot_len = ntohs(iph->tot_len);
	
	// create new buffer to add ID
	if(tot_len >= 1500)
		tot_len -= addlen;

	unsigned char * newb = (unsigned char *) malloc(sizeof(char)*(tot_len+addlen));

	memcpy(newb, buffer, tot_len);
	// add my ID
	memcpy(newb+tot_len, &lb, addlen);
	
	tot_len+=addlen;
	
	// create struct for ip header	
	iph = (struct iphdr*) newb;
	
	// create struct for tcp header
	unsigned short iphdrlen = iph->ihl*4;
	tcph = (struct tcphdr *) (newb + iphdrlen);
	
	iph->tot_len = htons(tot_len);
	
	// Compute TCP checksum
	compute_tcp_checksum(iph, (unsigned short*)tcph);
	compute_ip_checksum(iph);
	
	if ((err = sendto(s, newb, ntohs(iph->tot_len),0,(struct sockaddr *)&to, sizeof(to))) < 0)
		die("forwarder - sendPacekt: send");
	
	count[config.id]++;

	free(newb);	
}

void terminate(int sig){
	pthread_kill(thread, sig);
	die(strsignal(sig));
}

void die(char *error){
	end = 1;
	sleep(1);

	perror(error);

	
	pthread_join(thread, NULL);
	pthread_mutex_destroy(&lock);

	printf("L0: %d | L1: %d | L2: %d\n", count[0], count[1], count[2]);
	/*
	int k = 0;
	for(k = 0; k < bs; k++){
		checkFilter(bags[k]);
		free(bags[k]);
	}
	free(bags);

	printf("L0: %d | L1: %d | L2: %d\n", count[0], count[1], count[2]);*/
	
	if(newts != -1)
		close(newts);
	newts = -1;
	
	if(s != -1)
		close(s);
	s= -1;

	if(ts != -1)
		close(ts);
	ts = -1;
	
	cleanState();
	clearState();
	
	cleanConfigs();
	clearConfigs();

	// pcap
	pcap_freecode(&fp);

	if(handle != NULL)
		pcap_close(handle);
	handle = NULL;

	exit(1);
}

void resetState() {
	int i;
	for(i = 0; i < state.numLB; i++){
		state.susp[i] = 0;
		state.asusp[i] = 0;
		state.faulty[i] = 0;

		if(state.list[i] != NULL)
			cleanList(state.list[i]);
		state.list[i] = NULL;
		
		if(state.restart != NULL)
			state.restart[i] = 0;
	}
}

void clearState() {
	state.numLB = -1;
	state.numServers = -1;
	state.lbs = NULL;
	state.servers = NULL;
	state.susp = NULL;
	state.asusp = NULL;
	state.list = NULL;
	state.faulty = NULL;
	state.restart = NULL;
}

void cleanState() {
	int i;
	if(state.lbs != NULL){
		for(i = 0; i < state.numLB; i++){
			if(state.lbs[i].ip != NULL)
				free(state.lbs[i].ip);
			state.lbs[i].ip = NULL;
		}
		free(state.lbs);	
	}		
	state.lbs = NULL;
	
	if(state.list != NULL){
		for(i = 0; i < state.numLB; i++){
			if(state.list[i] != NULL)
				cleanList(state.list[i]);
			state.list[i] = NULL;
		}
		free(state.list);
	}
	state.list = NULL;
	
	if(state.servers != NULL){
		for(i = 0; i < state.numServers; i++){
			if(state.servers[i].ip != NULL)
				free(state.servers[i].ip);
			state.servers[i].ip = NULL;
		}
		free(state.servers);
	}
	state.servers = NULL;
	
	if(state.restart != NULL)
		free(state.restart);
	state.restart = NULL;
	
	if(state.faulty != NULL)
		free(state.faulty);
	state.faulty = NULL;
	
	if(state.susp != NULL)
		free(state.susp);
	state.susp = NULL;
	
	if(state.asusp != NULL)
		free(state.asusp);
	state.asusp = NULL;
}
