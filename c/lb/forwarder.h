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

#include "bloom.c"

char **bags;
int bs = 0;

typedef struct {
	int id;
	char *ip;
} Server;

typedef struct {
	int id;
	char *ip;
} LoadBalancer;

typedef struct {
	int numLB;
	int numServers;
	LoadBalancer *lbs;
	Server *servers;
	int *susp;
	int *asusp;
	HttpRequestNode **list;
	int *faulty; //tests only
	int *restart;
} State;

/* socket */
int list, s, err, on = 1;
int newts = -1;


/* thread end flag */
pthread_t thread;
pthread_mutex_t lock;
int end = 0;

/* Aux struct to resend */
struct sockaddr_in to;

State state;

int main(int argc, char **argv);
void cb(unsigned char * buffer, int recv_len);

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
