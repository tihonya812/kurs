#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "treealoc.h"
#include "visual.h"

// Объявления оригинальных функций
extern void* __real_malloc(size_t size);
extern void __real_free(void* ptr);
extern void* __real_realloc(void* ptr, size_t size);
extern void* __real_calloc(size_t nmemb, size_t size);

// Хуки
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

int main() {
    pthread_t vis_thread;

    treealoc_init();

    // Запускаем визуализацию в отдельном потоке
    pthread_create(&vis_thread, NULL, run_visual, NULL);

    // Тест 1: Простое выделение и освобождение
    printf("=== Test 1: Simple malloc/free ===\n");
    void* p1 = malloc(32);
    sleep(1);
    void* p2 = malloc(64);
    sleep(1);
    free(p1);
    sleep(1);

    // Тест 2: Realloc
    printf("=== Test 2: Realloc ===\n");
    void* p3 = realloc(p2, 128);
    sleep(1);
    p3 = realloc(p3, 16);
    sleep(1);
    free(p3);
    sleep(1);

    // Тест 3: Calloc
    printf("=== Test 3: Calloc ===\n");
    void* p4 = calloc(10, 8); // 10 элементов по 8 байт
    sleep(1);
    free(p4);
    sleep(1);

    // Тест 4: Интенсивное выделение/освобождение
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

    // Ждём завершения визуализации
    pthread_join(vis_thread, NULL);

    return 0;
}