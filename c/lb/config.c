#include <math.h>
#include "config.h"

void addIptablesRules(){
	/* clear iptables */
	if (fork() == 0) {
		char *args[] = {"iptables", "-t", "raw", "-F", NULL};
		execvp("iptables", args);
	}
	
	/* add arp rule for front_end ff:ff:ff:ff:ff:ff */
	/*if(fork() == 0){
		char *args[] = {"arp", "-s", config.frontEnd, "ff:ff:ff:ff:ff:ff", NULL};
		execvp("arp", args);
	}*/
	
	sleep(1);
	
	/* iptables -vxL -t raw */

	/* add rules */
	int plen = floor(log10(abs(config.frontPort))) + 1;
	char port[plen+1];
	sprintf(port, "%d", config.frontPort);
	port[plen] = '\0';

	if (fork() == 0) {
		char *args[] = {"iptables", "-t", "raw", "-A", "PREROUTING", 
						"-p", "tcp", "--dport", port, "-d", 
						config.frontEnd, "-j", "NFQUEUE", 
						"--queue-num", "1", NULL};
		execvp("iptables", args);
	}
	
	sleep(1);
	
	plen = floor(log10(abs(config.helloPort))) + 1;
	char hport[plen+1];
	sprintf(hport, "%d", config.helloPort);
	port[plen] = '\0';
	
	if (fork() == 0) {
		char *args[] = {"iptables", "-t", "raw", "-A", "PREROUTING", 
						"-p", "udp", "--dport", hport, "-d", 
						config.frontEnd, "-j", "NFQUEUE", 
						"--queue-num", "1", NULL};
		execvp("iptables", args);
	}
}

/* resets the configs */
void clearConfigs(){
	/* State config */
	config.id = -1;
	config.wait = 2; //Default
	config.endPort = -1;
	config.frontPort = -1;
	config.helloPort = -1;
	config.frontEnd = NULL;
	
	/* Bloom Config */
	bconfig.bloomLen = 2500000; //Default;
	bconfig.backPort = -1;
		
	/* Faults Config */
	fconfig.faults = 1; //Default 1 fault tolerance
	fconfig.KILL_TH_SUSP = 100; //Default
	fconfig.KILL_TH_ASUSP = 10; //Default
	fconfig.TIMEOUT = 4; //Default
}

/* frees the memory */
void cleanConfigs(){
	if(config.frontEnd != NULL)
		free(config.frontEnd);
	config.frontEnd = NULL;
}

void readConfig(char *filename) {
	
	clearConfigs();
	clearState();

	char * line = NULL;
	size_t read = 0, len = 0;
	int i = 0;
	FILE * fp = fopen (filename, "r");

	if(fp == NULL)
		die("cannot open file");
	do{
		i++;
		read = getline(&line, &len, fp);
		if((int) read > 1)
			parseLine(line, i);
	}while((int) read >= 0);
	
	free(line);
	fclose(fp);
	
	if(config.id == -1)
		die("bad config file: need to define the load balancer id");
	
	if(config.frontPort == -1)
		die("bad config file: need to define the front end PORT");

	if(config.endPort == -1)
		die("bad config file: need to define the back end server PORT");

	if(config.helloPort == -1)
		die("bad config file: need to define the recovery PORT");

	if(config.frontEnd == NULL)
		die("bad config file: need to define the front end IP");
		
	if(bconfig.backPort == -1)
		die("bad config file: need to define the second channel PORT");
		
	/*if((2*fconfig.faults+1) > state.numLB){
		char init[] = "bad config file: need at least ";		
		char end[] = " load balancers";
		int nlb = 2*fconfig.faults+1;
		int lblen = floor(log10(abs(nlb))) + 1;
		
		char msg[strlen(init)+lblen+strlen(end)+1];
		strncpy(msg, init, strlen(init));
		sprintf(msg+strlen(init), "%d", nlb);
		strncpy(msg+strlen(init)+lblen, end, strlen(end));
		msg[strlen(init)+lblen+strlen(end)] = '\0';
		
		die(msg);
	}*/
}

