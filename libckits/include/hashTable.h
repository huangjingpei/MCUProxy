#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif
    
typedef enum _iterCtrl iterCtrl;
enum _iterCtrl
{
    ITERCTRL_CONTINUE,
    ITERCTRL_BREAK
};

typedef iterCtrl (*iterFunc)(const char *tokenName, void *tokenValue, void *param);
typedef int (*diffFunc)(void *value1, void *value2);

typedef void *(*allocFunc)(size_t size);
typedef void (*freeFunc)(void *ptr);

int inline setHtStringAlloc(allocFunc func1, freeFunc func2);

int initHashTable(void **pHandle, uint32_t hashSize);

int setHashValue(void *handle, const char *name, void *value, void **retval);
int getHashValue(void *handle, const char *name, void **retval);
int delHashValue(void *handle, const char *name, void **retval);

int iterHashTable(void *handle, iterFunc callback, void *param);
void dumpHashTable(void *handle);

/*
inline int modifyValue(char * name, const char * value);
inline int lookupValue(const char *name, char *value, uint32_t maxlen);


inline int delConfigToken(const char * name);

void tableHealth();
*/


#if defined(__cplusplus)
};
#endif

#endif

