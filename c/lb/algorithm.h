#ifndef ALGORITHM_H
#define ALGORITHM_H

void initAlgorithm();
int handleUPDPacket(struct iphdr *iph, struct udphdr *udph, char *payload);
int handleTCPPacket(struct iphdr *iph, struct tcphdr *tcph);
int handleOtherProto(struct iphdr *iph, char *ipPayload);
void cleanAlgorithm();

int selectServer(struct iphdr *iph, struct tcphdr *tcph);

#endif