void parseLine(char *line, int l){
	
	char * current = line;
	while(current[0] == ' ' || current[0] == '\t')
		current++;
		
	if(current[0] == '\0' || current[0] == '\n' || current[0] == '\r')
		return;
	
	if(current[0] == '#')
		return;
	else if(current[0] == '<'){
		if(strncmp(current+1, "SERVERS", 7) == 0){
			current+=9;
			
			int i = 0;
			while(current[i] != ' ' && current[i] != '\0')
				i++;
			current[i] = '\0';
			
			if(config.endPort == -1)
				config.endPort = atoi(current);
	
			current+=i+1;
			i=0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
				
			current[i] = '\0';
			if(state.numServers == -1){
				state.numServers = atoi(current);
				
				state.servers = (Server *) malloc(sizeof(Server)*state.numServers);
				
				for(i = 0; i < state.numServers; i++){
					state.servers[i].ip = NULL;
					state.servers[i].id = i;
				}
					
				return;
			}
			
			die("bad config file: servers already defined");
		}
		else if(strncmp(current+1, "BALANCERS", 9) == 0) {
			current+=11;
			
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			
			if(state.numLB == -1){
				state.numLB = atoi(current);
				
				state.lbs = (LoadBalancer *) malloc(sizeof(LoadBalancer)*state.numLB);
				int t = 0;
				for(t = 0; t < SEARCH_THREADS_NUM; t++){
					state.list[t] = (HttpRequestNode **) malloc(sizeof(HttpRequestNode *)*state.numLB);
					state.tail[t] = (HttpRequestNode **) malloc(sizeof(HttpRequestNode *)*state.numLB);
				}
				
				for(i = 0; i < state.numLB; i++){
					state.lbs[i].ip = NULL;
					state.lbs[i].id = i;
					for(t = 0; t < SEARCH_THREADS_NUM; t++){
						state.list[t][i] = NULL;
						state.tail[t][i] = NULL;
					}
				}

				state.susp = (int *) calloc(sizeof(int), state.numLB);
				state.asusp = (int *) calloc(sizeof(int), state.numLB);
				state.restart = (int *) calloc(sizeof(int), state.numLB);
												
				return;
			}
			
			die("bad config file: balancers already defined");
		}
		else if(strncmp(current+1, "BAL", 3) == 0) {
			if(state.numLB < 0)
				die("bad config file: need to define the balancers number first");
			
			current+=5;
			
			int i = 0;
			while(current[i] != ' ' && current[i] != '\0')
				i++;
			current[i] = '\0';
			int id = atoi(current);
			
			if(id >= state.numLB)
				die("bad config file: id >= number of load balancers");
			
			if(state.lbs[id].ip != NULL)
				die("bad config file: balancer already defined");
			
			current+=i+1;
			i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			
			state.lbs[id].ip = (char *) malloc(sizeof(char)*(i+1));
			strncpy(state.lbs[id].ip, current, i);
			state.lbs[id].ip[i] = '\0';
			return;
		}
		else if(strncmp(current+1, "SER", 3) == 0) {
			if(state.numServers == -1)
				die("bad config file: need to define the servers number first");
			
			current+=5;
			
			int i = 0;
			while(current[i] != ' ' && current[i] != '\0')
				i++;
			current[i] = '\0';
			int id = atoi(current);
			
			if(id >= state.numServers)
				die("bad config file: id >= number of servers");
			
			if(state.servers[id].ip != NULL)
				die("bad config file: server already defined");
			
			current+=i+1;
			i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			
			state.servers[id].ip = (char *) malloc(sizeof(char)*(i+1));
			strncpy(state.servers[id].ip, current, i);
			state.servers[id].ip[i] = '\0';
			
			return;
		}
		else if(strncmp(current+1, "WAIT", 4) == 0) {
			current+=6;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			config.wait = atoi(current);
			return;
		}
		else if(strncmp(current+1, "TIMEOUT", 7) == 0) {
			current+=9;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			fconfig.TIMEOUT = atoi(current);
			return;
		}
		else if(strncmp(current+1, "ID", 2) == 0) {
			current+=4;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			config.id = atoi(current);
			return;
		}
		else if(strncmp(current+1, "BLEN", 4) == 0) {
			current+=6;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			bconfig.bloomLen = atoi(current);
			return;
		}
		else if(strncmp(current+1, "ASUSP", 5) == 0) {
			current+=7;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			fconfig.KILL_TH_ASUSP = atoi(current);
			return;
		}
		else if(strncmp(current+1, "SUSP", 4) == 0) {
			current+=6;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			fconfig.KILL_TH_SUSP = atoi(current);
			return;
		}
		else if(strncmp(current+1, "FRONTPORT", 9) == 0) {
			current+=11;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			config.frontPort = atoi(current);
			return;
		}
		else if(strncmp(current+1, "FRONTEND", 8) == 0) {
			current+=10;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			
			config.frontEnd = (char *) malloc(sizeof(char)*(i+1));
			strncpy(config.frontEnd, current, i);
			config.frontEnd[i] = '\0';

			return;
		}
		else if(strncmp(current+1, "FAULTS", 6) == 0) {
			current+=8;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			
			fconfig.faults = atoi(current);
			return;

		}
		else if(strncmp(current+1, "BPORT", 5) == 0) {
			current+=7;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';

			bconfig.backPort = atoi(current);
			return;

		}
		else if(strncmp(current+1, "HPORT", 5) == 0) {
			current+=7;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';

			config.helloPort = atoi(current);
			return;

		}
	}

	char error[30];
	error[29] = '\0';
	strncpy(error, "bad config file on line ", 24);
	sprintf(error+24, "%d", l); 
	die(error);
}
