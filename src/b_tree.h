#ifndef B_TREE_H
#define B_TREE_H

#include <stddef.h>

#define T 2 // Минимальная степень B-дерева

typedef struct BNode {
    size_t n;              // Текущее количество ключей
    size_t sizes[2*T-1];   // Массив размеров блоков
    void* blocks[2*T-1];   // Массив указателей на блоки
    int is_free[2*T-1];    // Статус блоков (1 = свободен, 0 = занят)
    struct BNode* children[2*T]; // Дочерние узлы
    int leaf;              // 1, если узел листовой
} BNode;

extern BNode* root;

void btree_insert(size_t size, void* ptr);
void btree_remove(void* ptr);
void* btree_find_best_fit(size_t size);
void btree_debug(void);
BNode* find_node(BNode* node, void* ptr, int* index);

#endif