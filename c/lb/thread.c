#include <fcntl.h> /* set non blocking recv */
#include <errno.h> 

#include "thread.h"

// add to the head
void addListToList(int t, int lb, HttpRequestNode *head, HttpRequestNode *tail){
	if(state.list[t][lb] == NULL){
		state.list[t][lb] = head;
		//state.tail[lb] = tail;
	}
	else {
		tail->next = state.list[t][lb];
		state.list[t][lb]->prev = tail;
		state.list[t][lb] = head;
	}
}

void * checkList(void * args){
	sleep(1);
	//long my_id = (long) args;
	int myid = 0;
	for(myid = 0; myid < SEARCH_THREADS_NUM; myid++)
		if(searchThreads[myid] == pthread_self())
			break;

	printf("Myid: %d\n", myid);

	while(end != 1){

		if(workingBloom != NULL && workingBloom->threads[myid] != 1){

			int lb = workingBloom->lb;
			int server = workingBloom->server;

			pthread_mutex_lock(&searchLocks[myid]);
			if(state.list[myid][lb] != NULL){

				//check my stuffs
				HttpRequestNode *current = state.list[myid][lb];
				state.list[myid][lb] = NULL;
				state.tail[myid][lb] = NULL;
				int nlist = lcount[lb];
				pthread_mutex_unlock(&searchLocks[myid]);

				HttpRequestNode *head = NULL;
				HttpRequestNode *tail = NULL;

				//check
				do {
					current = removeFromList(current);
					/*if(current->server == server && checkBloom(workingBloom->bloom, current->buffer, current->len)){
						if(state.asusp[lb] > 0)
							state.asusp[lb]--;

						workingBloom->packets--;
						current = removeFromList(current);
						lcount[lb]--;

					}
					else if(current->server == server && current->added+fconfig.TIMEOUT < time(0)){
						printf("<<<<<<<<<< timeout: %d >>>>>>>>>>>>\n", nlist);
						state.susp[lb]++;
						state.asusp[lb]++;
						// Create structs pointers
						//struct iphdr *iph = (struct iphdr*) current->buffer;
						//struct tcphdr *tcph = (struct tcphdr *) (current->buffer + iph->ihl*4);
						//sendPacket(iph, tcph, (unsigned char *) current->buffer, -1);
						current = removeFromList(current);
						lcount[lb]--;
					}
					else {
						if(head == NULL)
							head = current;
						tail = current;
						current = current->next;
					}*/

				}while(current != NULL);
				
				pthread_mutex_lock(&searchLocks[myid]);
				if(head != NULL)
					addListToList(myid, lb, head, tail);
			}
			
			pthread_mutex_unlock(&searchLocks[myid]);
			workingBloom->threads[myid] = 1;
		}
		else
			sleep(config.wait);
	}

	pthread_exit((void *) args);
}


void checkFilter(LBBloom * bloom){
	
	int lb = bloom->lb;
	//int server = bloom->server;

	// if I saw LBi packets
	/*if(state.list[lb] != NULL){
		
		pthread_mutex_lock(&lock);
		HttpRequestNode *current = state.list[lb];
		state.list[lb] = NULL;
		int nlist = lcount[lb];
		pthread_mutex_unlock(&lock);

		HttpRequestNode *head = NULL;
		HttpRequestNode *tail = NULL;

		do {
			if(current->server == server && checkBloom(bloom->bloom, current->buffer, current->len)){
				if(state.asusp[lb] > 0)
					state.asusp[lb]--;

				bloom->packets--;
				current = removeFromList(current);
				lcount[lb]--;

			}
			else if(current->server == server && current->added+fconfig.TIMEOUT < time(0)){
				printf("<<<<<<<<<< timeout: %d >>>>>>>>>>>>\n", nlist);
				state.susp[lb]++;
				state.asusp[lb]++;
				// Create structs pointers
				//struct iphdr *iph = (struct iphdr*) current->buffer;
				//struct tcphdr *tcph = (struct tcphdr *) (current->buffer + iph->ihl*4);
				//sendPacket(iph, tcph, (unsigned char *) current->buffer, -1);
				current = removeFromList(current);
				lcount[lb]--;
			}
			else {
				if(head == NULL)
					head = current;
				tail = current;
				current = current->next;
			}

		}while(current != NULL);

		if(head != NULL){
			pthread_mutex_lock(&lock);
			addListToList(t, lb, head, tail);
			pthread_mutex_unlock(&lock);
		}
	}*/

	if(bloom->packets < 0){
		printf("problem:  %d\n", bloom->packets);
	}

	if(bloom->packets > 0){
		printf("<<<<<<<<<< %d a mais de %d >>>>>>>>>>>>\n", bloom->packets, bloom->lb);
		state.susp[lb]++;
		state.asusp[lb]++;
	}
		
	if(isFaulty(lb))
		markFaulty(lb);

	if(bloom->bloom != NULL)
		destroyBloom(bloom->bloom);
		
	if(bloom != NULL)
		free(bloom);
	bloom = NULL;

}

