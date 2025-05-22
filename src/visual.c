#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "b_tree.h"
#include "visual.h"
#include "Lib.h" // Для вызова функций treealoc_malloc/cleanup

int WINDOW_WIDTH = 846;
int WINDOW_HEIGHT = 579;
#define NODE_WIDTH 100
#define NODE_HEIGHT 60
#define VERTICAL_SPACING 100
#define HORIZONTAL_SPACING 40

#define MAX_TREE_DEPTH 10

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;
TTF_Font* small_font = NULL; // Для отладочной информации или мелкого текста

int visual_initialized = 0;
static int tree_changed = 1; // Флаг, указывающий, нужно ли перерисовать дерево
static int debug_info_visible = 1; // Флаг для управления видимостью отладочной информации

static Uint32 last_resize_time = 0;
static const Uint32 RESIZE_DEBOUNCE_MS = 100;

static FILE* visual_log_file = NULL;

void visual_log(const char* message) {
    if (!visual_log_file) return;
    time_t now = time(NULL);
    char* timestamp = ctime(&now);
    timestamp[strlen(timestamp) - 1] = '\0';
    fprintf(visual_log_file, "[%s] [visual] %s\n", timestamp, message);
}

void visual_init_logging() {
    visual_log_file = fopen("visual.log", "a");
    if (!visual_log_file) {
        fprintf(stderr, "[visual] Failed to open visual.log\n");
    }
}

void visual_cleanup_logging() {
    if (visual_log_file) {
        fclose(visual_log_file);
        visual_log_file = NULL;
    }
}

// Вспомогательная функция для отрисовки текста
void draw_text(SDL_Renderer* renderer, TTF_Font* text_font, const char* text, SDL_Color color, int x, int y, int align_center_x, int align_center_y) {
    if (!text_font) {
        visual_log("Attempted to draw text with NULL font.");
        return;
    }
    SDL_Surface* surface = TTF_RenderUTF8_Blended(text_font, text, color);
    if (!surface) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Failed to render text surface: %s", TTF_GetError());
        visual_log(msg);
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Failed to create text texture: %s", SDL_GetError());
        visual_log(msg);
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect text_rect = {x, y, surface->w, surface->h};
    if (align_center_x) {
        text_rect.x = x - surface->w / 2;
    }
    if (align_center_y) {
        text_rect.y = y - surface->h / 2;
    }

    SDL_RenderCopy(renderer, texture, NULL, &text_rect);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

// Обновленная функция для отрисовки кнопки
void draw_button(const char* text, SDL_Rect rect, SDL_Color bg_color, SDL_Color text_color) {
    SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Border color
    SDL_RenderDrawRect(renderer, &rect);

    if (font) { // Используем основной шрифт для кнопок
        int text_width, text_height;
        if (TTF_SizeUTF8(font, text, &text_width, &text_height) == -1) {
            char msg[128];
            snprintf(msg, sizeof(msg), "TTF_SizeUTF8 failed: %s", TTF_GetError());
            visual_log(msg);
            return;
        }
        draw_text(renderer, font, text, text_color,
                  rect.x + (rect.w - text_width) / 2,
                  rect.y + (rect.h - text_height) / 2,
                  0, 0); // Already calculated alignment
    }
}


typedef struct {
    int node_count;
    int total_width;
} LevelInfo;

void calculate_level_width(BNode* node, int depth, LevelInfo* levels, int max_depth) {
    if (!node || depth >= max_depth) return;
    // Count only actual blocks, not empty slots
    for (int i = 0; i < node->n; i++) {
        if (node->blocks[i]) {
            levels[depth].node_count++;
        }
    }
    // Simple width calculation for the level based on average node count * spacing
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

    // Set background color (light grey)
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255); // <<-- ИЗМЕНЕНИЕ: Светло-серый фон
    SDL_RenderClear(renderer);

    // Max depth for level info
    LevelInfo levels[MAX_TREE_DEPTH] = {0};
    calculate_level_width(root, 0, levels, MAX_TREE_DEPTH);

    int current_node_count = 0;
    // Apply scale to tree rendering only
    SDL_RenderSetScale(renderer, scale, scale);
    draw_node(root, (WINDOW_WIDTH / 2 + offset_x)/scale, (50 + offset_y)/scale, 0, &current_node_count, 0, 0, scale, levels);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f); // Reset scale for UI elements

    // --- Draw UI Elements (buttons and debug info) ---
    SDL_Color button_bg_color = {100, 100, 200, 255}; // Blueish
    SDL_Color button_text_color = {255, 255, 255, 255}; // White
    SDL_Color debug_color = {0, 0, 0, 255}; // Black

    // Button positions
    SDL_Rect add64_btn_rect = {10, 10, 160, 40};
    SDL_Rect add256_btn_rect = {180, 10, 160, 40};
    SDL_Rect cleanup_btn_rect = {350, 10, 160, 40};
    SDL_Rect toggle_debug_btn_rect = {520, 10, 160, 40};

    draw_button("Добавить 64 байта", add64_btn_rect, button_bg_color, button_text_color);
    draw_button("Добавить 256 байт", add256_btn_rect, button_bg_color, button_text_color);
    draw_button("Очистить дерево", cleanup_btn_rect, button_bg_color, button_text_color);
    draw_button(debug_info_visible ? "Скрыть отладку" : "Показать отладку", toggle_debug_btn_rect, button_bg_color, button_text_color);


    // Debug info at the bottom (only if visible)
    if (debug_info_visible && small_font) {
        char debug_text[128];
        snprintf(debug_text, sizeof(debug_text), "Смещение: (%d, %d), Масштаб: %.1fx, Узлов: %d", offset_x, offset_y, scale, current_node_count);
        draw_text(renderer, small_font, debug_text, debug_color, 10, WINDOW_HEIGHT - 30, 0, 0);

        char font_status[128];
        snprintf(font_status, sizeof(font_status), "Шрифт загружен: %s", font ? "Да" : "Нет");
        draw_text(renderer, small_font, font_status, debug_color, 10, WINDOW_HEIGHT - 50, 0, 0);
    }

    // Update window title with node count
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "Treealoc Visualizer | Узлов: %d", current_node_count); // <<-- ИЗМЕНЕНИЕ: Динамический заголовок
    SDL_SetWindowTitle(window, window_title);

    SDL_RenderPresent(renderer);
    tree_changed = 0;
    tree_modified = 0;
}

