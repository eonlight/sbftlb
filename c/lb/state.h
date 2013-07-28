#ifndef STATE_H
#define STATE_H

typedef struct {
	int numLB;
	int numServers;
	LoadBalancer *lbs;
	Server *servers;
	int *susp;
	int *asusp;
	HttpRequestNode **list;
	HttpRequestNode **tail;
	int *restart;
} State;

// State of the LB
State state;

void addNewLB(int id, char *ip);
void addNewServer(int id, char *ip);

void clearState();
void resetState();
void cleanState();

#endif