void readBloom(char *buf) {
	int ilen = sizeof(int);

	// LB:SERVER_ID:NUM_PACKETS:BLOOM
	int lb, server, packets;

	memcpy(&lb, buf, ilen);
	memcpy(&server, buf+ilen, ilen);
	memcpy(&packets, buf+2*ilen, ilen);

	workingBloom = (LBBloom *) malloc(sizeof(LBBloom));

	if(!(workingBloom->bloom=createBloom(bconfig.bloomLen, 0.1)))
		die("cannot create bloom filter");

	int blen = sizeof(char)*workingBloom->bloom->bytes;

	workingBloom->server = server;
	workingBloom->lb = lb;
	workingBloom->packets = packets;
	int i = 0;
	for(i = 0; i < SEARCH_THREADS_NUM; i++)
		workingBloom->threads[i] = 0;

	memcpy(workingBloom->bloom->bf, buf+3*ilen, blen);

	//printf("Received %d from %d with %d (%d) packets - %08X\n", lb, server, packets, count[lb], MurmurHash2(workingBloom->bloom->bf, blen, 0x00));
	printf("L0: %d | L1: %d | L2: %d | T: %d\n", count[0], count[1], count[2], counter);
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
	int blen = sizeof(int)*3+sizeof(char)*bloom->bytes;
	destroyBloom(bloom);
	
	char buffer[blen+1];
	buffer[blen] = '\0';

	//bags = (char **) malloc(sizeof(char *)*100);
	socklen_t slen = sizeof(server);
	while(end != 1 ){

		/* TODO: change to only 1 socket */

		newts = accept(ts, (struct sockaddr *) &server, &slen);
		if(newts < 0)
			die("thread accept");
		
		int r = 0;
		while(r < blen)
			r += read(newts, buffer+r, blen);
		

		printf("new bloom...\n");

		readBloom(buffer);

		int i = 0;
		/*for(i = 0; i < SEARCH_THREADS_NUM; i++){

		}*/

		/* TODO: THREADS!!!!! */
		/* checkfilter -> function thread */
		/* new function, locks[num_threads], check_list[num_threads] */
		//checkFilter(lbb);
		while(i != SEARCH_THREADS_NUM){
			for(i = 0; i < SEARCH_THREADS_NUM; i++)
				if(workingBloom->threads[i] == 0)
					break;
			if(i != SEARCH_THREADS_NUM)
				sleep(0.0001);
		}

		int lb = workingBloom->lb;

		if(workingBloom->packets < 0)
			printf("!!!!!!!!!!!!!!!!!!!! Problem: %d collisions. !!!!!!!!!!!!!!!!!!!!\n", workingBloom->packets);

		if(workingBloom->packets > 0){
			printf("<<<<<<<<<< %d a mais de %d >>>>>>>>>>>>\n", workingBloom->packets, workingBloom->lb);
			state.susp[lb]++;
			state.asusp[lb]++;
		}
				
		if(isFaulty(lb))
			markFaulty(lb);

		if(workingBloom->bloom != NULL)
			destroyBloom(workingBloom->bloom);
				
		if(workingBloom != NULL)
			free(workingBloom);
		workingBloom = NULL;
		
		close(newts);

	}

	if(ts != -1)
		close(ts);
	ts = -1;

	pthread_exit((void *)arg);
}
