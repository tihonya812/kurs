// treealoc.h
#ifndef TREEALOC_H
#define TREEALOC_H

#include <stddef.h>

void* __wrap_malloc(size_t size);
void __wrap_free(void* ptr);
void* __wrap_realloc(void* ptr, size_t size);

void* treealoc_malloc(size_t size);
void* treealoc_realloc(void* ptr, size_t size);
void  treealoc_free(void* ptr);

void treealoc_init();
void treealoc_debug();

#endif // TREEALOC_H