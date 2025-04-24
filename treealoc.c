// treealoc.c
#include "treealoc.h"
#include "b_tree.h"
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

// Указываем, что будем вызывать оригинальные malloc/free через __real_*
extern void* __real_malloc(size_t size);
extern void  __real_free(void* ptr);
extern void* __real_realloc(void* ptr, size_t size);

static int initialized = 0;

void* __wrap_malloc(size_t size) {
    if (!initialized) return __real_malloc(size);  
    void* ptr = treealoc_malloc(size);
    printf("[HOOK] malloc(%zu) = %p\n", size, ptr);
    return ptr;
}

void __wrap_free(void* ptr) {
    if (!initialized) {
        __real_free(ptr);
        return;
    }
    printf("[HOOK] free(%p)\n", ptr);
    treealoc_free(ptr);
}

void* __wrap_realloc(void* ptr, size_t size) {
    if (!initialized) return __real_realloc(ptr, size);
    void* new_ptr = treealoc_realloc(ptr, size);
    printf("[HOOK] realloc(%p, %zu) = %p\n", ptr, size, new_ptr);
    return new_ptr;
}

void* treealoc_malloc(size_t size) {
    printf("[DEBUG] Inside treealoc_malloc(%zu)\n", size);
    printf("[DEBUG] root = %p\n", root);
    void* ptr = btree_find_best_fit(size);
    if (ptr) {
        printf("[treealoc] reused block for malloc(%zu) = %p\n", size, ptr);
        return ptr;
    }

    ptr = __real_malloc(size);
    if (!ptr) {
        printf("[ERROR] real malloc failed\n");
        return NULL;
    }
    btree_insert(size, ptr);
    printf("[treealoc] malloc(%zu) = %p\n", size, ptr);
    return ptr;
}

void* treealoc_realloc(void* ptr, size_t size) {
    // Попробуем найти подходящий блок в дереве
    void* new_ptr = btree_find_best_fit(size);
    if (!new_ptr) {
        // Если блока не найдено, просто вызываем стандартный realloc
        return __real_realloc(ptr, size);
    }

    // Если новый размер меньше текущего, просто обновим размер
    BNode* node = btree_find_best_fit(size);
    if (size <= node->size) {
        node->size = size;
        return ptr;
    }

    // Если новый размер больше, пробуем найти новый блок подходящего размера в дереве
    new_ptr = btree_find_best_fit(size);
    if (new_ptr) {
        // Если нашли блок, освобождаем старый
        btree_remove(ptr);
        return new_ptr;
    }

    // Если не нашли подходящий блок, выполняем обычный realloc
    return __real_realloc(ptr, size);
}



void treealoc_free(void* ptr) {
    btree_remove(ptr);
    printf("[treealoc] free(%p)\n", ptr);
}

void treealoc_init() {
    initialized = 1;
    printf("[treealoc] Initialized!\n");
}

void treealoc_debug() {
    btree_debug();
}