#ifndef FORWARDER_H
#define FORWARDER_H

#include <unistd.h>
#include <netinet/in.h>
#include <linux/types.h>

#include <netinet/ip.h>	/* Provides declarations for ip header */
#include <netinet/tcp.h>	/* Provides declarations for tcp header */ 
#include <netinet/udp.h>	/* Provides declarations for udp header */ 

#include <string.h> /* memset */
#include <arpa/inet.h> /* sockets */

#include <time.h> /* time(0) */

#define UDP_PROTO 17
#define SIZE_ETHERNET 14
#define SNAP_LEN 1512

#include <pcap.h>

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
int s, err, on = 1;
int newts = -1;


/* thread end flag */
pthread_t thread;
pthread_mutex_t lock;
int end = 0;

/* NFqueue structs */
//struct nfq_handle *h = NULL;
//struct nfq_q_handle *qh = NULL;
pcap_t * handle = NULL;	
struct bpf_program fp; //compiled filter

/* Aux struct to resend */
struct sockaddr_in to;

State state;

int main(int argc, char **argv);
int cb(unsigned char * buffer, int recv_len);

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
