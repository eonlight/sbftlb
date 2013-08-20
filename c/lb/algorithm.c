#include <arpa/inet.h> // inet_to... , sockets
#include <unistd.h> //socket close

#include "algorithm.h"
#include "httpreqlist.c"

#include "config.h"
#include "hash.c"

int new_lb_num = LB_NUM;
int new_server_num = SERVER_NUM;

struct sockaddr_in source, to;
//socket to send the packet to the server
int s = -1;

//faults [lb]
int *susp = NULL, *asusp = NULL, *faulty = NULL, *wildcard = NULL, *packets = NULL;

int **votes = NULL;

char **lbs = NULL, **servers = NULL;

//list with the packets: [lb][server][packet_num]
HttpRequestNode ***list = NULL, ***tail = NULL;

pthread_t thread;
pthread_mutex_t lock;

//seeds fot the hashs
#define SSEED 0x45d5b4a4
#define LBSEED 0x6a5b223d

//protocols ID
#define HELLO_LB_PROTO 1
#define RESET_LB_PROTO 2
#define HELLO_SERV_PROTO 3

int amWhatcher(int lb);
int selectLB(struct iphdr *iph, struct tcphdr *tcph);
void sendPacket(struct iphdr * iph, struct tcphdr * tcph, int server);

//hello protocol
void sendHelloMessage();
void addIptablesRules();

#include "thread.c"

void initAlgorithm(){

	if(ID == -1)
		die("algorithm - initAlgorithm: Configure your ID at config.h.");

	memset(&source, 0, sizeof(source));
	to.sin_family = AF_INET;

	addIptablesRules();
	sendHelloMessage();

	printf("initAlgorithm: Creating sockets...\n");
	// create a socket
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_TCP)) == -1)
		die("algorithm - initAlgorithm: creating socket");
	int on = 1;
	// set socket options to send packets
	if(setsockopt(s, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) == -1)
        die("algorithm - initAlgorithm: setsockopt IP");

    printf("initAlgorithm: Initializing structures...\n");
    list = (HttpRequestNode ***) malloc(sizeof(HttpRequestNode **)*LB_NUM); 
	tail = (HttpRequestNode ***) malloc(sizeof(HttpRequestNode **)*LB_NUM);
	votes = (int **) malloc(sizeof(int *)*LB_NUM);

	int lb, server;
	for(lb = 0; lb < LB_NUM; lb++){
		list[lb] = (HttpRequestNode **) malloc(sizeof(HttpRequestNode *)*SERVER_NUM);
		tail[lb] = (HttpRequestNode **) malloc(sizeof(HttpRequestNode *)*SERVER_NUM);
		votes[lb] = (int *) calloc(sizeof(int), LB_NUM+SERVER_NUM);
		for(server = 0; server < SERVER_NUM; server++){
			list[lb][server] = NULL;
			tail[lb][server] = NULL;
		}
	}

	susp = (int *) calloc(sizeof(int), LB_NUM);
	asusp = (int *) calloc(sizeof(int), LB_NUM);
	faulty = (int *) calloc(sizeof(int), LB_NUM);
	packets = (int *) calloc(sizeof(int), LB_NUM);
	wildcard = (int *) calloc(sizeof(int), LB_NUM);

	lbs = (char **) malloc(sizeof(char *)*LB_NUM);
	for(lb = 0; lb < LB_NUM; lb++){
		lbs[lb] = (char *) malloc(sizeof(char)*(strlen(LBS[lb])+1));
		strncpy(lbs[lb], LBS[lb], strlen(LBS[lb]));
		lbs[lb][strlen(LBS[lb])] = '\0';
	}

	servers = (char **) malloc(sizeof(char *)*SERVER_NUM);
	for(server = 0; server < SERVER_NUM; server++){
		servers[server] = (char *) malloc(sizeof(char)*(strlen(SERVERS[server])+1));
		strncpy(servers[server], SERVERS[server], strlen(SERVERS[server]));
		servers[server][strlen(SERVERS[server])] = '\0';
	}

	if (pthread_mutex_init(&lock, NULL) != 0)
        die("algorithm - initAlgorithm: mutex init failed");

	if(pthread_create(&thread, NULL, bloomChecker, NULL) == -1)
		die("unable to create thread");
}

