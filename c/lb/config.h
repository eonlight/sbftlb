#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
	int id;
	int wait; //Time a thread sleeps before checking for new Blooms
	int endPort;
	int frontPort;
	int helloPort;
	char * frontEnd;
} Config;

typedef struct {
	int bloomLen;
	int backPort;
} BloomConfig;

typedef struct {
	int faults;
	int KILL_TH_SUSP;
	int KILL_TH_ASUSP;
	int TIMEOUT; //timeout of a HTTPRequestNode
} BFTConfig;

Config config;
BloomConfig bconfig;
BFTConfig fconfig;

void readConfig(char *filename);
void parseLine(char *line, int l);
void cleanConfigs();
void clearConfigs();
void addIptablesRules();

#endif
