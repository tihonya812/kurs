#ifndef B_TREE_H
#define B_TREE_H

#include <stddef.h>

#define T 2 // Минимальная степень B-дерева

typedef struct BNode {
    size_t n;
    size_t sizes[2*T-1];
    void* blocks[2*T-1];
    int is_free[2*T-1];
    struct BNode* children[2*T];
    int leaf;
} BNode;

extern BNode* root;
extern int tree_modified; // Добавляем флаг

void btree_insert(size_t size, void* ptr);
void btree_remove(void* ptr);
void btree_debug(void);
BNode* find_node(BNode* node, void* ptr, int* index);
void* btree_find_best_fit(size_t size); 

#endif