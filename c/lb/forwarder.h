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
#define PROTO_LB_HELLO 1
#define PROTO_RESET 2
#define PROTO_SERV_HELLO 3

#include "bloom.c"

typedef struct {
	int id;
	char *ip;
} Server;

typedef struct {
	int id;
	char *ip;
} LoadBalancer;

// thread, end flag and locks
pthread_t thread;
pthread_mutex_t lock;
int end = 0;

#include "state.c"

// sockets and sockets vars
int s, err, on = 1;
int newts = -1;

// NFqueue structs 
struct nfq_handle *h = NULL;
struct nfq_q_handle *qh = NULL;

// Aux struct to resend
struct sockaddr_in to;

int main(int argc, char **argv);
void handleUDPProtocols(struct iphdr *iph, struct udphdr *udph, char * payload);
int handlePacket(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data);

int needRestart();
int isFaulty(int lb);
void markFaulty(int lb);
int amWhatcher(int lb, int num);

void sendPacket(struct iphdr * iph, struct tcphdr * tcph, unsigned char * buffer, int lb);

void die(char *error);
void terminate(int sig);

#endif
