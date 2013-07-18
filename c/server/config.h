#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
	int id;
	int renew; //time before a bloom is renew
	int wait; //time a thread waits for a LB answer
	int serverPort;
	int frontPort;
	int helloPort;
	char * frontEnd;
	int faults;
} Config;

typedef struct {
	int bloomLen;
	int backPort;
} BloomConfig;

Config config;
BloomConfig bconfig;

void addIptablesRules();

void parseConfigLine(char *line, int l);
void readConfig(char *filename);

void cleanConfigs();
void clearConfigs();

#endif
