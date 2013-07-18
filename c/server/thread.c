#include <fcntl.h> // set non blocking recv
#include "thread.h"

void resetBloom(LBBloom *bloom) {
	//pthread_mutex_lock(&lock);
	
	if(bloom->bloom != NULL)
		destroyBloom(bloom->bloom);
	bloom->packets = 0;
	bloom->init = time(0);
	if(!(bloom->bloom=createBloom(bconfig.bloomLen,0.1)))
		die("cannot create bloom filter");
		
	//pthread_mutex_unlock(&lock);
}

void sendBloom(int lb, char * buffer, int blen){

	int i;
	for(i = 0; i < state.numLB; i++)
		if(state.lbs != NULL && state.lbs[i].ip != NULL 
				&& isWhatcher(lb, i, state.numLB)){
		
			/* TODO: FAZER SO 1 SOCKET por LB */
		
			if((ts = socket(AF_INET, SOCK_STREAM, 0)) == -1)
				die("thread socket");
	
			struct sockaddr_in lbaddr;
	
			memset((char *) &lbaddr, 0, sizeof(lbaddr));
			lbaddr.sin_family = AF_INET;
			lbaddr.sin_addr.s_addr=inet_addr(state.lbs[i].ip);
			lbaddr.sin_port=htons(bconfig.backPort);

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
				int w = write(ts, buffer, blen);
				if(w < 0)
					die("cannot write to LB");
			}
			
			close(ts);
			ts = -1;
		}
}


void *bloomThread(void *arg){
	time_t now;
	int i;

	// Check if to renew next time
	while(!end){	
		now = time(0);
		
		// Adicionar aqui && num_lb_now > num_lb_before
		if(state.blooms == NULL) {
			state.blooms = (LBBloom **) malloc(sizeof(LBBloom *)*state.numLB);
			state.toSend = (LBBloom **) malloc(sizeof(LBBloom *)*state.numLB);

			for(i = 0; i < state.numLB; i++){
				state.blooms[i] = (LBBloom *) malloc(sizeof(LBBloom));
				state.toSend[i] = NULL;

 				if(!(state.blooms[i]->bloom=createBloom(bconfig.bloomLen, 0.1)))
						die("cannot create bloom filter");

				state.blooms[i]->server = config.id;
				state.blooms[i]->lb = i;
				state.blooms[i]->packets = 0;
				state.blooms[i]->init = time(0);
			}
		}
		
		int blen = sizeof(char)*state.blooms[0]->bloom->bytes+3*sizeof(int);

		// Verifica os tempos de todos os blooms
		for(i = 0; i < state.numLB; i++)		
			if(state.blooms != NULL && state.blooms[i] != NULL 
				&& state.blooms[i]->init+config.renew < now){
					
					/*pthread_mutex_lock(&lock);
					char * buf = createBloomBuffer(state.blooms[i]);
					pthread_mutex_unlock(&lock);
					//printf("L0: %d | L1: %d | L2: %d\n", count[0], count[1], count[2]);
					sendBloom(i, buf, blen);
					resetBloom(state.blooms[i]);
					free(buf);*/

				pthread_mutex_lock(&lock);
				if(state.toSend[i] != NULL){

					char * buf = createBloomBuffer(state.toSend[i]);
					sendBloom(i, buf, blen);

					free(buf);

					memcpy(state.toSend[i]->bloom->bf, state.blooms[i]->bloom->bf, sizeof(char)*state.blooms[i]->bloom->bytes);
					state.toSend[i]->server = state.blooms[i]->server;
					state.toSend[i]->lb = state.blooms[i]->lb;
					state.toSend[i]->packets = state.blooms[i]->packets;
					state.toSend[i]->init = state.blooms[i]->init;

					resetBloom(state.blooms[i]);
				} else {
					//printf("2\n");

					//copy blooms[i] para toSend[i]
					state.toSend[i] = (LBBloom *) malloc(sizeof(LBBloom));

					if(!(state.toSend[i]->bloom=createBloom(bconfig.bloomLen, 0.1)))
							die("cannot create bloom filter");

					state.toSend[i]->server = state.blooms[i]->server;
					state.toSend[i]->lb = state.blooms[i]->lb;
					state.toSend[i]->packets = state.blooms[i]->packets;
					state.toSend[i]->init = state.blooms[i]->init;

					memcpy(state.toSend[i]->bloom->bf, state.blooms[i]->bloom->bf, sizeof(char)*state.blooms[i]->bloom->bytes);

					resetBloom(state.blooms[i]);
				}
				pthread_mutex_unlock(&lock);
				
			}
	}
	
	return NULL;
}

int isWhatcher(int lb, int whatcher, int num){
	int i;
	for(i = (lb+1)%num; i != (lb+(2*config.faults))%num ; i=(i+1)%num)
		if(whatcher == i)
			return 1;
	
	if(whatcher == i)		
		return 1;
		
	return 0;
	//return (lb+1)%num <= whatcher && (lb+(2*config.faults))%num >= whatcher;
}

char * createBloomBuffer(LBBloom *bloom){
	// LB:SERVER_ID:NUM_PACKETS:BLOOM
	// 4b:4b:4b:BLOOM_LEN
	int ilen = sizeof(int), blen = sizeof(char)*bloom->bloom->bytes;
	char * buffer = (char *) calloc(sizeof(char), ilen*3+blen+1);
	//printf("%d:%08X\n",bloom->lb, MurmurHash2(bloom->bloom->bf, blen, 0x00));
	//printf("%d: %d packets <<<<<<<<<<<<<<<<<\n", bloom->lb, bloom->packets);
	//count[bloom->lb]-=bloom->packets;
	printf("Sending %d to %d, %d with %d packets - %08X\n", bloom->lb, (bloom->lb+1)%3, (bloom->lb+2)%3, 
			bloom->packets, MurmurHash2(bloom->bloom->bf, blen, 0x00));
	memcpy(buffer, &(bloom->lb), ilen);
	memcpy(buffer+ilen, &(bloom->server), ilen);
	memcpy(buffer+ilen*2, &(bloom->packets), ilen);
	memcpy(buffer+ilen*3, bloom->bloom->bf, blen);
	buffer[ilen*3+blen] = '\0';
	return buffer;
}
