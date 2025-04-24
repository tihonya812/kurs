#include "b_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

BNode* root = NULL;
int tree_modified = 0; // Флаг изменения дерева

static BNode* create_node(int leaf) {
    BNode* node = malloc(sizeof(BNode));
    if (!node) return NULL;
    node->n = 0;
    node->leaf = leaf;
    for (int i = 0; i < 2*T; i++) {
        node->children[i] = NULL;
    }
    for (int i = 0; i < 2*T-1; i++) {
        node->sizes[i] = 0;
        node->blocks[i] = NULL;
        node->is_free[i] = 0;
    }
    printf("[btree] Created node %p (leaf=%d)\n", node, leaf);
    tree_modified = 1; // Устанавливаем флаг
    return node;
}

static void split_child(BNode* parent, int i, BNode* child) {
    BNode* new_node = create_node(child->leaf);
    new_node->n = T-1;

    for (int j = 0; j < T-1; j++) {
        new_node->sizes[j] = child->sizes[j+T];
        new_node->blocks[j] = child->blocks[j+T];
        new_node->is_free[j] = child->is_free[j+T];
    }

    if (!child->leaf) {
        for (int j = 0; j < T; j++) {
            new_node->children[j] = child->children[j+T];
            child->children[j+T] = NULL;
        }
    }

    child->n = T-1;

    for (int j = parent->n; j > i; j--) {
        parent->children[j+1] = parent->children[j];
    }
    for (int j = parent->n-1; j >= i; j--) {
        parent->sizes[j+1] = parent->sizes[j];
        parent->blocks[j+1] = parent->blocks[j];
        parent->is_free[j+1] = parent->is_free[j];
    }

    parent->children[i+1] = new_node;
    parent->sizes[i] = child->sizes[T-1];
    parent->blocks[i] = child->blocks[T-1];
    parent->is_free[i] = child->is_free[T-1];
    parent->n++;
    printf("[btree] Split child %p at index %d, new node %p\n", child, i, new_node);
    tree_modified = 1;
}

static void insert_nonfull(BNode* node, size_t size, void* ptr) {
    int i = node->n - 1;

    if (node->leaf) {
        while (i >= 0 && node->blocks[i] > ptr) {
            node->sizes[i+1] = node->sizes[i];
            node->blocks[i+1] = node->blocks[i];
            node->is_free[i+1] = node->is_free[i];
            i--;
        }
        node->sizes[i+1] = size;
        node->blocks[i+1] = ptr;
        node->is_free[i+1] = 0;
        node->n++;
        printf("[btree] Inserted block %p (size %zu) into node %p\n", ptr, size, node);
        tree_modified = 1;
    } else {
        while (i >= 0 && node->blocks[i] > ptr) i--;
        i++;
        if (node->children[i]->n == 2*T-1) {
            split_child(node, i, node->children[i]);
            if (node->blocks[i] < ptr) i++;
        }
        insert_nonfull(node->children[i], size, ptr);
    }
}

void btree_insert(size_t size, void* ptr) {
    if (!root) {
        root = create_node(1);
        root->sizes[0] = size;
        root->blocks[0] = ptr;
        root->is_free[0] = 0;
        root->n = 1;
        printf("[btree] Inserted block %p (size %zu) as root\n", ptr, size);
        tree_modified = 1;
        return;
    }

    if (root->n == 2*T-1) {
        BNode* new_root = create_node(0);
        new_root->children[0] = root;
        split_child(new_root, 0, root);
        root = new_root;
        insert_nonfull(root, size, ptr);
    } else {
        insert_nonfull(root, size, ptr);
    }
}

static void print_node(BNode* node, int depth) {
    if (!node) return;

    for (int i = 0; i < depth; i++) printf("  ");
    printf("↳ [ ");
    for (int i = 0; i < node->n; i++) {
        printf("%zu@%p(%s) ", node->sizes[i], node->blocks[i], node->is_free[i] ? "free" : "used");
    }
    printf("] %s\n", node->leaf ? "leaf" : "");

    for (int i = 0; i <= node->n; i++) {
        print_node(node->children[i], depth + 1);
    }
}

void btree_debug() {
    printf("[DEBUG] B-tree structure:\n");
    print_node(root, 0);
}

BNode* find_node(BNode* node, void* ptr, int* index) {
    if (!node) {
        printf("[btree] Node is NULL for ptr %p\n", ptr);
        return NULL;
    }

    int i = 0;
    while (i < node->n && node->blocks[i] < ptr) i++;
    if (i < node->n && node->blocks[i] == ptr) {
        *index = i;
        printf("[btree] Found block %p at index %d in node %p\n", ptr, i, node);
        return node;
    }

    if (node->leaf) {
        printf("[btree] Block %p not found in leaf node %p\n", ptr, node);
        return NULL;
    }
    return find_node(node->children[i], ptr, index);
}

void btree_remove(void* ptr) {
    int index;
    BNode* node = find_node(root, ptr, &index);
    if (node) {
        node->is_free[index] = 1;
        printf("[btree] Marked block %p as free\n", ptr);
        tree_modified = 1;
    } else {
        printf("[btree] Block %p not found\n", ptr);
    }
}

// Статическая функция для рекурсивного поиска лучшего блока
static void search_best_fit(BNode* node, size_t size, void** best_block, size_t* best_size, int* best_index, BNode** best_node) {
    if (!node) return;

    // Проверяем текущий узел
    for (int i = 0; i < node->n; i++) {
        if (node->is_free[i] && node->sizes[i] >= size) {
            if (node->sizes[i] < *best_size) {
                *best_block = node->blocks[i];
                *best_size = node->sizes[i];
                *best_index = i;
                *best_node = node;
            }
        }
    }

    // Рекурсивно проверяем дочерние узлы
    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++) {
            search_best_fit(node->children[i], size, best_block, best_size, best_index, best_node);
        }
    }
}

void* btree_find_best_fit(size_t size) {
    if (!root) return NULL;

    void* best_block = NULL;
    size_t best_size = SIZE_MAX;
    int best_index = -1;
    BNode* best_node = NULL;

    search_best_fit(root, size, &best_block, &best_size, &best_index, &best_node);

    if (best_block) {
        printf("[btree] Found best fit block %p (size %zu) for size %zu\n", best_block, best_size, size);
        best_node->is_free[best_index] = 0; // Помечаем блок как занятый
        tree_modified = 1;
        return best_block;
    }

    printf("[btree] No suitable free block found for size %zu\n", size);
    return NULL;
}