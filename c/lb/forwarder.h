#ifndef FORWARDER_H
#define FORWARDER_H

#define UDP_PROTO 17
#define TCP_PROTO 6

struct nfq_handle *h = NULL;
struct nfq_q_handle *qh = NULL;

int main(int argc, char **argv);
int handlePacket(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data);
void cleanNFQueue();
void terminate(int sig);
void die(char *error);

#endif