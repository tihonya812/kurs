#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "treealoc.h"
#include "visual.h"

void* run_visual(void* arg) {
    visual_main_loop();
    return NULL;
}

int main() {
    pthread_t vis_thread;

    treealoc_init();

    // Запускаем визуализацию в отдельном потоке
    pthread_create(&vis_thread, NULL, run_visual, NULL);

    void* p1 = malloc(32);
    sleep(1);
    void* p2 = malloc(64);
    sleep(1);
    free(p1);
    sleep(1);
    void* p3 = realloc(p2, 128);
    sleep(1);

    // Ждём завершения окна (если закроют)
    pthread_join(vis_thread, NULL);

    return 0;
}
