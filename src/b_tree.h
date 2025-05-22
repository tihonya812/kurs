#ifndef B_TREE_H
#define B_TREE_H

#include <stddef.h>

#define T 2 // Минимальная степень B-дерева

typedef struct BNode {
    int n;              // Количество ключей в узле
    int leaf;           // Является ли узел листом (1 - да, 0 - нет)
    size_t sizes[2*T-1]; // Размеры блоков
    void* blocks[2*T-1]; // Указатели на блоки
    int is_free[2*T-1]; // Флаги свободных блоков (0 - занят, 1 - свободен)
    struct BNode* children[2*T]; // Указатели на дочерние узлы
    int freed;          // Флаг, указывающий, был ли узел освобождён
} BNode;

extern BNode* root;
extern int tree_modified;

void btree_insert(size_t size, void* ptr);
void btree_debug();
void btree_remove(void* ptr);
void btree_full_free(void* ptr);
void btree_cleanup();
void btree_cleanup_node(BNode* node); // Добавляем прототип
void* btree_find_best_fit(size_t size);
BNode* find_node(BNode* node, void* ptr, int* index);

#endif