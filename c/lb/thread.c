#include <fcntl.h> /* set non blocking recv */
#include <errno.h> 

#include "thread.h"

//LBBloom temp[3];



void checkFilter(char *buf){
	
	LBBloom * bloom = readBloom(buf);
	
	int lb = bloom->lb;
	// if I saw LBi packets
	if(state.list[lb] != NULL){
		
		HttpRequestNode *current = state.list[lb];
		state.list[lb] = NULL;
		//printf("%d", a++);
		do {
		
			// Create structs pointers
			//struct iphdr *iph = (struct iphdr*) current->buffer;
			//struct tcphdr *tcph = (struct tcphdr *) (current->buffer + iph->ihl*4);
			//bloom->packets--;
			//current = removeFromList(current);
			if(checkBloom(bloom->bloom, current->buffer, current->len)){
				//if(state.asusp[lb] > 0)
				//	state.asusp[lb]--;
				count[lb]--;
				bloom->packets--;
				current = removeFromList(current);
			}
			else if(current->added+fconfig.TIMEOUT < time(0)){
				printf("<<<<<<<<<< timeout >>>>>>>>>>>>\n");
				//state.susp[lb]++;
				//state.asusp[lb]++;
				//count[lb]--;
				//sendPacket(iph, tcph, (unsigned char *) current->buffer, -1);
				current = removeFromList(current);
			}
			else {
				pthread_mutex_lock(&lock);
				if(state.list[lb] == NULL)
					state.list[lb] = current;
				current = current->next;
				pthread_mutex_unlock(&lock);
			}
		}while(current != NULL);	
	}

	if(bloom->packets < 0){
		printf("problem:  %d\n", bloom->packets);
	}

	if(bloom->packets > 0){

			printf("<<<<<<<<<< %d a mais de %d >>>>>>>>>>>>\n", bloom->packets, bloom->lb);

		//state.susp[lb]++;
		//state.asusp[lb]++;
		/*if(bloom->packets > 100){
			int d;
			int fd = 0;
			for(d = 0; d < bloom->bloom->bfsize; d++)
				if(bloom->bloom->bf[d] != 0)
					fd = 1;
			printf("fd:%d\n",fd);
		}*/
	}
		
	if(isFaulty(lb))
		markFaulty(lb);

	if(bloom->bloom != NULL)
		destroyBloom(bloom->bloom);
		
	if(bloom != NULL)
		free(bloom);
		
	//bloom = NULL;

}

LBBloom * readBloom(char *buf) {
	/* with TEMP*/
	/*int ilen = sizeof(int), blen = sizeof(char)*bconfig.bloomLen;
	
	// LB:SERVER_ID:NUM_PACKETS:BLOOM
	int lb, server, packets;
	
	memcpy(&lb, buf, ilen);
	memcpy(&server, buf+ilen, ilen);
	memcpy(&packets, buf+2*ilen, ilen);
	
	//LBBloom *lbb = (LBBloom *) malloc(sizeof(LBBloom));

	if(temp[lb].bloom == NULL)	
		if(!(temp[lb].bloom=createBloom(bconfig.bloomLen)))
			die("cannot create bloom filter");

	temp[lb].server = server;
	temp[lb].lb = lb;
	temp[lb].packets = packets;

	memcpy(temp[lb].bloom->bf, buf+3*ilen, blen);

	//printf("Packets: %d <<<<<<<<<<<<<<<<<\n", packets);
	printf("%d: %d (%d) packets <<<<<<<<<<<<<<<<<\n", lb, packets, count[lb]);


	// PRINT LB_BLOOM DATA
	//printf("**** (s%d,f%d):%d ****\n", server, lb, packets);
	//printf("L0: %d | L1: %d | L2: %d | T: %d\n", count[0], count[1], count[2], counter);
	//printSeenData
	
	return &temp[lb];*/
	/* NORMAL */
	int ilen = sizeof(int);

	// LB:SERVER_ID:NUM_PACKETS:BLOOM
	int lb, server, packets;

	memcpy(&lb, buf, ilen);
	memcpy(&server, buf+ilen, ilen);
	memcpy(&packets, buf+2*ilen, ilen);

	LBBloom *lbb = (LBBloom *) malloc(sizeof(LBBloom));

	if(!(lbb->bloom=createBloom(bconfig.bloomLen, 0.1)))
		die("cannot create bloom filter");

	int blen = sizeof(char)*lbb->bloom->bytes;

	lbb->server = server;
	lbb->lb = lb;
	lbb->packets = packets;

	memcpy(lbb->bloom->bf, buf+3*ilen, blen);

	//printf("%d: %d (%d) packets <<<<<<<<<<<<<<<<<\n", lb, packets, count[lb]);
	//printf("%d:%08X<-%d\n", lb, MurmurHash2(lbb->bloom->bf, blen, 0x00), server);
	printf("Received %d from %d with %d (%d) packets - %08X\n", lb, server, packets, 
			count[lb], MurmurHash2(lbb->bloom->bf, blen, 0x00));
	// PRINT LB_BLOOM DATA
	//printf("**** (s%d,f%d):%d ****\n", server, lb, packets);
	//printf("L0: %d | L1: %d | L2: %d | T: %d\n", count[0], count[1], count[2], counter);

	return lbb;
}

void sendLBsInfo(int newts){
	char resp[sizeof(int)*state.numLB+1];
	int i = 0;
	for(i = 0; i < state.numLB; i++)
		memcpy(resp+(i*sizeof(int)), &state.faulty[i], sizeof(int));
	resp[sizeof(int)*state.numLB] = '\0';
	int w = write(newts, resp, sizeof(int)*state.numLB);
	if(w < 0)
		die("cannot respond to Server");
}

void * bloomChecker(void *arg){

	if((ts = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		die("thread socket");
		
	struct sockaddr_in me, server;
	memset((char *) &me, 0, sizeof(me));
	me.sin_family = AF_INET;
	me.sin_addr.s_addr=htonl(INADDR_ANY);
	me.sin_port=htons(bconfig.backPort);
	
	setsockopt(ts, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	
	if(bind(ts, (struct sockaddr *) &me, sizeof(me)) < 0){
		printf("errno: %s\n", strerror(errno));
		die("thread bind"); 
	}

	listen(ts, state.numServers);
	Bloom *bloom=createBloom(bconfig.bloomLen, 0.1);
	int blen = sizeof(int)*3+
			sizeof(char)*bloom->bytes;

	/*temp[0].bloom = NULL;
	temp[1].bloom = NULL;
	temp[2].bloom = NULL;
	*/
	//bags = (char **) malloc(sizeof(char *)*100);
	socklen_t slen = sizeof(server);
	while(end != 1 ){

		/* TODO: change to only 1 socket */

		newts = accept(ts, (struct sockaddr *) &server, &slen);
		if(newts < 0)
			die("thread accept");

		char buffer[blen];
		buffer[blen-1] = '\0';
		
		int r = 0;
		while(r < blen)
			r += read(newts, buffer+r, blen);

		// join to Array
		checkFilter(buffer);
		//bags[bs] = (char *) malloc(sizeof(char)*blen);
		//memcpy(bags[bs], buffer, blen);
		//bs++;
		close(newts);

	}
	if(ts != -1)
		close(ts);
	ts = -1;
	return NULL;
}
