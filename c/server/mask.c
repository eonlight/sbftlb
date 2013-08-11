#include <stdio.h>
#include <stdlib.h>

#include <signal.h> // Signals
#include <pthread.h> // Threads

#include <string.h>

int counter = 0;
int count[] = {0,0,0};

#include "hash.c"

#include "mask.h"
#include "config.c"
#include "packets.c"

#include "thread.c"

void addToFilter(char * buffer, int id, int size){
	pthread_mutex_lock(&lock);
	if(state.blooms != NULL && state.blooms[id] != NULL && state.blooms[id]->bloom != NULL){
		addToBloom(state.blooms[id]->bloom, buffer, size);
		state.blooms[id]->packets++;
	}
	pthread_mutex_unlock(&lock);
}

void addNewLB(int id, char *ip){

	LoadBalancer *newLBS = (LoadBalancer *) malloc(sizeof(LoadBalancer)*(state.numLB+1));
	memcpy(newLBS, state.lbs, sizeof(LoadBalancer)*state.numLB);
	LoadBalancer * oldLBS = state.lbs;
	state.lbs = newLBS;
	free(oldLBS);

	state.lbs[state.numLB].ip = (char *) malloc(sizeof(char)*(strlen(ip)+1));
	memcpy(state.lbs[state.numLB].ip, ip, strlen(ip)*sizeof(char));
	state.lbs[state.numLB].ip[strlen(ip)] = '\0';
	state.lbs[state.numLB].id = id;

	state.numLB++;
}

int handlePacket(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data){
	
	unsigned char *buffer;
	// buffer[recv_len] == '\0'
	nfq_get_payload(nfa, (char **) &buffer);

	struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfa);
	int id = ntohl(ph->packet_id);

	// create struct for ip header
	struct iphdr *iph = (struct iphdr*) buffer;
	
	// create struct for tcp header
	unsigned short iphdrlen = iph->ihl*4;
	
	if(iph->protocol == UDP_PROTO){
    	struct udphdr *udph = (struct udphdr*)(buffer + iphdrlen);
    	char * pl = (char *) buffer + iphdrlen + sizeof(udph);
    	
    	int type, lbid;
    	memcpy(&type, pl, sizeof(int));
		memcpy(&lbid, pl+sizeof(int), sizeof(int));

    	if(type == HELLO_LB_PROTO){
			if(lbid == state.numLB){
				struct sockaddr_in source;
				memset(&source, 0, sizeof(source));
    			source.sin_addr.s_addr = iph->saddr;
    			printf("Adding new LB...\n");
				//Add new LB to the list
				addNewLB(lbid, inet_ntoa(source.sin_addr));
			}
		}

		return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
	}

	struct tcphdr *tcph = (struct tcphdr *) (buffer + iphdrlen);

	// IF OUTPUT
	/*if(ntohs(tcph->source) == config.serverPort) {

		// Change sport
		tcph->source = htons(config.frontPort);
		// Change source
		iph->saddr = inet_addr(config.frontEnd);

		// compute checksums
		compute_tcp_checksum(iph, (unsigned short*)tcph);
		compute_ip_checksum(iph);
		
		// get packet id
		struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfa);
		id = ntohl(ph->packet_id);
	
		return nfq_set_verdict(qh, id, NF_ACCEPT, ntohs(iph->tot_len), (unsigned char *)buffer);
	}*/
	
	// ELSE IF INPUT
	int lb = -1;
	int add_len = sizeof(int);

	int tot_len = ntohs(iph->tot_len);
	memcpy(&lb, buffer+tot_len-add_len, add_len);
	
	iph->tot_len = htons(tot_len-add_len);
	tot_len = ntohs(iph->tot_len);
	buffer[tot_len] = '\0';

	iph->daddr = inet_addr(config.frontEnd);

	compute_tcp_checksum(iph, (unsigned short*)tcph);
	compute_ip_checksum(iph);

	//if(lb >= 0 && lb < state.numLB)// && ((unsigned int)tcph->syn) != 1)
		//addToFilter((char *)buffer, lb, tot_len);

	/* TEST | INFOR VARS */
	//count[lb]++;
	//counter++;
	/******** END ********/

	// Default: accept packets
	return nfq_set_verdict(qh, id, NF_ACCEPT, ntohs(iph->tot_len), (unsigned char *)buffer);
}

