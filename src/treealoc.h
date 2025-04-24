#ifndef TREEALOC_H
#define TREEALOC_H

#include <stddef.h>

void treealoc_init(void);
void treealoc_cleanup(void); // Добавляем cleanup
void* treealoc_malloc(size_t size);
void* treealoc_realloc(void* ptr, size_t size);
void* treealoc_calloc(size_t nmemb, size_t size);
void treealoc_free(void* ptr);
void treealoc_debug(void);

#endif