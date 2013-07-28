#include "httpreqlist.h"
#include "forwarder.h"

void cleanList(HttpRequestNode *head){
	while(head != NULL){
		if(head->buffer != NULL)
			free(head->buffer);
		head->buffer = NULL;
		HttpRequestNode *aux = head->next;
		free(head);
		head = aux;
	}
}

/* devovler o proximo */
HttpRequestNode * removeFromList(HttpRequestNode * current){

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
	
	return aux;
}

// add to the head
void addNodeToList(int lb, HttpRequestNode *node){
	if(state.list[lb] == NULL){
		state.list[lb] = node;
		state.tail[lb] = node;
	}
	else {
		//current to the end
		HttpRequestNode *current = node;
		while(current->next != NULL)
			current = current->next;
		//head.prev = current
		state.list[lb]->prev = current;
		//current.next = head
		current->next = state.list[lb];
		//head = node
		state.list[lb] = node;
	}
}

// Add new node to tail
void addToList(int lb, int len, unsigned char * buffer, int server){

	if(state.list[lb] == NULL){
		state.list[lb] = (HttpRequestNode *) malloc(sizeof(HttpRequestNode));
		
		state.list[lb]->buffer = (char *) malloc (sizeof(char)*(len+1));
		memcpy(state.list[lb]->buffer, buffer, len);
		state.list[lb]->buffer[len] = '\0';

		state.list[lb]->next = NULL;
		state.list[lb]->prev = NULL;
		state.list[lb]->server = server;
		state.list[lb]->added = time(0);
		state.list[lb]->len = len;
		state.list[lb]->blooms = 0;

		//tail = head
		state.tail[lb] = state.list[lb];
	}
	else {
		HttpRequestNode *current = state.tail[lb];
		current->next = (HttpRequestNode *) malloc(sizeof(HttpRequestNode));

		current->next->buffer = (char *) malloc (sizeof(char)*(len+1));
		memcpy(current->next->buffer, buffer, len);
		current->next->buffer[len] = '\0';

		current->next->server = server;
		current->next->added = time(0);
		current->next->blooms = 0;
		current->next->len = len;

		current->next->prev = current;
		current->next->next = NULL;

		state.tail[lb] = current->next;
	}
}
