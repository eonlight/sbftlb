#ifndef FORWARDER_H
#define FORWARDER_H

#include <unistd.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h> /* for NF_ACCEPT */
#include <libnetfilter_queue/libnetfilter_queue.h>

#include <netinet/ip.h>	/* Provides declarations for ip header */
#include <netinet/tcp.h>	/* Provides declarations for tcp header */ 
#include <netinet/udp.h>	/* Provides declarations for udp header */ 

#include <string.h> /* memset */
#include <arpa/inet.h> /* sockets */

#include <time.h> /* time(0) */

#define UDP_PROTO 17
#define HELLO_SERV_PROTO 3
#define RESET_LB_PROTO 2
#define HELLO_LB_PROTO 1

#include "bloom.c"

typedef struct {
	int id;
	char *ip;
} Server;

typedef struct {
	int id;
	char *ip;
} LoadBalancer;

#define SEARCH_THREADS_NUM 10

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

/* socket */
int s, err, on = 1;
int newts = -1;


/* thread end flag */
pthread_t thread;
pthread_mutex_t lock; //primary list lock
pthread_attr_t attr;
int end = 0;

int mainID;

/* thread pool and vars to check httplist */
pthread_t searchThreads[SEARCH_THREADS_NUM]; // search threads
pthread_mutex_t searchLocks[SEARCH_THREADS_NUM]; //locks of the lists
HttpRequestNode *searchList[SEARCH_THREADS_NUM]; //list to check


/* NFqueue structs */
struct nfq_handle *h = NULL;
struct nfq_q_handle *qh = NULL;

/* Aux struct to resend */
struct sockaddr_in to;

State state;

int main(int argc, char **argv);
int cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data);

void addNewLB(int id, char *ip);
void addNewServer(int id, char *ip);

int needRestart();
int isFaulty(int lb);
void markFaulty(int lb);
int amWhatcher(int lb, int num);

void sendPacket(struct iphdr * iph, struct tcphdr * tcph, unsigned char * buffer, int lb);

void die(char *error);
void terminate(int sig);
void clearState();
void resetState();
void cleanState();

#endif
