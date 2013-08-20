#ifndef BAGGER_H
#define BAGGER_H

#define UDP_PROTO 17
#define TCP_PROTO 6

#define SIZE_ETHERNET 14
#define SNAP_LEN 1516

pcap_t * handle = NULL;	
char filter[1024];
struct bpf_program fp; //compiled filter

int main(int argc, char **argv);
void handlePacket(u_char *args, const struct pcap_pkthdr *header, const u_char *packet);
void terminate(int sig);
void die(char *error);

#endif