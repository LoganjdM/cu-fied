/// reused code from my own C types implementation repository. its a local repo, maybe i'll upload it to GH, idk i barely use GH //
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifndef NDEBUG
#	include <assert.h>
#else
#	define assert(x) if(!(x)) exit(-1)
#endif

union generic {
	char* s;
	
	int* i;
	unsigned int* u;
	long* l;
	size_t* lu;

	void* p;
};

struct bucket {
	void* key;
	void* val;
};

// this is kind of an OOP way of doing things, but I felt syntatically & semantically it is nice for tables //
typedef struct table table_t;
struct table {
	size_t cap;
	struct bucket* buckets;

	bool (*put)(table_t*, void*, void*);
	union generic (*get)(table_t*, void*);
	// you set your own hash function //
	int32_t (*hash_func)(void*);
};

bool __ht_put(table_t* ht, void* key, void* val) {
	assert(key);
	
	int32_t hash = ht->hash_func(key);
	size_t i = hash % ht->cap;
	while(ht->buckets[i].key) {
		if(!strcmp(ht->buckets[i].key, key)) {
			ht->buckets[i].val = malloc(strlen(val));
			if(!ht->buckets[i].val) return false;
			else strcpy(ht->buckets[i].val, val);
			
			return true;
		}
		++i;
		if(i > ht->cap) i = 0;
	}

	ht->buckets[i].key = malloc(strlen(key)+1);
	if(!ht->buckets[i].key) return false;

	strcpy(ht->buckets[i].key, key);
	ht->buckets[i].val = val;
	return true;
}

union generic __ht_get(table_t* ht, void* key) {
	assert(key);

	int32_t hash = ht->hash_func(key);
	size_t i = hash % ht->cap;
	while(ht->buckets[i].key) {
		if(!strcmp(ht->buckets[i].key, key)) {
			return (union generic){ .p = ht->buckets[i].val };
		}
		++i;
		if(i >ht->cap) i = 0;
	}
	return (union generic){ .p = NULL };
}

#define FNV_OFFSET 0x811c9dc5
#define FNV_PRIME  0x01000193
// Fowler's Noll Vo's hashing func //
int32_t fnv_1(void* k) {
	int32_t hash = FNV_OFFSET;
	for(uint32_t i=0; *((char*)k + i)/*prime example of generics sucking in C*/; ++i) {
		hash *= FNV_PRIME;
		hash ^= FNV_OFFSET;
	} return hash;
}

table_t* ht_create(size_t starting_cap) {
	table_t* resulting_table = malloc(sizeof(table_t));
	if(!resulting_table) return NULL;

	struct bucket* buckets = calloc(starting_cap+1, sizeof(struct bucket));
	if(!buckets) {
		free(resulting_table);
		return NULL;
	}

	// the reason why we are so fuckin weird with the hash function here is cuz I originally made this as an STB style table header lib where you could use any hashing f(x) you wanted //
	*resulting_table = (table_t){ .cap = starting_cap, .buckets = buckets, 
								  .hash_func = fnv_1, .put = __ht_put, .get = __ht_get };
	return resulting_table;
}

void ht_free(table_t* ht) {
	for(size_t i=0; i<ht->cap; ++i) {
		if(ht->buckets[i].key) free(ht->buckets[i].key);
	} free(ht->buckets);
	free(ht);
}