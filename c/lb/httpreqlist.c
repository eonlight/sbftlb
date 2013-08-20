#include <time.h>

#include "httpreqlist.h"

//cleans a list
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

// devolve o proximo
HttpRequestNode * removeFromList(HttpRequestNode * current){

	if(current == NULL || current->buffer == NULL)
		return NULL;
	
	// remove da lista
	if(current->prev != NULL)
		current->prev->next = current->next;
		
	if(current->next != NULL)
		current->next->prev = current->prev;

	HttpRequestNode *aux = current->next;

	// liberta o node
	if(current->buffer != NULL)
		free(current->buffer);
	current->buffer = NULL;
	
	if(current != NULL)
		free(current);
	current = NULL;
	
	return aux;
}

//create a HttpReqNode
HttpRequestNode * createNode(int len, char *buffer){
	HttpRequestNode *current = (HttpRequestNode *) malloc(sizeof(HttpRequestNode));
	current->buffer = (char *) malloc (sizeof(char)*(len+1));
	memcpy(current->buffer, buffer, len);
	current->buffer[len] = '\0';

	current->added = time(0);
	current->len = len;
	current->blooms = 0;
	//current->bag = bags;
	//current->server = server;

	current->next = NULL;
	current->prev = NULL;

	return current;
}

// adds to the last a node
void addToList(HttpRequestNode *last, HttpRequestNode *node){
	if(last != NULL)
		last->next = node;
	node->prev = last;
}
