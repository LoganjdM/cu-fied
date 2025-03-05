#ifndef _STDLIB_H
#	define __need_size_t
#	include <stddef.h>
#endif 
#ifndef _STDINT_H
#	include <stdint.h>
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

// good ol grandpa C having to forward declare //
typedef struct table table_t;
struct table {
	size_t cap;
	struct bucket* buckets;

	bool (*put)(table_t*, void*, void*);
	union generic (*get)(table_t*, void*);
	int32_t (*hash_func)(void*);
};

table_t* ht_create(size_t starting_cap);
void ht_free(table_t* ht);