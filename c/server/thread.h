#ifndef THREAD_H
#define THREAD_H

/* Socket to send and receive blooms */
int ts;

void *bloomThread(void *arg);
int isWhatcher(int lb, int whatcher, int num);

void resetBloom(LBBloom *bloom);
char * createBloomBuffer(LBBloom *bloom);
void sendBloom(int lb, char * buffer, int blen);

#endif
