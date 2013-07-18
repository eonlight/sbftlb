#ifndef THREAD_H
#define THREAD_H

/* Socket to send and receive blooms */
int ts;

void resetBloom(LBBloom *bloom);
void *bloomThread(void *arg);

char * createBloomBuffer(LBBloom *bloom);
void sendBloom(int lb, char * buffer, int blen);
int isWhatcher(int lb, int whatcher, int num);

#endif
