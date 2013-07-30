#include <math.h>
#include "config.h"

void parseConfigLine(char *line, int l){
	
	char * current = line;
	while(current[0] == ' ' || current[0] == '\t')
		current++;
		
	if(current[0] == '\0' || current[0] == '\n' || current[0] == '\r')
		return;
	
	if(current[0] == '#')
		return;
	else if(current[0] == '<'){
		if(strncmp(current+1, "BALANCERS", 9) == 0) {
			current+=11;
			
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			
			if(state.numLB == -1){
				state.numLB = atoi(current);
				
				state.lbs = (LoadBalancer *) malloc(sizeof(LoadBalancer)*state.numLB);

				for(i = 0; i < state.numLB; i++){
					state.lbs[i].id = i;
					state.lbs[i].ip = NULL;
				}
			
				return;
			}
			
			die("bad config file: balancers already defined");
		}
		else if(strncmp(current+1, "BAL", 3) == 0) {
			if(state.numLB == -1)
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
		else if(strncmp(current+1, "SERVERPORT", 10) == 0) {
			current+=12;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			config.serverPort = atoi(current);
			return;
		}
		else if(strncmp(current+1, "RENEW", 5) == 0) {
			current+=7;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			config.renew = atoi(current);
			return;
		}
		else if(strncmp(current+1, "WAIT", 4) == 0) {
			current+=2;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			config.wait = atoi(current);
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
		else if(strncmp(current+1, "BPORT", 5) == 0) {
			current+=7;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';

			bconfig.backPort = atoi(current);
			return;

		}
		else if(strncmp(current+1, "FAULTS", 6) == 0) {
			current+=8;
			int i = 0;
			while(current[i] != '>' && current[i] != '\0')
				i++;
			current[i] = '\0';
			
			config.faults = atoi(current);
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


void readConfig(char *filename){

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
			parseConfigLine(line, i);
	}while((int) read >= 0);
	
	free(line);
	fclose(fp);
	
	if(config.id == -1)
		die("bad config file: need to define the load balancer id");
		
	if(config.serverPort == -1)
		die("bad config file: need to define the server port");
		
	if(config.frontPort == -1)
		die("bad config file: need to define the front end port");
	
	if(bconfig.backPort == -1)
		die("bad config file: need to define the back port");
	
	if(config.frontEnd == NULL)
		die("bad config file: need to define the front end");
	
	if(config.helloPort == -1)
		die("bad config file: need to define the recovery PORT");

	if(state.blooms == NULL) {
		state.blooms = (LBBloom **) malloc(sizeof(LBBloom *)*state.numLB);
		state.toSend = (LBBloom **) malloc(sizeof(LBBloom *)*state.numLB);

		for(i = 0; i < state.numLB; i++){
			state.blooms[i] = (LBBloom *) malloc(sizeof(LBBloom));
			state.toSend[i] = NULL;

			if(!(state.blooms[i]->bloom=createBloom(bconfig.bloomLen, 0.1)))
				die("cannot create bloom filter");

			state.blooms[i]->server = config.id;
			state.blooms[i]->lb = i;
			state.blooms[i]->packets = 0;
			state.blooms[i]->init = time(0);
		}
	}

}

void addIptablesRules(){
	// clear iptables
	if (fork() == 0) {
		char *args[] = {"iptables", "-t", "raw", "-F", NULL};
		execvp("iptables", args);
	}
	
	if (fork() == 0) {
		char *args[] = {"iptables", "-t", "mangle", "-F", NULL};
		execvp("iptables", args);
	}

	/* add arp rule for front_end ff:ff:ff:ff:ff:ff */
	/*if(fork() == 0){
		char *args[] = {"arp", "-s", config.frontEnd, "ff:ff:ff:ff:ff:ff", NULL};
		execvp("arp", args);
	}*/
	
	// add rules
	int plen = floor(log10(abs(config.serverPort))) + 1;
	char port[plen+1];
	sprintf(port, "%d", config.serverPort);
	port[plen] = '\0';

	sleep(1);

	if (fork() == 0) {
		char *args[] = {"iptables", "-t", "raw", "-A", "PREROUTING", 
						"-p", "tcp", "--dport", port, 
						"-j", "NFQUEUE", "--queue-num", "1", NULL};
		execvp("iptables", args);
	}

	if (fork() == 0) {
		char *args[] = {"iptables", "-t", "mangle", "-A", "POSTROUTING", 
						"-p", "tcp", "--sport", port, 
						"-j", "NFQUEUE", "--queue-num", "1", NULL};
		execvp("iptables", args);
	}

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

// resets the configs
void clearConfigs(){
	// State config
	config.id = -1;
	config.renew = 2; //Default
	config.wait = 1; //Default
	config.faults = 1; //Default
	
	config.serverPort = -1;
	config.frontPort = -1;
	config.frontEnd = NULL;
	
	// Bloom Config
	bconfig.bloomLen = 2500000; //Default
	bconfig.backPort = -1;
}

// frees the memory
void cleanConfigs(){
	if(config.frontEnd != NULL)
		free(config.frontEnd);
	
	config.frontEnd = NULL;
}
