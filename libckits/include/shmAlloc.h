#ifndef MY_SHM_ALLOC_H
#define MY_SHM_ALLOC_H

#include <stdint.h>

typedef void * shm_alloc_handle;

#ifdef __cplusplus
extern "C"
{
#endif

shm_alloc_handle init_shm_allocator(uint8_t *memStart, uint32_t chunkSize);
int uninit_shm_allocator(shm_alloc_handle handle);

void * shm_malloc(shm_alloc_handle handle, size_t size);
char * shm_strdup(shm_alloc_handle handle, const char *s);

void shm_free(shm_alloc_handle handle, void *mem);
int shm_add_reference(shm_alloc_handle handle, void *mem);

inline uint32_t shm_offset(shm_alloc_handle handle, void *mem);
inline void * shm_pointer(shm_alloc_handle handle, uint32_t offset);
    
void shm_dump(shm_alloc_handle handle);

#ifdef __cplusplus
};
#endif

#endif
