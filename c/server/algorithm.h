#ifndef ALGORITHM_H
#define ALGORITHM_H

void initAlgorithm();
void handleUPDPacket(struct iphdr *iph, struct udphdr *udph, char *payload);
void handleTCPPacket(struct iphdr *iph, struct tcphdr *tcph);
void handleOtherProto(struct iphdr *iph, char *ipPayload);
void cleanAlgorithm();

#endif