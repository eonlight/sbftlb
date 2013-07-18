unsigned int sax_hash(const char *key, int size){
	unsigned int h=0;
	int i;
	for(i=0;i<size;i++) h^=(h<<5)+(h>>2)+(unsigned char)*key++;
	return h;
}

unsigned int sdbm_hash(const char *key, int size){
	unsigned int h=0;
	int i;
	for(i=0;i<size;i++) h=(unsigned char)*key++ + (h<<6) + (h<<16) - h;
	return h;
}

unsigned int MurmurHash2 (const void * key, int len, unsigned int seed){
	const unsigned int m = 0x5bd1e995;
	const int r = 24;
	unsigned int h = seed ^ len;
	const unsigned char * data = (const unsigned char *)key;
	while(len >= 4){
		unsigned int k = *(unsigned int *)data;
		k *= m; k ^= k >> r; k *= m; h *= m; h ^= k;
		data += 4; len -= 4;}
	switch(len){
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0]; h *= m; };
	h ^= h >> 13;h *= m; h ^= h >> 15;
	return h;
}