#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include "b_tree.h"
#include "visual.h"

int WINDOW_WIDTH = 1500;
int WINDOW_HEIGHT = 1200;
#define NODE_WIDTH 100 // Увеличиваем ширину узлов
#define NODE_HEIGHT 60
#define VERTICAL_SPACING 100
#define HORIZONTAL_SPACING 40

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;

static int tree_changed = 1;

// Для debounce при изменении размера окна
static Uint32 last_resize_time = 0;
static const Uint32 RESIZE_DEBOUNCE_MS = 100; // Задержка 100 мс

// Структура для хранения информации о ширине уровня
typedef struct {
    int node_count;
    int total_width;
} LevelInfo;

void calculate_level_width(BNode* node, int depth, LevelInfo* levels, int max_depth) {
    if (!node) return;
    levels[depth].node_count += node->n;
    levels[depth].total_width = levels[depth].node_count * (NODE_WIDTH + HORIZONTAL_SPACING);
    for (int i = 0; i <= node->n; i++) {
        if (node->children[i]) {
            calculate_level_width(node->children[i], depth + 1, levels, max_depth);
        }
    }
}

void draw_node(BNode* node, int x, int y, int depth, int* node_count, int parent_x, int parent_y, float scale, LevelInfo* levels);

void draw_tree(int offset_x, int offset_y, float scale) {
    extern int tree_modified;
    static int last_node_count = -1;

    if (!tree_changed && !tree_modified) return;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    LevelInfo levels[10] = {0};
    calculate_level_width(root, 0, levels, 10);

    int node_count = 0;
    draw_node(root, WINDOW_WIDTH / 2 + offset_x, 50 + offset_y, 0, &node_count, 0, 0, scale, levels);
    if (node_count != last_node_count) {
        printf("[visual] Total nodes displayed: %d\n", node_count);
        last_node_count = node_count;
    }
    SDL_RenderPresent(renderer);
    tree_changed = 0;
    tree_modified = 0;
}

void draw_rect_with_text(int x, int y, size_t size, void* addr, int is_free) {
    // Рисуем тень (смещённый прямоугольник)
    SDL_Rect shadow_rect = {x - NODE_WIDTH / 2 + 3, y + 3, NODE_WIDTH, NODE_HEIGHT};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 150); // Полупрозрачная тень
    SDL_RenderFillRect(renderer, &shadow_rect);

    // Рисуем основной прямоугольник
    SDL_Rect rect = {x - NODE_WIDTH / 2, y, NODE_WIDTH, NODE_HEIGHT};
    SDL_SetRenderDrawColor(renderer, is_free ? 0 : 180, is_free ? 180 : 0, 0, 255); // Тёмно-зелёный или тёмно-красный
    SDL_RenderFillRect(renderer, &rect);

    // Рисуем обводку
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Белая обводка
    SDL_RenderDrawRect(renderer, &rect);

    if (font) {
        char text[64];
        snprintf(text, sizeof(text), "%zu\n%p", size, addr); // Убираем статус
        SDL_Color color = {255, 255, 255, 255}; // Белый текст
        SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, text, color, NODE_WIDTH - 10);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                // Центрируем текст
                SDL_Rect text_rect = {
                    x - NODE_WIDTH / 2 + (NODE_WIDTH - surface->w) / 2,
                    y + (NODE_HEIGHT - surface->h) / 2,
                    surface->w,
                    surface->h
                };
                SDL_RenderCopy(renderer, texture, NULL, &text_rect);
                SDL_DestroyTexture(texture);
            } else {
                printf("[visual] Failed to create texture: %s\n", SDL_GetError());
            }
            SDL_FreeSurface(surface);
        } else {
            printf("[visual] Failed to render text: %s\n", TTF_GetError());
        }
    }
}

void draw_node(BNode* node, int x, int y, int depth, int* node_count, int parent_x, int parent_y, float scale, LevelInfo* levels) {
    if (!node) return;

    int level_width = levels[depth].total_width;
    int node_count_on_level = levels[depth].node_count;
    int horizontal_spacing = node_count_on_level > 1 ? (WINDOW_WIDTH - node_count_on_level * NODE_WIDTH) / (node_count_on_level - 1) : HORIZONTAL_SPACING;
    if (horizontal_spacing < 20) horizontal_spacing = 20;
    if (horizontal_spacing > HORIZONTAL_SPACING * 2) horizontal_spacing = HORIZONTAL_SPACING * 2;

    int first_key_x = x;
    for (int i = 0; i < node->n; i++) {
        int key_x = x + (i - node->n/2) * (NODE_WIDTH + horizontal_spacing);
        if (i == 0) first_key_x = key_x;
        draw_rect_with_text(key_x, y, node->sizes[i], node->blocks[i], node->is_free[i]);
        (*node_count)++;
    }

    if (depth > 0) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawLine(renderer, first_key_x, y, parent_x, parent_y + NODE_HEIGHT);
    }

    for (int i = 0; i <= node->n; i++) {
        if (node->children[i]) {
            int child_x = x + (i - node->n/2) * (NODE_WIDTH + horizontal_spacing);
            draw_node(node->children[i], child_x, y + VERTICAL_SPACING, depth + 1, node_count, first_key_x, y, scale, levels);
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
                              WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
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

    font = TTF_OpenFont("/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf", 14); // Возвращаем шрифт 14
    if (!font) {
        printf("[visual] TTF_OpenFont failed: %s\n", TTF_GetError());
    } else {
        printf("[visual] Font loaded\n");
    }

    int running = 1;
    SDL_Event e;
    float scale = 1.0f;
    int offset_x = 300;
    int offset_y = 0;
    int fullscreen = 0;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_PLUS:
                    case SDLK_EQUALS:
                        scale += 0.1f;
                        if (scale > 3.0f) scale = 3.0f;
                        printf("[visual] Scale increased to %.1fx\n", scale);
                        tree_changed = 1;
                        break;
                    case SDLK_MINUS:
                        scale -= 0.1f;
                        if (scale < 0.3f) scale = 0.3f;
                        printf("[visual] Scale decreased to %.1fx\n", scale);
                        tree_changed = 1;
                        break;
                    case SDLK_w:
                        offset_y += 50;
                        tree_changed = 1;
                        break;
                    case SDLK_s:
                        offset_y -= 50;
                        tree_changed = 1;
                        break;
                    case SDLK_a:
                        offset_x += 50;
                        tree_changed = 1;
                        break;
                    case SDLK_d:
                        offset_x -= 50;
                        tree_changed = 1;
                        break;
                    case SDLK_f:
                        fullscreen = !fullscreen;
                        if (fullscreen) {
                            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                            printf("[visual] Fullscreen enabled\n");
                        } else {
                            SDL_SetWindowFullscreen(window, 0);
                            printf("[visual] Fullscreen disabled\n");
                        }
                        tree_changed = 1;
                        break;
                }
            }
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                Uint32 current_time = SDL_GetTicks();
                if (current_time - last_resize_time >= RESIZE_DEBOUNCE_MS) {
                    WINDOW_WIDTH = e.window.data1;
                    WINDOW_HEIGHT = e.window.data2;
                    printf("[visual] Window resized to %dx%d\n", WINDOW_WIDTH, WINDOW_HEIGHT);
                    tree_changed = 1;
                    last_resize_time = current_time;
                }
            }
        }

        SDL_RenderSetScale(renderer, scale, scale);
        draw_tree(offset_x, offset_y, scale);
        SDL_RenderSetScale(renderer, 1.0f, 1.0f);
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