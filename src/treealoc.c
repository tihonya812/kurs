#include "treealoc.h"
#include "b_tree.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int initialized = 0;

void log_to_file(const char* message) {
    FILE* log_file = fopen("treealoc.log", "a");
    if (log_file) {
        time_t now = time(NULL);
        char* timestamp = ctime(&now);
        timestamp[strlen(timestamp) - 1] = '\0'; // Удаляем \n
        fprintf(log_file, "[%s] %s\n", timestamp, message);
        fclose(log_file);
    }
}

void* treealoc_malloc(size_t size) {
    char log_msg[128];
    printf("[DEBUG] Inside treealoc_malloc(%zu)\n", size);
    snprintf(log_msg, sizeof(log_msg), "[DEBUG] Inside treealoc_malloc(%zu)", size);
    log_to_file(log_msg);

    printf("[DEBUG] root = %p\n", root);
    snprintf(log_msg, sizeof(log_msg), "[DEBUG] root = %p", root);
    log_to_file(log_msg);

    void* ptr = btree_find_best_fit(size);
    if (ptr) {
        printf("[treealoc] reused block for malloc(%zu) = %p\n", size, ptr);
        snprintf(log_msg, sizeof(log_msg), "[treealoc] reused block for malloc(%zu) = %p", size, ptr);
        log_to_file(log_msg);
        return ptr;
    }

    ptr = malloc(size);
    if (!ptr) {
        printf("[ERROR] malloc failed\n");
        log_to_file("[ERROR] malloc failed");
        return NULL;
    }
    btree_insert(size, ptr);
    printf("[treealoc] malloc(%zu) = %p\n", size, ptr);
    snprintf(log_msg, sizeof(log_msg), "[treealoc] malloc(%zu) = %p", size, ptr);
    log_to_file(log_msg);
    return ptr;
}

void* treealoc_realloc(void* ptr, size_t size) {
    char log_msg[128];
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
        snprintf(log_msg, sizeof(log_msg), "[treealoc] Shrunk block %p to %zu", ptr, size);
        log_to_file(log_msg);
        return ptr;
    }

    void* new_ptr = btree_find_best_fit(size);
    if (new_ptr) {
        size_t old_size = node ? node->sizes[index] : size;
        memcpy(new_ptr, ptr, old_size < size ? old_size : size);
        btree_remove(ptr);
        printf("[treealoc] Reused block for realloc(%p, %zu) = %p\n", ptr, size, new_ptr);
        snprintf(log_msg, sizeof(log_msg), "[treealoc] Reused block for realloc(%p, %zu) = %p", ptr, size, new_ptr);
        log_to_file(log_msg);
        return new_ptr;
    }

    void* old_ptr = ptr;
    btree_remove(old_ptr);
    printf("[treealoc] realloc(%p, %zu) = ", old_ptr, size);
    snprintf(log_msg, sizeof(log_msg), "[treealoc] realloc(%p, %zu) = ", old_ptr, size);
    log_to_file(log_msg);
    new_ptr = realloc(old_ptr, size);
    if (!new_ptr) {
        printf("failed\n");
        log_to_file("[ERROR] realloc failed");
        return NULL;
    }
    printf("%p\n", new_ptr);
    snprintf(log_msg, sizeof(log_msg), "%p", new_ptr);
    log_to_file(log_msg);
    btree_insert(size, new_ptr);
    return new_ptr;
}

void* treealoc_calloc(size_t nmemb, size_t size) {
    char log_msg[128];
    size_t total = nmemb * size;
    void* ptr = treealoc_malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
        printf("[treealoc] calloc(%zu, %zu) = %p\n", nmemb, size, ptr);
        snprintf(log_msg, sizeof(log_msg), "[treealoc] calloc(%zu, %zu) = %p", nmemb, size, ptr);
        log_to_file(log_msg);
    }
    return ptr;
}

void treealoc_free(void* ptr) {
    char log_msg[128];
    if (ptr) {
        btree_remove(ptr);
        printf("[treealoc] free(%p)\n", ptr);
        snprintf(log_msg, sizeof(log_msg), "[treealoc] free(%p)", ptr);
        log_to_file(log_msg);
    }
}

void treealoc_init() {
    initialized = 1;
    printf("[treealoc] Initialized!\n");
    log_to_file("[treealoc] Initialized!");
}

void treealoc_debug() {
    btree_debug();
}