void addNewLB(int id, char *ip){

	int *new_susp = (int *) calloc(sizeof(int), LB_NUM+1);
	int *new_asusp = (int *) calloc(sizeof(int), LB_NUM+1);
	int *new_faulty = (int *) calloc(sizeof(int), LB_NUM+1);
	int *new_packets = (int *) calloc(sizeof(int), LB_NUM+1);
	int *new_wildcard = (int *) calloc(sizeof(int), LB_NUM+1);

	memcpy(new_susp, susp, sizeof(int)*LB_NUM);
	free(susp);
	susp = new_susp;
	memcpy(new_asusp, asusp, sizeof(int)*LB_NUM);
	free(asusp);
	asusp = new_asusp;
	memcpy(new_faulty, faulty, sizeof(int)*LB_NUM);
	free(faulty);
	faulty = new_faulty;
	memcpy(new_packets, packets, sizeof(int)*LB_NUM);
	free(packets);
	packets = new_packets;
	free(wildcard);
	memcpy(new_wildcard, wildcard, sizeof(int)*LB_NUM);
	wildcard = new_wildcard;

	HttpRequestNode ***new_list = (HttpRequestNode ***) malloc(sizeof(HttpRequestNode **)*(LB_NUM+1)); 
	HttpRequestNode ***new_tail = (HttpRequestNode ***) malloc(sizeof(HttpRequestNode **)*(LB_NUM+1));
	int **new_votes = (int **) malloc(sizeof(int *)*(LB_NUM+1));

	//list + tail + lbs
	pthread_mutex_lock(&lock);
	memcpy(new_list, list, sizeof(HttpRequestNode **)*LB_NUM);
	memcpy(new_tail, tail, sizeof(HttpRequestNode **)*LB_NUM);
	memcpy(new_votes, votes, sizeof(int *)*LB_NUM);

	new_list[LB_NUM] = (HttpRequestNode **) malloc(sizeof(HttpRequestNode *)*SERVER_NUM);
	new_tail[LB_NUM] = (HttpRequestNode **) malloc(sizeof(HttpRequestNode *)*SERVER_NUM);
	new_votes[LB_NUM] = (int *) calloc(sizeof(int), LB_NUM+1+SERVER_NUM);
	
	int server;
	for(server = 0; server < SERVER_NUM; server++){
		list[LB_NUM][server] = NULL;
		tail[LB_NUM][server] = NULL;
	}

	free(list);
	list = new_list;
	free(tail);
	tail = new_tail;
	free(votes);
	votes = new_votes;

	char **new_lbs = (char **) malloc(sizeof(char *) * (LB_NUM+1));
	memcpy(new_lbs, lbs, sizeof(char *)*LB_NUM);
	free(lbs);
	lbs = new_lbs;
	lbs[LB_NUM] = (char *) malloc(sizeof(char)*(strlen(ip)+1));
	strncpy(lbs[LB_NUM], ip, strlen(ip));
	lbs[LB_NUM][strlen(ip)] = '\0';

	new_lb_num++;

#undef LB_NUM
#define LB_NUM new_lb_num

	pthread_mutex_unlock(&lock);

	printf("Added new LB: %d\n", new_lb_num-1);

}

void cleanLB(int id){
	//TODO: clean bags on server

	susp[id] = 0;
	asusp[id] = 0;
	faulty[id] = 0;
	//count packets on list;
	wildcard[id] -= packets[id];
	packets[id] = 0;

	int server, i;
	for(i = 0; i < LB_NUM+SERVER_NUM; i++)
		votes[id][i] = 0;

	pthread_mutex_lock(&lock);
	for(server = 0; server < SERVER_NUM; server++){
		cleanList(list[id][server]);
		list[id][server] = NULL;
		tail[id][server] = NULL;
	}
	pthread_mutex_unlock(&lock);
}

//0..LB_NUM for lb
//LB_NUM..LB_NUM+SERVER_NUM for server
int getID(char *ip){
	int i, id = -1;
	for(i = 0; i < LB_NUM && id == -1; i++)
		if(strcmp(ip, lbs[i]) == 0)
			id = i;
	for(i = 0; i < SERVER_NUM && id == -1; i++)
		if(strcmp(ip, servers[i]) == 0)
			id = i;
	return id;
}

int isWhatcher(int lb, int whatcher){
	int i;
	for(i = (lb+1)%LB_NUM; i != (lb+(2*FAULTS))%LB_NUM ; i=(i+1)%LB_NUM)
		if(whatcher == i)
			return 1;
	
	return whatcher == i;
}

//if f+1 BCV vote
int consideredFaulty(int id){
	int i, total = 0;
	for(i = 0; i < LB_NUM; i++)
		if(isWhatcher(id, i) && votes[id][i] == 1)
			total++;
	return total >= FAULTS+1;
}

int allFaulty(int id){
	int i;
	for(i = 0; i < LB_NUM+SERVER_NUM; i++)
		if(votes[id][i] == 0 && i != id)
			return 0;
	return 1;
}

