#ifndef THREAD_H
#define THREAD_H

typedef struct {
	Bloom *bloom;
	int packets; //num packets on bloom
	time_t init;
	int lb; //lb id
	int server; //server id
	int threads[SEARCH_THREADS_NUM];
} LBBloom;

/* Socket to send and receive blooms */
int ts;
LBBloom * workingBloom = NULL;

void * bloomChecker(void *arg);
void checkFilter(LBBloom * bloom);
void readBloom(char *buf);

#endif
