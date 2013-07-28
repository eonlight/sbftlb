#include <limits.h>
#include <stdarg.h>

#include "bloom.h"

Bloom *createBloom(int entries, double error){
  Bloom *bloom;
  if(!(bloom=calloc(1,sizeof(Bloom)))) 
    return NULL;
  
  if (entries < 1 || error == 0) {
        return NULL;
    }

    bloom->entries = entries;
    bloom->error = error;

    double num = log(bloom->error);
    double denom = 0.480453013918201; // ln(2)^2
    bloom->bpe = -(num / denom);

    double dentries = (double)entries;
    bloom->bits = (int)(dentries * bloom->bpe);

    if (bloom->bits % 8) {
        bloom->bytes = (bloom->bits / 8) + 1;
    } else {
        bloom->bytes = bloom->bits / 8;
    }

    bloom->hashes = (int)ceil(0.693147180559945 * bloom->bpe);    // ln(2)

    bloom->bf = (unsigned char *)calloc(bloom->bytes, sizeof(unsigned char));
    if (bloom->bf == NULL) {
        return NULL;
    }

    bloom->ready = 1;
    return bloom;
}

int destroyBloom(Bloom *bloom){
  free(bloom->bf);
  //free(bloom->funcs);
  free(bloom);
  return 0;
}

int addToBloom(Bloom *bloom, const char *s, int size){
  return bloomCheckAdd(bloom, s, size, 1);
}

int checkBloom(Bloom *bloom, const char *s, int size){
  return bloomCheckAdd(bloom, s, size, 0);
}

int bloomCheckAdd(Bloom * bloom, const void * buffer, int len, int add) {
    if (bloom->ready == 0) {
        (void)printf("bloom at %p not initialized!\n", (void *)bloom);
        return -1;
    }

    int hits = 0;
    register unsigned int a = MurmurHash2(buffer, len, 0x9747b28c);
    register unsigned int b = MurmurHash2(buffer, len, a);
    register unsigned int x;
    register unsigned int i;
    register unsigned int byte;
    register unsigned int mask;
    register unsigned char c;

    for (i = 0; i < bloom->hashes; i++) {
        x = (a + i*b) % bloom->bits;
        byte = x >> 3;
        c = bloom->bf[byte];  // expensive memory access
        mask = 1 << (x % 8);

        if (c & mask) {
            hits++;
        } else {
            if (add) {
                bloom->bf[byte] = c | mask;
            }
        }
    }

    if (hits == bloom->hashes) {
        return 1;   // 1 == element already in (or collision)
    }

    return 0;
}
