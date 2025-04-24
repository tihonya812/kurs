#include "treealoc.h"
#include "b_tree.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int initialized = 0;

void* treealoc_malloc(size_t size) {
    printf("[DEBUG] Inside treealoc_malloc(%zu)\n", size);
    printf("[DEBUG] root = %p\n", root);
    void* ptr = btree_find_best_fit(size);
    if (ptr) {
        printf("[treealoc] reused block for malloc(%zu) = %p\n", size, ptr);
        return ptr;
    }

    ptr = malloc(size);
    if (!ptr) {
        printf("[ERROR] malloc failed\n");
        return NULL;
    }
    btree_insert(size, ptr);
    printf("[treealoc] malloc(%zu) = %p\n", size, ptr);
    return ptr;
}

void* treealoc_realloc(void* ptr, size_t size) {
    if (!ptr) return treealoc_malloc(size);
    if (size == 0) {
        treealoc_free(ptr);
        return NULL;
    }

    int index;
    BNode* node = find_node(root, ptr, &index);
    if (node && size <= node->sizes[index]) {
        node->sizes[index] = size;
        printf("[treealoc] Shrunk block %p to %zu\n", ptr, size);
        return ptr;
    }

    void* new_ptr = btree_find_best_fit(size);
    if (new_ptr) {
        size_t old_size = node ? node->sizes[index] : size;
        memcpy(new_ptr, ptr, old_size < size ? old_size : size);
        btree_remove(ptr);
        printf("[treealoc] Reused block for realloc(%p, %zu) = %p\n", ptr, size, new_ptr);
        return new_ptr;
    }

    new_ptr = realloc(ptr, size);
    if (new_ptr) {
        btree_remove(ptr);
        btree_insert(size, new_ptr);
    }
    printf("[treealoc] realloc(%p, %zu) = %p\n", ptr, size, new_ptr);
    return new_ptr;
}

void* treealoc_calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* ptr = treealoc_malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
        printf("[treealoc] calloc(%zu, %zu) = %p\n", nmemb, size, ptr);
    }
    return ptr;
}

void treealoc_free(void* ptr) {
    if (ptr) {
        btree_remove(ptr);
        printf("[treealoc] free(%p)\n", ptr);
    }
}

void treealoc_init() {
    initialized = 1;
    printf("[treealoc] Initialized!\n");
}

void treealoc_debug() {
    btree_debug();
}