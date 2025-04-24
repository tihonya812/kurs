#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
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
TTF_Font* font = NULL;

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

    if (font) {
        char text[64];
        snprintf(text, sizeof(text), "%zu@%p %s", size, addr, is_free ? "free" : "used");
        SDL_Color color = {0, 0, 0, 255};
        SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect text_rect = {x - NODE_WIDTH / 2 + 5, y + 5, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, NULL, &text_rect);
            SDL_FreeSurface(surface);
            SDL_DestroyTexture(texture);
        } else {
            printf("[visual] Failed to render text: %s\n", TTF_GetError());
        }
    }
}

void draw_node(BNode* node, int x, int y, int depth) {
    if (!node) return;

    // Рисуем каждый ключ в узле
    for (int i = 0; i < node->n; i++) {
        int key_x = x + (i - node->n/2) * (NODE_WIDTH + HORIZONTAL_SPACING);
        draw_rect_with_text(key_x, y, node->sizes[i], node->blocks[i], node->is_free[i]);
    }

    // Рисуем дочерние узлы
    for (int i = 0; i <= node->n; i++) {
        if (node->children[i]) {
            int child_x = x + (i - node->n/2) * (NODE_WIDTH + HORIZONTAL_SPACING);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderDrawLine(renderer, x, y + NODE_HEIGHT, child_x, y + VERTICAL_SPACING);
            draw_node(node->children[i], child_x, y + VERTICAL_SPACING, depth + 1);
        }
    }
}

int visual_main_loop() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[visual] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("[visual] SDL initialized\n");

    if (TTF_Init() < 0) {
        printf("[visual] TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    printf("[visual] TTF initialized\n");

    window = SDL_CreateWindow("Treealoc Visualizer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        printf("[visual] SDL_CreateWindow failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    printf("[visual] Window created\n");

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("[visual] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    printf("[visual] Renderer created\n");

    font = TTF_OpenFont("/usr/share/fonts/liberation/LiberationSans-Regular.ttf", 12);
    if (!font) {
        printf("[visual] TTF_OpenFont failed: %s\n", TTF_GetError());
    } else {
        printf("[visual] Font loaded\n");
    }

    int running = 1;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }

        draw_tree();
        SDL_Delay(100);
    }

    if (font) TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    printf("[visual] Visualizer closed\n");
    return 0;
}