void verifyFaults(int id){

	//if i am BCV || i received from f+1 BCV: then sendVote again
	if(amWhatcher(id) || consideredFaulty(id))
		voteFaulty(id);
	//if all vote yes mark faulty
	else if(allFaulty(id)){
		//if me faulty, reset
		//if(id == ID)
		//	reset(); -> send hello
		//else
		printf("LB%d removed from the algorithm.\n", id);
		faulty[id] = 1;
	}
}

void addNewServer(int id, char *ip){
	//lists + tails + votes + servers

	pthread_mutex_lock(&lock);

	int lb;
	for(lb = 0; lb < LB_NUM; lb++){
		HttpRequestNode ** new_list = (HttpRequestNode **) malloc(sizeof(HttpRequestNode *)*(SERVER_NUM+1));
		HttpRequestNode ** new_tail = (HttpRequestNode **) malloc(sizeof(HttpRequestNode *)*(SERVER_NUM+1));
		int * new_votes = (int *) calloc(sizeof(int), LB_NUM+SERVER_NUM+1);

		memcpy(new_list, list[lb], sizeof(HttpRequestNode *)*(SERVER_NUM));
		memcpy(new_tail, tail[lb], sizeof(HttpRequestNode *)*(SERVER_NUM));
		memcpy(new_votes, votes[lb], sizeof(int)*(SERVER_NUM+LB_NUM));
		
		new_list[LB_NUM] = NULL;
		new_tail[LB_NUM] = NULL;

		free(list[lb]);
		list[lb] = new_list;
		free(tail[lb]);
		tail[lb] = new_tail;
		free(votes[lb]);
		votes[lb] = new_votes;
	}

	char **new_servers = (char **) malloc(sizeof(char *) * (SERVER_NUM+1));
	memcpy(new_servers, servers, sizeof(char *)*SERVER_NUM);
	free(servers);
	servers = new_servers;
	servers[SERVER_NUM] = (char *) malloc(sizeof(char)*(strlen(ip)+1));
	strncpy(servers[SERVER_NUM], ip, strlen(ip));
	servers[SERVER_NUM][strlen(ip)] = '\0';

	new_server_num++;

#undef SERVER_NUM
#define SERVER_NUM new_server_num

	pthread_mutex_unlock(&lock);

	printf("Added new Server: %d\n", new_lb_num-1);

}

int handleUPDPacket(struct iphdr *iph, struct udphdr *udph, char *payload){

    char * pl = (char *) iph + iph->ihl*4 + sizeof(udph);
    	
    int type, id, voter;
    memcpy(&type, pl, sizeof(int));
	memcpy(&id, pl+sizeof(int), sizeof(int));

    source.sin_addr.s_addr = iph->saddr;
    char *from = inet_ntoa(source.sin_addr);

	switch(type){
		case HELLO_LB_PROTO:
			if(id == LB_NUM)
				addNewLB(id, from);
			else
				cleanLB(id);
		break;
		case RESET_LB_PROTO:
			if(faulty[id] == 0){
				voter = getID(from);
				if(voter != -1)
					votes[id][voter] = 1;
				verifyFaults(id);
			}
		break;
		case HELLO_SERV_PROTO:
			if(id == SERVER_NUM)
				addNewServer(id, from);
			//else //ignore
		break;
		default: break;
	};

	return DEFAULT_RULE;
}

void sendPacket(struct iphdr * iph, struct tcphdr * tcph, int server){

	to.sin_addr.s_addr = inet_addr(servers[server]);
	to.sin_port = htons(SERVER_PORT);
	
	if(sendto(s, (char *) iph, ntohs(iph->tot_len), 0, (struct sockaddr *)&to, sizeof(to)) < 0)
		die("algorithm - sendPacket: send");

}

int handleTCPPacket(struct iphdr *iph, struct tcphdr *tcph){
	int lb = selectLB(iph, tcph);

	while(faulty[lb])
		lb = (lb+1)%LB_NUM;

	int server = selectServer(iph, tcph);

	if(lb == ID)
		sendPacket(iph, tcph, server);
	//else if(amWhatcher(lb)){
	/*	packets[lb]++;
		HttpRequestNode * node = createNode(ntohs(iph->tot_len), (char *) iph);
		
		pthread_mutex_lock(&lock);
		addToList(tail[lb][server], node);
		tail[lb][server] = node;
		if(list[lb][server] == NULL)
			list[lb][server] = tail[lb][server];
		pthread_mutex_unlock(&lock);*/
	//}

	return DEFAULT_RULE;
}

int handleOtherProto(struct iphdr *iph, char *ipPayload){
	// NOT SUPPOSED TO BE HERE
	// NOTHING TO DO, JUST DROP
	return DEFAULT_RULE;
}

