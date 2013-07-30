#ifndef __BLOOM_H__
#define __BLOOM_H__

#include <stdlib.h>

typedef struct {
	int entries;
  	double error;
  	int bits;
  	int bytes;
  	int hashes;
	double bpe;
  	unsigned char * bf;
  	int ready;
} Bloom;

Bloom *createBloom(int entries, double error);
int destroyBloom(Bloom *bloom);
int addToBloom(Bloom *bloom, const char *s, int size);
int checkBloom(Bloom *bloom, const char *s, int size);

#endif
