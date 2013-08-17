#ifndef MASK_H
#define MASK_H

#include <unistd.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h> // for NF_ACCEPT
#include <libnetfilter_queue/libnetfilter_queue.h>

#include <netinet/ip.h>	// Provides declarations for ip header
#include <netinet/tcp.h> // Provides declarations for tcp header

#include <string.h> // memset
#include <arpa/inet.h> // sockets

#include <time.h> // time(0)
#include <math.h>

#include "bloom.c"

#define UDP_PROTO 17
#define HELLO_SERV_PROTO 3
#define HELLO_LB_PROTO 1

typedef struct {
	Bloom *bloom;
	int packets; //num packets on bloom
	time_t init;
	int lb; //lb id
	int server; //server id
} LBBloom;

typedef struct {
	int id;
	char *ip;
} LoadBalancer;

typedef struct {
	int numLB;
	LoadBalancer *lbs;
	LBBloom **blooms;
	LBBloom **toSend;
} State;


// Thread vars
pthread_t thread;
pthread_mutex_t lock;
int end = 0;

// NFqueue structs
struct nfq_handle *h = NULL;
struct nfq_q_handle *qh = NULL;

State state;

int main(int argc, char **argv);
int handlePacket(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data);

void addNewLB(int id, char *ip);

void die(char *error);
void terminate(int sig);

void cleanState();
void clearState();

void addToFilter(char * buffer, int id, int size);

#endif
