// b_tree.c
#include "b_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> 
#include <malloc.h>

extern void* __real_malloc(size_t size);

BNode* root = NULL;

static BNode* create_node(size_t size, void* ptr) {
    BNode* node = __real_malloc(sizeof(BNode));
    node->size = size;
    node->block = ptr;
    node->is_free = 0;
    node->left = NULL;
    node->right = NULL;
    return node;
}

static void insert_node(BNode** root, size_t size, void* ptr) {
    if (!*root) {
        *root = create_node(size, ptr);
        return;
    }
    if (size < (*root)->size)
        insert_node(&(*root)->left, size, ptr);
    else
        insert_node(&(*root)->right, size, ptr);
}

void btree_insert(size_t size, void* ptr) {
    insert_node(&root, size, ptr);
}

static void print_node(BNode* node, int depth) {
    if (!node) return;

    for (int i = 0; i < depth; i++) printf("  ");
    printf("↳ [%zu bytes] @ %p [%s]\n", node->size, node->block, node->is_free ? "free" : "used");

    print_node(node->left, depth + 1);
    print_node(node->right, depth + 1);
}

void btree_debug() {
    printf("[DEBUG] B-tree structure:\n");
    print_node(root, 0);
}
 
void btree_remove(void* ptr) {
    BNode* current = root;
    while (current) {
        if (current->block == ptr) {
            current->is_free = 1;
            return;
        } else if ((uintptr_t)ptr < (uintptr_t)current->block) {
            current = current->left;
        } else {
            current = current->right;
        }
    }
}

void* btree_find_best_fit(size_t size) {
    BNode* current = root;
    BNode* best = NULL;
    
    while (current) {
        if (current->is_free && current->size >= size) {
            if (!best || current->size < best->size) {
                best = current;
            }
        }
        if (size < current->size) {
            current = current -> left;
        } else {
            current = current->right;
        }
    }

    if(best) {
        best -> is_free = 0;
        return best->block;
    }
    return NULL;
}