#include "algorithm.h"

#include "config.h"
#include "hash.c"

#include "bloom.c"

#define LBSEED 0x6a5b223d

//protocols ID
#define HELLO_LB_PROTO 1
#define RESET_LB_PROTO 2
#define HELLO_SERV_PROTO 3

struct sockaddr_in source, lbaddr;

typedef struct {
	Bloom *bloom;
	int packets; //num packets on bloom
	time_t init;
	int lb; //lb id
	int server; //server id
} LBBloom;

LBBloom **active = NULL;
LBBloom **toSend = NULL;

int *faulty = NULL;

pthread_t thread;
pthread_mutex_t lock;

int new_lb_num = LB_NUM;
char **lbs;

int **votes = NULL;

#include "thread.c"

int selectLB(struct iphdr *iph, struct tcphdr *tcph);

//hello protocol
void sendHelloMessage();

void initAlgorithm(){

	printf("initAlgorithm: Creating rules...\n");
	//set broadcast ip
	char buffer[1024];
	sprintf(buffer, "arp -s %s ff:ff:ff:ff:ff:ff", HELLO_ADDR);
	if(system(buffer) == -1)
		die("algorithm - initAlgorithm: set broadcast address failed");

	//sprintf(filter, "tcp port %d and dst %s or udp port %d and dst %s", SERVER_PORT, HELLO_PORT, HELLO_ADDR);
	sprintf(filter, "tcp dst port %d or ( udp port %d and dst %s )", SERVER_PORT, HELLO_PORT, HELLO_ADDR);

	printf("initAlgorithm: Creating structs...\n");
	active = (LBBloom **) malloc(sizeof(LBBloom *)*LB_NUM);
	toSend = (LBBloom **) malloc(sizeof(LBBloom *)*LB_NUM);
	votes = (int **) malloc(sizeof(int *)*LB_NUM);

	int lb = 0;
	for(lb = 0; lb < LB_NUM; lb++){
		active[lb] = (LBBloom *) malloc(sizeof(LBBloom));
		toSend[lb] = (LBBloom *) malloc(sizeof(LBBloom));
		votes[lb] = (int *) calloc(sizeof(int), LB_NUM);

		if(!(active[lb]->bloom=createBloom(BLOOM_LEN, BLOOM_ERROR)))
			die("algorithm - initAlgorithm: cannot create bloom filter");
		active[lb]->server = ID;
		active[lb]->lb = lb;
		active[lb]->packets = 0;
		active[lb]->init = time(0);

		if(!(toSend[lb]->bloom=createBloom(BLOOM_LEN, BLOOM_ERROR)))
			die("algorithm - initAlgorithm: cannot create bloom filter");
		toSend[lb]->server = ID;
		toSend[lb]->lb = lb;
		toSend[lb]->packets = 0;
		toSend[lb]->init = time(0);
	}

	lbs = (char **) malloc(sizeof(char *)*LB_NUM);
	for(lb = 0; lb < LB_NUM; lb++){
		lbs[lb] = (char *) malloc(sizeof(char)*(strlen(LBS[lb])+1));
		strncpy(lbs[lb], LBS[lb], strlen(LBS[lb]));
		lbs[lb][strlen(LBS[lb])] = '\0';
	}

	faulty = (int *) calloc(sizeof(int), LB_NUM);

	memset((char *) &lbaddr, 0, sizeof(lbaddr));
	lbaddr.sin_family = AF_INET;
	memset(&source, 0, sizeof(source));

	if (pthread_mutex_init(&lock, NULL) != 0)
        die("algorithm - initAlgorithm: mutex init failed");

	//init thread
	if(pthread_create(&thread, NULL, bloomManager, NULL) == -1)
		die("algorithm - initAlgorithm: unable to create thread");
}

