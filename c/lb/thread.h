#ifndef THREAD_H
#define THREAD_H

typedef struct {
	Bloom *bloom;
	int packets; //num packets on bloom
	time_t init;
	int lb; //lb id
	int server; //server id
} LBBloom;

/* Socket to send and receive blooms */
int ts;

void sendLBsInfo(int newts);
void * bloomChecker(void *arg);
void checkFilter(char *buf);
LBBloom * readBloom(char *buf);

#endif