void draw_rect_with_text(int x, int y, size_t size, void* addr, int is_free) {
    SDL_Rect shadow_rect = {x - NODE_WIDTH / 2 + 3, y + 3, NODE_WIDTH, NODE_HEIGHT};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 100); // <<-- ИЗМЕНЕНИЕ: Уменьшена прозрачность тени
    SDL_RenderFillRect(renderer, &shadow_rect);

    SDL_Rect rect = {x - NODE_WIDTH / 2, y, NODE_WIDTH, NODE_HEIGHT};
    SDL_SetRenderDrawColor(renderer, is_free ? 0 : 180, is_free ? 180 : 0, 0, 255);
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &rect);

    if (font) {
        char text_size[32];
        char text_addr[32];
        snprintf(text_size, sizeof(text_size), "%zu", size);
        snprintf(text_addr, sizeof(text_addr), "%p", addr);

        SDL_Color color = {255, 255, 255, 255};

        // Render size
        int size_w, size_h;
        TTF_SizeUTF8(font, text_size, &size_w, &size_h);
        draw_text(renderer, font, text_size, color,
                  x - size_w / 2, y + (NODE_HEIGHT / 2) - size_h - 5,
                  0, 0);

        // Render address
        int addr_w, addr_h;
        TTF_SizeUTF8(font, text_addr, &addr_w, &addr_h);
        draw_text(renderer, font, text_addr, color,
                  x - addr_w / 2, y + (NODE_HEIGHT / 2) + 5,
                  0, 0);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "Failed to render text: Font is NULL.");
        visual_log(msg);
    }
}