int main(int argc, char **argv){

	if(argc < 2){
		printf("Usage: %s [config file]\n", argv[0]);
		die("no config file");
	}
	
	// init mutex 
	if (pthread_mutex_init(&lock, NULL) != 0)
        die("mutex init failed");
	
	signal(SIGINT, terminate);
	signal(SIGTERM, terminate);
	signal(SIGKILL, terminate);
	
	readConfig(argv[1]);	
	addIptablesRules();

	// HELLO protocol for Servers
	int rs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	struct sockaddr_in all;
	all.sin_family = AF_INET;
	all.sin_addr.s_addr = inet_addr(config.frontEnd);
	all.sin_port = htons(config.helloPort);

	int type = HELLO_SERV_PROTO;
	char hello[sizeof(int)*2+1];
	memcpy(hello, &type, sizeof(int));
	memcpy(hello+sizeof(int), &config.id, sizeof(int));
	hello[sizeof(int)*2] = '\0';

	sendto(rs, (const char*) hello, sizeof(int)*2, 0, (struct sockaddr *)&all, sizeof(struct sockaddr_in));
	close(rs);
	
	//create bloom filter thread
	/*int ret = pthread_create(&thread, NULL, bloomThread, NULL);
	if(ret == -1)
		die("unable to create thread");
	*/
	int fd, rv;
	char buf[4096] __attribute__ ((aligned));

	printf("Setting up nfqueue...\n");
	// opening library handle
	h = nfq_open();
	if(!h)
		die("error during nfq_open()");

	// unbinding existing nf_queue handler for AF_INET (if any)
	if(nfq_unbind_pf(h, AF_INET) < 0)
		die("error during nfq_unbind_pf()");
	
	// binding nfnetlink_queue as nf_queue handler for AF_INET
	if(nfq_bind_pf(h, AF_INET) < 0)
		die("error during nfq_bind_pf()");
		
	// binding this socket to queue '1'
	qh = nfq_create_queue(h,  1, &handlePacket, NULL);
	if(!qh)
		die("error during nfq_create_queue()");

	// setting copy_packet mode
	if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0)
		die("can't set packet_copy mode");

	fd = nfq_fd(h);

	printf("All set!\n");
	while (end != 1){
		rv = recv(fd, buf, sizeof(buf), 0);
		if(rv >= 0)
			nfq_handle_packet(h, buf, rv);
	}
	
	printf("Cleaning...\n");
	// unbinding from queue 1
	if(qh != NULL)
		nfq_destroy_queue(qh);
	qh = NULL;

#ifdef INSANE
	/* normally, applications SHOULD NOT issue this command, since
	 * it detaches other programs/sockets from AF_INET, too ! */
	// unbinding from AF_INET
	if(h != NULL)
		nfq_unbind_pf(h, AF_INET);
#endif
	// closing library handle
	if(h != NULL)
		nfq_close(h);
	h = NULL;
	
	// closing socket
	if(s != -1)
		close(s);
	s = -1;
	
	die("Done!\n");
	return 0;
}

// when a signal is catched
void terminate(int sig){
	pthread_kill(thread, sig);
	die(strsignal(sig));
}

void die(char *error){
	end = 1;
	sleep(1);

	pthread_join(thread, NULL);
	pthread_mutex_destroy(&lock);

	if(qh != NULL)
		nfq_destroy_queue(qh);
	qh = NULL;
	
	if(h != NULL)
		nfq_close(h);
	h = NULL;
	
	cleanConfigs();
	cleanState();

	if(s != -1)
		close(s);
	s= -1;

	if(ts != -1)
		close(ts);
	ts = -1;
	
	perror(error);
	exit(1);
}

void clearState() {
	state.numLB = -1;
	state.lbs = NULL;
	state.blooms = NULL;
	state.toSend = NULL;	
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

	if(state.toSend != NULL){
		for(i = 0; i < state.numLB; i++){
			if(state.toSend[i] != NULL 
				&& state.toSend[i]->bloom != NULL)
				destroyBloom(state.toSend[i]->bloom);
			if(state.toSend[i] != NULL)
				free(state.toSend[i]);
			state.toSend[i] = NULL;
		}
		free(state.toSend);
	}
	state.toSend = NULL;

	if(state.blooms != NULL){
		for(i = 0; i < state.numLB; i++){
			if(state.blooms[i] != NULL 
				&& state.blooms[i]->bloom != NULL)
				destroyBloom(state.blooms[i]->bloom);
			if(state.blooms[i] != NULL)
				free(state.blooms[i]);
			state.blooms[i] = NULL;
		}
		free(state.blooms);
	}
	state.blooms = NULL;
}
