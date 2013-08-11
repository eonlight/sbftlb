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

void * bloomChecker(void *arg);
void checkFilter(LBBloom * bloom, int now, int bag);
LBBloom * readBloom(char *buf);

#endif
