#include "state.h"

void addNewServer(int id, char *ip){

	Server * newServers = (Server *) malloc(sizeof(Server)*(state.numServers+1));
	memcpy(newServers, state.servers, sizeof(Server)*state.numServers);
	Server * old = state.servers;

	/* Add new server data */
	newServers[state.numServers].ip = (char *) malloc(sizeof(char)*(strlen(ip)+1));
	memcpy(newServers[state.numServers].ip, ip, strlen(ip)*sizeof(char));
	newServers[state.numServers].ip[strlen(ip)] = '\0';
	newServers[state.numServers].id = id;

	/*replace and free old server list */
	state.servers = newServers;
	free(old);

	state.numServers++;

}

void addNewLB(int id, char *ip){
	int i;

	/* LBS data */
	LoadBalancer *newLBS = (LoadBalancer *) malloc(sizeof(LoadBalancer)*(state.numLB+1));
	memcpy(newLBS, state.lbs, sizeof(LoadBalancer)*state.numLB);
	LoadBalancer * oldLBS = state.lbs;
	state.lbs = newLBS;
	free(oldLBS);

	/* new LBS packets list */
	HttpRequestNode ** newList = (HttpRequestNode **) malloc(sizeof(HttpRequestNode *)*(state.numLB+1));
	HttpRequestNode ** newTail = (HttpRequestNode **) malloc(sizeof(HttpRequestNode *)*(state.numLB+1));

	for(i = 0; i < state.numLB; i++){
		newList[i] = state.list[i];
		newTail[i] = state.tail[i];
	}
	
	newList[i] = NULL;
	newTail[i] = NULL;

	/* Add data of new LBS */
	state.lbs[i].ip = (char *) malloc(sizeof(char)*(strlen(ip)+1));
	memcpy(state.lbs[i].ip, ip, strlen(ip)*sizeof(char));
	state.lbs[i].ip[strlen(ip)] = '\0';
	state.lbs[i].id = id;

	/* Free old list and tail */
	HttpRequestNode ** oldList = state.list;
	HttpRequestNode ** oldTail = state.tail;
	state.list = newList;
	state.tail = newTail;
	free(oldList);
	free(oldTail);

	/* Other vars */
	int * newSusp, * newAsusp, * newRestart;

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

	newRestart = (int *) calloc(sizeof(int), state.numLB+1);
	memcpy(newRestart, state.restart, state.numLB*sizeof(int));
	old = state.restart;
	state.restart = newRestart;
	free(old);

	state.numLB++;
}

void resetState() {
	int i;
	for(i = 0; i < state.numLB; i++){
		state.susp[i] = 0;
		state.asusp[i] = 0;

		pthread_mutex_lock(&lock);
		if(state.list[i] != NULL)
			cleanList(state.list[i]);
		state.list[i] = NULL;
		state.tail[i] = NULL;
		pthread_mutex_unlock(&lock);
		
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
	state.restart = NULL;
	state.tail = NULL;
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
			state.tail[i] = NULL;
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
	
	if(state.susp != NULL)
		free(state.susp);
	state.susp = NULL;
	
	if(state.asusp != NULL)
		free(state.asusp);
	state.asusp = NULL;
}