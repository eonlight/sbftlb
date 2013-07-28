#include"httpreqlist.h"
#include "forwarder.h"

void cleanList(HttpRequestNode *head){
	pthread_mutex_lock(&lock);

	while(head != NULL){
		if(head->buffer != NULL)
			free(head->buffer);
		head->buffer = NULL;
		HttpRequestNode *aux = head->next;
		free(head);
		head = aux;
	}
	
	pthread_mutex_unlock(&lock);
}

/* devovler o proximo */
HttpRequestNode * removeFromList(HttpRequestNode * current){

	pthread_mutex_lock(&lock);

	if(current == NULL || current->buffer == NULL)
		return NULL;
	
	/* remove da lista */
	if(current->prev != NULL)
		current->prev->next = current->next;
		
	if(current->next != NULL)
		current->next->prev = current->prev;

	HttpRequestNode *aux = current->next;

	/* liberta o node */
	if(current->buffer != NULL)
		free(current->buffer);
	current->buffer = NULL;
	
	if(current != NULL)
		free(current);
	current = NULL;

	pthread_mutex_unlock(&lock);
	
	return aux;
}

void addToList(int lb, int len, unsigned char * buffer, int server){

	pthread_mutex_lock(&lock);

	if(state.list[lb] == NULL){
		state.list[lb] = (HttpRequestNode *) malloc(sizeof(HttpRequestNode));
		state.list[lb]->buffer = (char *) malloc (sizeof(char)*(len+1));
		memcpy(state.list[lb]->buffer, buffer, len);
		state.list[lb]->next = NULL;
		state.list[lb]->prev = NULL;
		state.list[lb]->server = server;
		state.list[lb]->added = time(0);
		state.list[lb]->len = len;
		state.list[lb]->buffer[len] = '\0';
	}
	else {

		HttpRequestNode *current = state.list[lb];
		state.list[lb]->prev = (HttpRequestNode *) malloc(sizeof(HttpRequestNode));

		current->prev->buffer = (char *) malloc (sizeof(char)*(len+1));
		memcpy(current->prev->buffer, buffer, len);
		current->prev->buffer[len] = '\0';

		current->prev->server = server;
		current->prev->added = time(0);
		current->prev->len = len;

		state.list[lb]->prev->next = state.list[lb];
		state.list[lb]->prev->prev = NULL;

		state.list[lb] = current->prev;

		/*
		HttpRequestNode *current = state.list[lb];
	
		while(current->next != NULL)
			current = current->next;
		
		current->next = (HttpRequestNode *) malloc(sizeof(HttpRequestNode));
		
		current->next->buffer = (char *) malloc (sizeof(char)*(len+1));
		memcpy(current->next->buffer, buffer, len);
		current->next->buffer[len] = '\0';
		
		current->next->server = server;
		
		current->next->added = time(0);
		current->next->len = len;
		
		current->next->next = NULL;
		current->next->prev = current;*/
	}
	
	pthread_mutex_unlock(&lock);
}