void draw_node(BNode* node, int x, int y, int depth, int* node_count, int parent_x, int parent_y, float scale, LevelInfo* levels) {
    if (!node) return;

    int current_node_center_x = x;

    // Calculate dynamic horizontal spacing based on the current level's total width and number of nodes
    int total_nodes_on_level = 0;
    for (int i = 0; i < node->n; i++) {
        if (node->blocks[i]) {
            total_nodes_on_level++;
        }
    }

    int current_level_width = total_nodes_on_level * NODE_WIDTH + (total_nodes_on_level > 0 ? (total_nodes_on_level - 1) * HORIZONTAL_SPACING : 0);
    int starting_x = x - current_level_width / 2;

    int block_x_offset = starting_x;
    for (int i = 0; i < node->n; i++) {
        if (node->blocks[i]) { // Проверяем, что блок не NULL
            draw_rect_with_text(block_x_offset + NODE_WIDTH / 2, y, node->sizes[i], node->blocks[i], node->is_free[i]);
            (*node_count)++;
            block_x_offset += NODE_WIDTH + HORIZONTAL_SPACING;
        }
    }
    
    // Connect to parent (if not root)
    if (depth > 0) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawLine(renderer, current_node_center_x, y, parent_x, parent_y + NODE_HEIGHT);
    }

    int child_start_x_for_level = x - (node->n * (NODE_WIDTH + HORIZONTAL_SPACING)) / 2;
    for (int i = 0; i <= node->n; i++) {
        if (node->children[i]) {
            int child_x = child_start_x_for_level + i * (NODE_WIDTH + HORIZONTAL_SPACING) + NODE_WIDTH / 2;
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderDrawLine(renderer, current_node_center_x, y + NODE_HEIGHT, child_x, y + VERTICAL_SPACING);
            draw_node(node->children[i], child_x, y + VERTICAL_SPACING, depth + 1, node_count, current_node_center_x, y, scale, levels);
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

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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

    // Try to load fonts from several paths, prioritizing user's working path
    const char* font_paths[] = {
        "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf", // User's confirmed path
        "./LiberationSans-Regular.ttf", // Current directory
        "./arial.ttf", // Current directory
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", // Common Linux path
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", // Another common Linux path
        "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf", // Common path for installed Arial
        "/Library/Fonts/Arial.ttf", // macOS path
        "/System/Library/Fonts/Supplemental/Arial.ttf", // macOS path
    };
    int num_font_paths = sizeof(font_paths) / sizeof(font_paths[0]);

    for (int i = 0; i < num_font_paths; ++i) {
        font = TTF_OpenFont(font_paths[i], 14); // Main font size for nodes and buttons
        if (font) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Main font loaded from: %s", font_paths[i]);
            visual_log(msg);
            break;
        } else {
            char msg_err[512];
            snprintf(msg_err, sizeof(msg_err), "Failed to load font from %s: %s", font_paths[i], TTF_GetError());
            visual_log(msg_err);
        }
    }

    if (!font) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Failed to load main font! TTF_Error: Couldn't open any font: %s. Please ensure a font file (e.g., 'arial.ttf', 'LiberationSans-Regular.ttf') is in the same directory as the executable, or installed in system paths.", TTF_GetError());
        visual_log(msg);
        fprintf(stderr, "%s\n", msg);
        // Continue without font, but log the error
    }

    // Try to load a small font for debug info (made larger now)
    for (int i = 0; i < num_font_paths; ++i) {
        small_font = TTF_OpenFont(font_paths[i], 18); // <<-- ИСПРАВЛЕНИЕ ЗДЕСЬ: Увеличенный размер шрифта для отладки
        if (small_font) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Small font loaded from: %s", font_paths[i]);
            visual_log(msg);
            break;
        } else {
            char msg_err[512];
            snprintf(msg_err, sizeof(msg_err), "Failed to load small font from %s: %s", font_paths[i], TTF_GetError());
            visual_log(msg_err);
        }
    }
    if (!small_font) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Failed to load small font! TTF_Error: Couldn't open any font for debug info: %s", TTF_GetError());
        visual_log(msg);
        fprintf(stderr, "%s\n", msg);
    }


    visual_initialized = 1;

    int running = 1;
    SDL_Event e;
    float scale = 1.0f;
    int offset_x = 0; 
    int offset_y = 0;
    int fullscreen = 0;
    int drag_active = 0;
    
    // Initial centering, adjust to account for potential UI elements
    offset_x = (WINDOW_WIDTH - (NODE_WIDTH + HORIZONTAL_SPACING) * T * 2) / 2; // Rough estimate for initial centering

    // Define button rectangles for click detection (same as drawing)
    SDL_Rect add64_btn_rect = {10, 10, 160, 40};
    SDL_Rect add256_btn_rect = {180, 10, 160, 40};
    SDL_Rect cleanup_btn_rect = {350, 10, 160, 40};
    SDL_Rect toggle_debug_btn_rect = {520, 10, 160, 40};

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
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    // Check for button clicks
                    SDL_Point mouse_point = {e.button.x, e.button.y};
                    if (SDL_PointInRect(&mouse_point, &add64_btn_rect)) {
                        treealoc_malloc(64); // Call your malloc wrapper
                        tree_changed = 1;
                        visual_log("Button: Add 64 bytes clicked.");
                    } else if (SDL_PointInRect(&mouse_point, &add256_btn_rect)) {
                        treealoc_malloc(256); // Call your malloc wrapper
                        tree_changed = 1;
                        visual_log("Button: Add 256 bytes clicked.");
                    } else if (SDL_PointInRect(&mouse_point, &cleanup_btn_rect)) {
                        treealoc_cleanup(); // Call your cleanup function
                        tree_changed = 1;
                        visual_log("Button: Cleanup Tree clicked.");
                    } else if (SDL_PointInRect(&mouse_point, &toggle_debug_btn_rect)) {
                        debug_info_visible = !debug_info_visible;
                        tree_changed = 1;
                        char msg[64];
                        snprintf(msg, sizeof(msg), "Button: Toggle debug info to %s", debug_info_visible ? "visible" : "hidden");
                        visual_log(msg);
                    } else {
                        // If no button was clicked, start dragging
                        drag_active = 1;
                    }
                }
            } else if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    drag_active = 0;
                }
            } else if (e.type == SDL_MOUSEMOTION) {
                if (drag_active) {
                    offset_x += e.motion.xrel;
                    offset_y += e.motion.yrel;
                    tree_changed = 1;
                }
            } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
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

        draw_tree(offset_x, offset_y, scale);
        SDL_Delay(100);
    }

    if (font) TTF_CloseFont(font);
    if (small_font) TTF_CloseFont(small_font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    visual_log("Visualizer closed");
    visual_cleanup_logging();
    return 0;
}