void cleanAlgorithm(){

	printf("cleanAlgorithm: cleaning iptables rules...\n");
	char buffer[1024];
	sprintf(buffer, "iptables -t raw -F");
	if(system(buffer) == -1)
		die("algorithm - addIptablesRules: failed to clean iptables");

	printf("cleanAlgorithm: cleaning structs...\n");
	int lb, server;

	pthread_mutex_lock(&lock);
	for(lb = 0; lb < LB_NUM; lb++){
		for(server = 0; server < SERVER_NUM; server++){
			cleanList(list[lb][server]);
			list[lb][server] = NULL;
			tail[lb][server] = NULL;
		}
		free(lbs[lb]);
		lbs[lb] = NULL;
		free(votes[lb]);
		votes[lb] = NULL;
		free(list[lb]);
		list[lb] = NULL;
		free(tail[lb]);
		tail[lb] = NULL;
	}
	free(lbs);
	lbs = NULL;
	free(list);
	list = NULL;
	free(tail);
	tail = NULL;
	free(votes);
	votes = NULL;
	pthread_mutex_unlock(&lock);
	
	for(server = 0; server < SERVER_NUM; server++){
		free(servers[server]);
		servers[server] = NULL;
	}
	free(servers);
	servers = NULL;

	free(susp);
	susp = NULL;
	free(asusp);
	asusp = NULL;
	free(faulty);
	faulty = NULL;
	free(wildcard);
	wildcard = NULL;

	cleanThreads();
	pthread_mutex_destroy(&lock);

}

int selectServer(struct iphdr *iph, struct tcphdr *tcph){
	source.sin_addr.s_addr = iph->saddr;
	char *host = inet_ntoa(source.sin_addr);
	int port = ntohs(tcph->source);
	int port_len = floor(log10(abs(port))) + 1, host_len = strlen(host);
	char key[host_len+port_len+1];
	memcpy(key, host, host_len);
	sprintf(key+host_len, "%d", port);
	key[host_len+port_len] = '\0';
	int hash = abs(MurmurHash2(key, host_len+port_len , SSEED));
	return hash%SERVER_NUM;
}

int selectLB(struct iphdr *iph, struct tcphdr *tcph){
	source.sin_addr.s_addr = iph->saddr;
	char *host = inet_ntoa(source.sin_addr);
	int port = ntohs(tcph->source);
	int port_len = floor(log10(abs(port))) + 1, host_len = strlen(host);
	char key[host_len+port_len+1];
	memcpy(key, host, host_len);
	sprintf(key+host_len, "%d", port);
	key[host_len+port_len] = '\0';
	int hash = abs(MurmurHash2(key, host_len+port_len , LBSEED));
	return hash%LB_NUM;
}

int amWhatcher(int lb){
	int i;
	for(i = (lb+1)%LB_NUM; i != (lb+(2*FAULTS))%LB_NUM ; i=(i+1)%LB_NUM)
		if(ID == i) return 1;
	return ID == i;
}

void sendHelloMessage(){
	printf("sendHelloMessage: sending hello message...\n");

	int rs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	to.sin_addr.s_addr = inet_addr(HELLO_ADDR);
	to.sin_port = htons(HELLO_PORT);

	int type = HELLO_LB_PROTO;
	char hello[sizeof(int)*2+1];
	memcpy(hello, &type, sizeof(int));
	int id = ID;
	memcpy(hello+sizeof(int), &id, sizeof(int));
	hello[sizeof(int)*2] = '\0';

	sendto(rs, (const char*) hello, sizeof(int)*2, 0, (struct sockaddr *)&to, sizeof(struct sockaddr_in));
	close(rs);
}

void addIptablesRules(){
	// iptables -vxL -t raw
	printf("addIptablesRules: adding iptables rules...\n");

	//clean iptables
	char buffer[1024];
	sprintf(buffer, "iptables -t raw -F");
	if(system(buffer) == -1)
		die("algorithm - addIptablesRules: failed to clean iptables");

	//set broadcast ip
	sprintf(buffer, "arp -s %s ff:ff:ff:ff:ff:ff", HELLO_ADDR);
	if(system(buffer) == -1)
		die("algorithm - addIptablesRules: set broadcast address failed");

	//rule for the clients requests
	sprintf(buffer, "iptables -t raw -A PREROUTING -p tcp --dport %d -d %s -j NFQUEUE --queue-num 1", FRONT_PORT, FRONT_END);
	if(system(buffer) == -1)
		die("algorithm - addIptablesRules: failed to add rule in");

	//rules for the protocol
	sprintf(buffer, "iptables -t raw -A PREROUTING -p udp --dport %d -d %s -j NFQUEUE --queue-num 1", HELLO_PORT, HELLO_ADDR);
	if(system(buffer) == -1)
		die("algorithm - addIptablesRules: failed to add rule hello");
}