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
	
	//se f+1 restart then restart
	return total >= fconfig.faults+1;
}

void handleUDPProtocols(struct iphdr *iph, struct udphdr *udph, char * payload){
	int type, lbid;

    memcpy(&type, payload, sizeof(int));
	memcpy(&lbid, payload+sizeof(int), sizeof(int));
	
	switch(type){
		case PROTO_LB_HELLO:
			//IF restart
			if(lbid >= 0 && lbid < state.numLB){
				state.susp[lbid] = 0;
				state.asusp[lbid] = 0;

				pthread_mutex_lock(&lock);
				if(state.list[lbid] != NULL)
					cleanList(state.list[lbid]);
				state.list[lbid] = NULL;
				state.tail[lbid] = NULL;
				pthread_mutex_unlock(&lock);
			}
			//IF new
			else if(lbid == state.numLB){
				struct sockaddr_in source;
				memset(&source, 0, sizeof(source));
				source.sin_addr.s_addr = iph->saddr;
				printf("Adding new LB...\n");
				addNewLB(lbid, inet_ntoa(source.sin_addr));
			}
			break;
		//vote: restart and it's for me
		case PROTO_RESET:
			 if(lbid == config.id){
				//from sid => this can be doen with the source IP address
				int sid;
				memcpy(&sid, payload+sizeof(int)*2, sizeof(int));
				state.restart[sid]++;
			
				if(needRestart()){
					printf("Recovering...\n");

					resetState();
					
					printf("Seding HELLO...\n");
					int rs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

					struct sockaddr_in all;
					all.sin_family = AF_INET;
					all.sin_addr.s_addr = inet_addr(config.frontEnd);
					all.sin_port = htons(config.helloPort);
			
					int type = PROTO_LB_HELLO;
					char hello[sizeof(int)*2+1];
					memcpy(hello, &type, sizeof(int));
					memcpy(hello+sizeof(int), &config.id, sizeof(int));
					hello[sizeof(int)*2] = '\0';
			
					sendto(rs, (const char*) hello, sizeof(int)*2, 0, (struct sockaddr *)&all, sizeof(struct sockaddr_in));

					close(rs);
				}
			}
			break;
		//server: hello
		case PROTO_SERV_HELLO:;
			if(lbid == state.numServers){
				struct sockaddr_in source;
				memset(&source, 0, sizeof(source));
				source.sin_addr.s_addr = iph->saddr;
				printf("Adding new Server...\n");
				addNewServer(lbid, inet_ntoa(source.sin_addr));
			}
			break;
	}
}

int handlePacket(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data) {

	unsigned char *buffer;
	// buffer[recv_len] == '\0'
	int recv_len = nfq_get_payload(nfa, (char **) &buffer);

	// Get package queue id
	struct nfqnl_msg_packet_hdr *ph = nfq_get_msg_packet_hdr(nfa);
	int id = ntohl(ph->packet_id);

	// create struct for ip header
	struct iphdr *iph = (struct iphdr*) buffer;
	unsigned short iphdrlen = iph->ihl*4;
	
	if(iph->protocol == UDP_PROTO){
    	struct udphdr *udph = (struct udphdr*)(buffer + iphdrlen);
    	char *payload = (char *) buffer + iphdrlen + sizeof(udph);

    	handleUDPProtocols(iph, udph, payload);
    	
		return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
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
	
	if(amWhatcher(lb, state.numLB)){
		pthread_mutex_lock(&lock);
		addToList(lb, recv_len, buffer, server);
		pthread_mutex_unlock(&lock);
	}
	
	if(lb == config.id)
		sendPacket(iph, tcph, buffer, config.id);

	/* INFO | TEST vars */
	count[lb]++;
	counter++;
	/* END */

	// Default: drop packets
	return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
}

int main(int argc, char **argv){

	printf("starting...\n");

	if(argc < 2){
		printf("Usage: %s [config file] {1}\n", argv[0]);
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
	
	//HELLO protocol
	int rs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	struct sockaddr_in all;
	all.sin_family = AF_INET;
	all.sin_addr.s_addr = inet_addr(config.frontEnd);
	all.sin_port = htons(config.helloPort);

	int type = PROTO_LB_HELLO;
	char hello[sizeof(int)*2+1];
	memcpy(hello, &type, sizeof(int));
	memcpy(hello+sizeof(int), &config.id, sizeof(int));
	hello[sizeof(int)*2] = '\0';

	sendto(rs, (const char*) hello, sizeof(int)*2, 0, (struct sockaddr *)&all, sizeof(struct sockaddr_in));

	close(rs);
	
	int ret = pthread_create(&thread, NULL, bloomChecker, NULL);
	if(ret == -1)
		die("unable to create thread");

	int fd, rv;
	
	char buf[4096] __attribute__ ((aligned));

	printf("Setting up sockets...\n");
	to.sin_family = AF_INET;

	// create a socket
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_TCP)) == -1)
		die("socket");

	// set socket options to send packets
	if(setsockopt(s, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) == -1)
        	die("setsockopt IP");

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
	while(end != 1){
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

int isFaulty(int lb){
	// 100 , 10
	return state.susp[lb] >= fconfig.KILL_TH_SUSP || state.asusp[lb] >= fconfig.KILL_TH_ASUSP;
}

void markFaulty(int lb){
	printf("---->>> %d says LB%d is faulty <<<----\n", config.id, lb);

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
}

int amWhatcher(int lb, int num){
	int i;
	for(i = (lb+1)%num; i != (lb+(2*fconfig.faults))%num ; i=(i+1)%num)
		if(config.id == i)
			return 1;
	
	if(config.id == i)		
		return 1;
		
	return 0;
}

void sendPacket(struct iphdr * iph, struct tcphdr * tcph, unsigned char * buffer, int lb){
	// Create addr to dest
	to.sin_addr.s_addr = iph->daddr;
	to.sin_port = tcph->dest;
	
	int addlen = sizeof(int);
	int tot_len = ntohs(iph->tot_len);
	
	/*
	if(tot_len >= 1500)
		tot_len -= addlen;
	*/

	// create new buffer to add ID
	unsigned char * newb = (unsigned char *) malloc(sizeof(char)*(tot_len+addlen));

	// copy buffer and add my ID
	memcpy(newb, buffer, tot_len);
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
	printf("L0: %d | L1: %d | L2: %d | T: %d \n", count[0], count[1], count[2], counter);

	pthread_join(thread, NULL);
	pthread_mutex_destroy(&lock);

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
	clearConfigs();

	if(qh != NULL)
		nfq_destroy_queue(qh);
	qh = NULL;
	
	if(h != NULL)
		nfq_close(h);
	h = NULL;
	
	exit(1);
}