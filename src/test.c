#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "treealoc.h"
#include "visual.h"

extern void* __real_malloc(size_t size);
extern void __real_free(void* ptr);
extern void* __real_realloc(void* ptr, size_t size);
extern void* __real_calloc(size_t nmemb, size_t size);

void* __wrap_malloc(size_t size) {
    void* ptr = treealoc_malloc(size);
    printf("[HOOK] malloc(%zu) = %p\n", size, ptr);
    return ptr;
}

void __wrap_free(void* ptr) {
    printf("[HOOK] free(%p)\n", ptr);
    treealoc_free(ptr);
}

void* __wrap_realloc(void* ptr, size_t size) {
    void* new_ptr = treealoc_realloc(ptr, size);
    printf("[HOOK] realloc(%p, %zu) = %p\n", ptr, size, new_ptr);
    return new_ptr;
}

void* __wrap_calloc(size_t nmemb, size_t size) {
    void* ptr = treealoc_calloc(nmemb, size);
    printf("[HOOK] calloc(%zu, %zu) = %p\n", nmemb, size, ptr);
    return ptr;
}

void* run_visual(void* arg) {
    visual_main_loop();
    return NULL;
}

void test_simple_malloc_free() {
    printf("=== Test 1: Simple malloc/free ===\n");
    void* p1 = malloc(32);
    sleep(1);
    void* p2 = malloc(64);
    sleep(1);
    free(p1);
    sleep(1);
    free(p2);
    sleep(1);
}

void test_realloc() {
    printf("=== Test 2: Realloc ===\n");
    void* p = malloc(64);
    sleep(1);
    p = realloc(p, 128);
    sleep(1);
    p = realloc(p, 16);
    sleep(1);
    free(p);
    sleep(1);
}

void test_calloc() {
    printf("=== Test 3: Calloc ===\n");
    void* p = calloc(10, 8);
    sleep(1);
    free(p);
    sleep(1);
}

void test_intensive() {
    printf("=== Test 4: Intensive malloc/free ===\n");
    void* ptrs[50];
    for (int i = 0; i < 50; i++) {
        ptrs[i] = malloc(rand() % 256 + 1);
        sleep(1);
    }
    for (int i = 0; i < 50; i++) {
        free(ptrs[i]);
        sleep(1);
    }
}

void test_fragmentation() {
    printf("=== Test 5: Memory Fragmentation ===\n");
    void* ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = malloc(rand() % 512 + 1);
        sleep(1);
    }
    for (int i = 0; i < 100; i += 2) {
        free(ptrs[i]);
        sleep(1);
    }
    void* large = malloc(1024);
    printf("[TEST] Large malloc(1024) = %p\n", large);
    sleep(1);
    free(large);
    sleep(1);
    for (int i = 1; i < 100; i += 2) {
        free(ptrs[i]);
        sleep(1);
    }
}

void test_edge_cases() {
    printf("=== Test 6: Edge Cases ===\n");
    void* p = malloc(0);
    printf("[TEST] malloc(0) = %p\n", p);
    sleep(1);
    free(p);
    sleep(1);
    p = realloc(NULL, 100);
    printf("[TEST] realloc(NULL, 100) = %p\n", p);
    sleep(1);
    p = realloc(p, 0);
    printf("[TEST] realloc(p, 0) = %p\n", p);
    sleep(1);
}

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void print_menu() {
    printf("\n=== Treealoc Test Menu ===\n");
    printf("1. Simple malloc/free\n");
    printf("2. Realloc\n");
    printf("3. Calloc\n");
    printf("4. Intensive malloc/free\n");
    printf("5. Memory Fragmentation\n");
    printf("6. Edge Cases\n");
    printf("7. Run All Tests\n");
    printf("0. Exit\n");
    printf("Enter choice: ");
}

int main() {
    pthread_t vis_thread;
    treealoc_init();
    pthread_create(&vis_thread, NULL, run_visual, NULL);

    int choice;
    char input[10];
    while (1) {
        print_menu();
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        clear_input_buffer();
        choice = atoi(input);
        switch (choice) {
            case 1: test_simple_malloc_free(); break;
            case 2: test_realloc(); break;
            case 3: test_calloc(); break;
            case 4: test_intensive(); break;
            case 5: test_fragmentation(); break;
            case 6: test_edge_cases(); break;
            case 7:
                test_simple_malloc_free();
                test_realloc();
                test_calloc();
                test_intensive();
                test_fragmentation();
                test_edge_cases();
                break;
            case 0: goto exit;
            default: printf("Invalid choice\n");
        }
    }

exit:
    pthread_join(vis_thread, NULL);
    treealoc_cleanup(); // Добавляем очистку
    return 0;
}