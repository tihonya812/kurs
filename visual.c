#include <SDL2/SDL.h>
#include <stdio.h>
#include "b_tree.h"
#include "visual.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define NODE_WIDTH 120
#define NODE_HEIGHT 40
#define VERTICAL_SPACING 80
#define HORIZONTAL_SPACING 20

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;

void draw_node(BNode* node, int x, int y, int depth);

void draw_tree() {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // white
    SDL_RenderClear(renderer);
    draw_node(root, WINDOW_WIDTH / 2, 50, 0);
    SDL_RenderPresent(renderer);
}

void draw_rect_with_text(int x, int y, size_t size, void* addr, int is_free) {
    SDL_Rect rect = {x - NODE_WIDTH / 2, y, NODE_WIDTH, NODE_HEIGHT};
    SDL_SetRenderDrawColor(renderer, is_free ? 0 : 200, is_free ? 200 : 0, 0, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &rect);
    // (Тут можно подключить SDL_ttf для вывода текста, если нужно)
}

void draw_node(BNode* node, int x, int y, int depth) {
    if (!node) return;

    draw_rect_with_text(x, y, node->size, node->block, node->is_free);

    int offset = (WINDOW_WIDTH >> (depth + 2));
    if (node->left) draw_node(node->left, x - offset, y + VERTICAL_SPACING, depth + 1);
    if (node->right) draw_node(node->right, x + offset, y + VERTICAL_SPACING, depth + 1);
}

int visual_main_loop() {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Treealoc Visualizer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    int running = 1;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }

        draw_tree();
        SDL_Delay(100);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
