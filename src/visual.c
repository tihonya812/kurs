#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "b_tree.h"
#include "visual.h"

int WINDOW_WIDTH = 846;
int WINDOW_HEIGHT = 579;
#define NODE_WIDTH 100
#define NODE_HEIGHT 60
#define VERTICAL_SPACING 100
#define HORIZONTAL_SPACING 40

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;

int visual_initialized = 0;
static int tree_changed = 1;

// Для debounce при изменении размера окна
static Uint32 last_resize_time = 0;
static const Uint32 RESIZE_DEBOUNCE_MS = 100;

// Для debounce логов о количестве узлов
static Uint32 last_node_log_time = 0;
static const Uint32 NODE_LOG_DEBOUNCE_MS = 1000; // Задержка 1 секунда для логов о количестве узлов

// Файл для логов
static FILE* visual_log_file = NULL;

// Функция для логирования в файл
void visual_log(const char* message) {
    if (!visual_log_file) return;
    time_t now = time(NULL);
    char* timestamp = ctime(&now);
    timestamp[strlen(timestamp) - 1] = '\0'; // Удаляем \n
    fprintf(visual_log_file, "[%s] [visual] %s\n", timestamp, message);
    fflush(visual_log_file);
}

// Инициализация логирования
void visual_init_logging() {
    visual_log_file = fopen("visual.log", "a");
    if (!visual_log_file) {
        fprintf(stderr, "[visual] Failed to open visual.log\n");
    }
}

// Очистка логирования
void visual_cleanup_logging() {
    if (visual_log_file) {
        fclose(visual_log_file);
        visual_log_file = NULL;
    }
}

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
        Uint32 current_time = SDL_GetTicks();
        if (current_time - last_node_log_time >= NODE_LOG_DEBOUNCE_MS) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Total nodes displayed: %d", node_count);
            visual_log(msg);
            last_node_log_time = current_time;
        }
        last_node_count = node_count;
    }
    SDL_RenderPresent(renderer);
    tree_changed = 0;
    tree_modified = 0;
}

void draw_rect_with_text(int x, int y, size_t size, void* addr, int is_free) {
    SDL_Rect shadow_rect = {x - NODE_WIDTH / 2 + 3, y + 3, NODE_WIDTH, NODE_HEIGHT};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 150);
    SDL_RenderFillRect(renderer, &shadow_rect);

    SDL_Rect rect = {x - NODE_WIDTH / 2, y, NODE_WIDTH, NODE_HEIGHT};
    SDL_SetRenderDrawColor(renderer, is_free ? 0 : 180, is_free ? 180 : 0, 0, 255);
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &rect);

    if (font) {
        char text[64];
        snprintf(text, sizeof(text), "%zu\n%p", size, addr);
        SDL_Color color = {255, 255, 255, 255};
        SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, text, color, NODE_WIDTH - 10);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_Rect text_rect = {
                    x - NODE_WIDTH / 2 + (NODE_WIDTH - surface->w) / 2,
                    y + (NODE_HEIGHT - surface->h) / 2,
                    surface->w,
                    surface->h
                };
                SDL_RenderCopy(renderer, texture, NULL, &text_rect);
                SDL_DestroyTexture(texture);
            } else {
                char msg[128];
                snprintf(msg, sizeof(msg), "Failed to create texture: %s", SDL_GetError());
                visual_log(msg);
            }
            SDL_FreeSurface(surface);
        } else {
            char msg[128];
            snprintf(msg, sizeof(msg), "Failed to render text: %s", TTF_GetError());
            visual_log(msg);
        }
    }
}

void draw_node(BNode* node, int x, int y, int depth, int* node_count, int parent_x, int parent_y, float scale, LevelInfo* levels) {
    if (!node) return;

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
    visual_init_logging();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "SDL_Init failed: %s", SDL_GetError());
        visual_log(msg);
        return 1;
    }
    visual_log("SDL initialized");

    if (TTF_Init() < 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "TTF_Init failed: %s", TTF_GetError());
        visual_log(msg);
        SDL_Quit();
        return 1;
    }
    visual_log("TTF initialized");

    window = SDL_CreateWindow("Treealoc Visualizer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (!window) {
        char msg[128];
        snprintf(msg, sizeof(msg), "SDL_CreateWindow failed: %s", SDL_GetError());
        visual_log(msg);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    visual_log("Window created");

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        char msg[128];
        snprintf(msg, sizeof(msg), "SDL_CreateRenderer failed: %s", SDL_GetError());
        visual_log(msg);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    visual_log("Renderer created");

    font = TTF_OpenFont("/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf", 14);
    if (!font) {
        char msg[128];
        snprintf(msg, sizeof(msg), "TTF_OpenFont failed: %s", TTF_GetError());
        visual_log(msg);
    } else {
        visual_log("Font loaded");
    }

    visual_initialized = 1;

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
                char msg[64];
                switch (e.key.keysym.sym) {
                    case SDLK_PLUS:
                    case SDLK_EQUALS:
                        scale += 0.1f;
                        if (scale > 3.0f) scale = 3.0f;
                        snprintf(msg, sizeof(msg), "Scale increased to %.1fx", scale);
                        visual_log(msg);
                        tree_changed = 1;
                        break;
                    case SDLK_MINUS:
                        scale -= 0.1f;
                        if (scale < 0.3f) scale = 0.3f;
                        snprintf(msg, sizeof(msg), "Scale decreased to %.1fx", scale);
                        visual_log(msg);
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
                            visual_log("Fullscreen enabled");
                        } else {
                            SDL_SetWindowFullscreen(window, 0);
                            visual_log("Fullscreen disabled");
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
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Window resized to %dx%d", WINDOW_WIDTH, WINDOW_HEIGHT);
                    visual_log(msg);
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
    visual_log("Visualizer closed");
    visual_cleanup_logging();
    return 0;
}