void addNewLB(int id, char *ip){


	int *new_faulty = (int *) calloc(sizeof(int), LB_NUM+1);

	memcpy(new_faulty, faulty, sizeof(int)*LB_NUM);
	free(faulty);
	faulty = new_faulty;

	//active + toSend
	int **new_votes = (int **) malloc(sizeof(int *)*(LB_NUM+1));
	LBBloom **new_active = (LBBloom **) malloc(sizeof(LBBloom *)*(LB_NUM+1));
	LBBloom **new_toSend = (LBBloom **) malloc(sizeof(LBBloom *)*(LB_NUM+1));

	pthread_mutex_lock(&lock);

	memcpy(new_votes, votes, sizeof(int *)*LB_NUM);
	memcpy(new_active, active, sizeof(LBBloom *)*LB_NUM);
	memcpy(new_toSend, toSend, sizeof(LBBloom *)*LB_NUM);

	new_active[LB_NUM] = (LBBloom *) malloc(sizeof(LBBloom));
	new_toSend[LB_NUM] = (LBBloom *) malloc(sizeof(LBBloom));
	new_votes[LB_NUM] = (int *) calloc(sizeof(int), LB_NUM+1);
	
	free(votes);
	votes = new_votes;
	free(active);
	active = new_active;
	free(toSend);
	toSend = new_toSend;

	if(!(active[LB_NUM]->bloom=createBloom(BLOOM_LEN, BLOOM_ERROR)))
			die("algorithm - initAlgorithm: cannot create bloom filter");
	active[LB_NUM]->server = ID;
	active[LB_NUM]->lb = LB_NUM;
	active[LB_NUM]->packets = 0;
	active[LB_NUM]->init = time(0);

	if(!(toSend[LB_NUM]->bloom=createBloom(BLOOM_LEN, BLOOM_ERROR)))
		die("algorithm - initAlgorithm: cannot create bloom filter");
	toSend[LB_NUM]->server = ID;
	toSend[LB_NUM]->lb = LB_NUM;
	toSend[LB_NUM]->packets = 0;
	toSend[LB_NUM]->init = time(0);

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
	faulty[id] = 0;

	int i;
	for(i = 0; i < LB_NUM; i++)
		votes[id][i] = 0;

	pthread_mutex_lock(&lock);
	resetBloom(active[id]);
	resetBloom(toSend[id]);
	pthread_mutex_unlock(&lock);
}

//0..LB_NUM for lb
//LB_NUM..LB_NUM+SERVER_NUM for server
int getID(char *ip){
	int i, id = -1;
	for(i = 0; i < LB_NUM && id == -1; i++)
		if(strcmp(ip, lbs[i]) == 0)
			id = i;
	return id;
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
	for(i = 0; i < LB_NUM; i++)
		if(votes[id][i] == 0 && i != id)
			return 0;
	return 1;
}

void voteFaulty(int lb){
	int rs = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	struct sockaddr_in all;
	all.sin_family = AF_INET;
	all.sin_addr.s_addr = inet_addr(HELLO_ADDR);
	all.sin_port = htons(HELLO_PORT);
		
	int type = RESET_LB_PROTO;
	char reset[sizeof(int)*2+1];
	memcpy(reset, &type, sizeof(int));
	memcpy(reset+sizeof(int), &lb, sizeof(int));
	reset[sizeof(int)*2] = '\0';
		
	sendto(rs, (const char*) reset, sizeof(int)*2, 0, (struct sockaddr *)&all, sizeof(struct sockaddr_in));

	close(rs);
}

void verifyFaults(int id){

	//i received from f+1 BCV: then sendVote
	if(consideredFaulty(id))
		voteFaulty(id);
	//if all vote yes mark faulty
	else if(allFaulty(id)){
		printf("LB%d removed from the algorithm.\n", id);
		faulty[id] = 1;
	}
}

void handleUPDPacket(struct iphdr *iph, struct udphdr *udph, char *payload){
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

		default: break;
	};
}

void handleTCPPacket(struct iphdr *iph, struct tcphdr *tcph){
	//check which lb
	int lb = selectLB(iph, tcph);

	while(faulty[lb])
		lb = (lb+1)%LB_NUM;

	//add to the bag
	pthread_mutex_lock(&lock);
	addToBloom(active[lb]->bloom, (char *) iph, ntohs(iph->tot_len));
	active[lb]->packets++;
	pthread_mutex_unlock(&lock);
}

void handleOtherProto(struct iphdr *iph, char *ipPayload){
	// NOT SUPPOSED TO BE HERE
	// NOTHING TO DO, JUST DROP
}

void cleanAlgorithm(){
	printf("cleanAlgorithm: cleaning iptables rules...\n");
	char buffer[1024];
	sprintf(buffer, "iptables -t raw -F");
	if(system(buffer) == -1)
		die("algorithm - addIptablesRules: failed to clean iptables");

	pthread_mutex_lock(&lock);
	int lb;
	for(lb = 0; lb < LB_NUM; lb++){
		if(active[lb] != NULL){
			if(active[lb]->bloom != NULL)
				destroyBloom(active[lb]->bloom);
			active[lb]->bloom = NULL;
			free(active[lb]);
		}
		active[lb] = NULL;

		if(toSend[lb] != NULL){
			if(toSend[lb]->bloom != NULL)
				destroyBloom(toSend[lb]->bloom);
			toSend[lb]->bloom = NULL;
			free(toSend[lb]);
		}
		toSend[lb] = NULL;

		free(lbs[lb]);
		lbs[lb] = NULL;
		free(votes[lb]);
		votes[lb] = NULL;
	}
	free(lbs);
	lbs = NULL;
	free(votes);
	votes = NULL;
	free(active);
	active = NULL;
	free(toSend);
	toSend = NULL;
	pthread_mutex_unlock(&lock);

	cleanThread();
	pthread_mutex_destroy(&lock);
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