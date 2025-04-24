// b_tree.h
#ifndef B_TREE_H
#define B_TREE_H

#include <stddef.h>

typedef struct BNode {
    void* block;
    size_t size;
    int is_free;
    struct BNode* left;
    struct BNode* right;
} BNode;

void btree_insert(size_t size, void* ptr);
void* btree_find_best_fit(size_t size);
void btree_remove(void* ptr);
void btree_debug();

extern BNode* root; // <- экспортируем root из b_tree.c

#endif // B_TREE_H