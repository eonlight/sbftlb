#include "bloom.c"

typedef struct {
	Bloom *bloom;
	int packets; //num packets on bloom
	time_t init;
	int lb; //lb id
	int server; //server id
} LBBloom;

int ts = -1, newts = -1;

int isFaulty(int lb){
	// 100 , 10
	return susp[lb] >= KILL_TH_SUSP || asusp[lb] >= KILL_TH_ASUSP;
}

void voteFaulty(int lb){
	printf("---------------->>> vote LB%d faulty. <<<----------------\n", lb);
	
	//TODO: add votes[lb][server] = 1
	votes[lb][ID] = 1;

	//send faulty fault to every component
	int rs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	struct sockaddr_in all;
	all.sin_family = AF_INET;
	all.sin_addr.s_addr = inet_addr(HELLO_ADDR);
	all.sin_port = htons(HELLO_PORT);
		
	int type = RESET_LB_PROTO;
	char reset[sizeof(int)*2+1];
	memcpy(reset, &type, sizeof(int));
	memcpy(reset+sizeof(int), &lb, sizeof(int));
	reset[sizeof(int)*2] = '\0';
		
	sendto(rs, (const char*) reset, sizeof(int)*2, 0, (struct sockaddr *)&all, sizeof(struct sockaddr_in));

	close(rs);
	
}

void addListToList(int lb, int server, HttpRequestNode *h, HttpRequestNode *t){
	if(list[lb][server] == NULL){
		list[lb][server] = h;
		tail[lb][server] = t;
	}
	else {
		t->next = list[lb][server];
		list[lb][server]->prev = t;
		list[lb][server] = h;
	}
}

void checkList(LBBloom * bloom, int now){//, int bag){
	
	int lb = bloom->lb;
	int server = bloom->server;

	// if I saw LBi packets
	if(list[lb][server] != NULL){
		
		pthread_mutex_lock(&lock);
		HttpRequestNode *current = list[lb][server];
		list[lb][server] = NULL;
		tail[lb][server] = NULL;
		pthread_mutex_unlock(&lock);

		HttpRequestNode *h = NULL;
		HttpRequestNode *t = NULL;

		do {
			if(checkBloom(bloom->bloom, current->buffer, current->len)){
				//printf("in-");
				if(asusp[lb] > 0)
					asusp[lb]--;

				bloom->packets--;
				packets[lb]--;
				current = removeFromList(current);

			}
			else if(current->added+TIMEOUT < now){
				printf(">>>>>>>>>>>>>>>>>> timeout >>>>>>>>>>>>>>>>>>\n");
				susp[lb]++;
				asusp[lb]++;
				// Create structs pointers
				struct iphdr *iph = (struct iphdr*) current->buffer;
				struct tcphdr *tcph = (struct tcphdr *) (current->buffer + iph->ihl*4);
				//TODO: Problem next bag +1 from lb = susp++; asusp++; 
				sendPacket(iph, tcph, server);
				packets[lb]--;
				current = removeFromList(current);
			}
			else {
				//printf("else-");
				if(h == NULL)
					h = current;
				t = current;
				current = current->next;
			}

		}while(current != NULL);

		if(h != NULL){
			pthread_mutex_lock(&lock);
			addListToList(lb, server, h, t);
			pthread_mutex_unlock(&lock);
		}
	}

	if(bloom->packets < 0){
		printf("Falso positivo:  %d\n", bloom->packets);
		wildcard[lb] += bloom->packets;
	}

	if(bloom->packets > 0){
		bloom->packets += wildcard[lb];
		if(bloom->packets > 0){
			printf("<<<<<<<<<< %d a mais de %d >>>>>>>>>>>>\n", bloom->packets, bloom->lb);
			susp[lb]++;
			asusp[lb]++;
		}
		else
			wildcard[lb] += bloom->packets;
	}
		
	if(isFaulty(lb))
		voteFaulty(lb);
}

LBBloom * readBloom(char *buf) {
	int ilen = sizeof(int);
	int lb, server, packets;

	memcpy(&lb, buf, ilen);
	memcpy(&server, buf+ilen, ilen);
	memcpy(&packets, buf+2*ilen, ilen);

	LBBloom *lbb = (LBBloom *) malloc(sizeof(LBBloom));

	if(!(lbb->bloom=createBloom(BLOOM_LEN, 0.1)))
		die("cannot create bloom filter");

	int blen = sizeof(char)*lbb->bloom->bytes;

	lbb->server = server;
	lbb->lb = lb;
	lbb->packets = packets;

	memcpy(lbb->bloom->bf, buf+3*ilen, blen);

	return lbb;
}

void * bloomChecker(void *arg){

	int r, now, on = 1;

	printf("bloomChecker: setting up listen socket... \n");
	if((ts = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		die("thread socket");
		
	struct sockaddr_in me, server;
	memset((char *) &me, 0, sizeof(me));
	me.sin_family = AF_INET;
	me.sin_addr.s_addr=htonl(INADDR_ANY);
	me.sin_port=htons(BACK_PORT);
	
	setsockopt(ts, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	
	if(bind(ts, (struct sockaddr *) &me, sizeof(me)) < 0){
		//printf("errno: %s\n", strerror(errno));
		die("thread bind"); 
	}

	listen(ts, SERVER_NUM);

	Bloom *bloom=createBloom(BLOOM_LEN, BLOOM_ERROR);
	int blen = sizeof(int)*3+sizeof(char)*bloom->bytes;
	destroyBloom(bloom);
	
	char buffer[blen+1];
	buffer[blen] = '\0';

	//bags = (char **) malloc(sizeof(char *)*100);
	socklen_t slen = sizeof(server);
	printf("bloomChecker: Thread listening... \n");
	while(1){

		newts = accept(ts, (struct sockaddr *) &server, &slen);
		if(newts < 0)
			die("thread accept");
		
		r = 0;
		while(r < blen)
			r += read(newts, buffer+r, blen);
		now = time(0);

		LBBloom * lbb = readBloom(buffer);

		checkList(lbb, now);
		
		if(lbb->bloom != NULL)
			destroyBloom(lbb->bloom);
		
		if(lbb != NULL)
			free(lbb);
		lbb = NULL;
		close(newts);

	}

	if(ts != -1)
		close(ts);
	ts = -1;

	return NULL;
}

void cleanThreads(){

	printf("cleanAlgorithm: cleaning threads...\n");
	pthread_kill(thread, 2);
	pthread_join(thread, NULL);

	if(ts != -1)
		close(ts);
	ts = -1;

	if(newts != -1)
		close(newts);
	newts = -1;
}