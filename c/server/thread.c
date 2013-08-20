#include <fcntl.h> // set non blocking recv
#include <unistd.h>

int ts = -1;

void sendBloom(int lb);

void resetBloom(LBBloom *bloom) {	
	if(bloom->bloom != NULL)
		destroyBloom(bloom->bloom);
	bloom->packets = 0;
	bloom->init = time(0);
	if(!(bloom->bloom = createBloom(BLOOM_LEN, BLOOM_ERROR)))
		die("cannot create bloom filter");		
}

void *bloomManager(void *arg){
	time_t now;
	int lb;

	// Check if to renew next time
	while(1){	
		now = time(0);
		for(lb = 0; lb < LB_NUM; lb++)
			if(active[lb]->init+RENEW < now){
				//sendBloom(lb);

				pthread_mutex_lock(&lock);
				memcpy(toSend[lb]->bloom->bf, active[lb]->bloom->bf, sizeof(char)*active[lb]->bloom->bytes);
				toSend[lb]->packets = active[lb]->packets;
				toSend[lb]->init = active[lb]->init;
				resetBloom(active[lb]);
				pthread_mutex_unlock(&lock);
			}
	}
	
	return NULL;
}

char * createBloomBuffer(LBBloom *bloom){

	// LB:SERVER_ID:NUM_PACKETS:BLOOM
	// 4b:4b:4b:BLOOM_LEN
	int ilen = sizeof(int), blen = sizeof(char)*bloom->bloom->bytes;
	char * buffer = (char *) calloc(sizeof(char), ilen*3+blen+1);

	memcpy(buffer, &(bloom->lb), ilen);
	memcpy(buffer+ilen, &(bloom->server), ilen);
	memcpy(buffer+ilen*2, &(bloom->packets), ilen);
	memcpy(buffer+ilen*3, bloom->bloom->bf, blen);
	buffer[ilen*3+blen] = '\0';
	return buffer;
}

int isWhatcher(int lb, int whatcher){
	int i;
	for(i = (lb+1)%LB_NUM; i != (lb+(2*FAULTS))%LB_NUM ; i=(i+1)%LB_NUM)
		if(whatcher == i)
			return 1;
	
	return whatcher == i;
}

void sendBloom(int lb){
	int i, blen = sizeof(char)*active[0]->bloom->bytes+3*sizeof(int);
	char * buffer = createBloomBuffer(toSend[lb]);
	for(i = 0; i < LB_NUM; i++)
		if(isWhatcher(lb, i)){
		
			/* TODO: FAZER SO 1 SOCKET por LB */
			if((ts = socket(AF_INET, SOCK_STREAM, 0)) == -1)
				die("thread socket");
	
			lbaddr.sin_addr.s_addr = inet_addr(lbs[i]);
			lbaddr.sin_port=htons(BACK_PORT);

			// check if good to connect
			struct timeval tv;
			fd_set myset;
			socklen_t lon;

			// Set non-blocking 
			int arg = fcntl(ts, F_GETFL, NULL); 
			arg |= O_NONBLOCK; 
			fcntl(ts, F_SETFL, arg); 

			// Trying to connect with timeout 
			int reconnect = 1;
			int valopt, res = connect(ts, (struct sockaddr *)&lbaddr, sizeof(lbaddr)); 
			if(res < 0){
				
				// TODO: CHANGE this to config
				tv.tv_sec = 1;
				tv.tv_usec = 0;
				
				FD_ZERO(&myset); 
				FD_SET(ts, &myset); 
				if (select(ts+1, NULL, &myset, NULL, &tv) > 0) { 
					lon = sizeof(int); 
					getsockopt(ts, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon);
					if(valopt)
						reconnect = 0;
				} 
				else
					reconnect = 0;
			}
			else
				reconnect = 0;
			
			/* END: checked if good to connect or not */
			
			if(reconnect == 1){
				if(res < 0){
					// Set to blocking mode again... 
					arg = fcntl(ts, F_GETFL, NULL); 
					arg &= (~O_NONBLOCK); 
					fcntl(ts, F_SETFL, arg);
				}
			
				if((res = connect(ts ,(struct sockaddr *) &lbaddr, sizeof(lbaddr))) < 0)
					die("cannot connect to LB");
			}
	
			if(res >= 0){
				//printf("sendding\n");
				int w = write(ts, buffer, blen);
				if(w < 0)
					die("cannot write to LB");
			}
			
			if(ts != -1)
				close(ts);
			ts = -1;
		}
	free(buffer);
}

void cleanThread(){
	printf("cleanAlgorithm: cleaning threads...\n");
	pthread_kill(thread, 2);
	pthread_join(thread, NULL);

	/*if(ts != -1)
		close(ts);
	ts = -1;*/
}