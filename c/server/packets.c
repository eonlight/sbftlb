/* Compute checksum for count bytes starting at addr */
static unsigned short compute_checksum(unsigned short *addr, unsigned int count) {
	register unsigned long sum = 0;
	while (count > 1) {
		sum += * addr++;
		count -= 2;
	}

	/* if any bytes left: pad the bytes and add */
	if(count > 0)
		sum += ((*addr)&htons(0xFF00));

	/* Fold sum to 16 bits: add carrier to result */
	while (sum>>16)
  		sum = (sum & 0xffff) + (sum >> 16);

  	sum = ~sum;
	return ((unsigned short)sum);
}

/* Set IP checksum of a given IP header */
void compute_ip_checksum(struct iphdr* iphdrp){
  iphdrp->check = 0;
  iphdrp->check = compute_checksum((unsigned short*)iphdrp, iphdrp->ihl<<2);
}

/* Set tcp checksum: given IP header and tcp segment */
void compute_tcp_checksum(struct iphdr *pIph, unsigned short *ipPayload) {
	register unsigned long sum = 0;
	unsigned short tcpLen = ntohs(pIph->tot_len) - (pIph->ihl<<2);
	struct tcphdr *tcphdrp = (struct tcphdr*)(ipPayload);
	
	/* Add the pseudo header */ 
	/* source ip */
	sum += (pIph->saddr>>16)&0xFFFF;
	sum += (pIph->saddr)&0xFFFF;
	/* dest ip */
	sum += (pIph->daddr>>16)&0xFFFF;
	sum += (pIph->daddr)&0xFFFF;
	/* protocol and reserved: 6 */
	sum += htons(IPPROTO_TCP);
	/* tcp length */
	sum += htons(tcpLen);

	/* clear tcp checksum */
	tcphdrp->check = 0;

	/* Add IP payload */
	while (tcpLen > 1) {
		sum += * ipPayload++;
		tcpLen -= 2;
	}

	/* if any bytes left: pad the bytes and add */
	if(tcpLen > 0)
		sum += ((*ipPayload)&htons(0xFF00));
  	
	/* Fold 32-bit sum to 16 bits: add carrier to result */
  	while (sum>>16)
  		sum = (sum & 0xffff) + (sum >> 16);
  		  	
	sum = ~sum;
	
	/* Set tcp checksum */
	tcphdrp->check = (unsigned short)sum;
}
