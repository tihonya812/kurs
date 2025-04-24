#include "b_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

extern void* __real_malloc(size_t size);
extern void __real_free(void* ptr);

BNode* root = NULL;

static BNode* create_node(int leaf) {
    BNode* node = __real_malloc(sizeof(BNode));
    if (!node) return NULL;
    node->n = 0;
    node->leaf = leaf;
    for (int i = 0; i < 2*T; i++) {
        node->children[i] = NULL;
    }
    return node;
}

static void split_child(BNode* parent, int i, BNode* child) {
    BNode* new_node = create_node(child->leaf);
    new_node->n = T-1;

    // Копируем вторую половину ключей и блоков в новый узел
    for (int j = 0; j < T-1; j++) {
        new_node->sizes[j] = child->sizes[j+T];
        new_node->blocks[j] = child->blocks[j+T];
        new_node->is_free[j] = child->is_free[j+T];
    }

    // Копируем дочерние узлы, если не лист
    if (!child->leaf) {
        for (int j = 0; j < T; j++) {
            new_node->children[j] = child->children[j+T];
            child->children[j+T] = NULL;
        }
    }

    child->n = T-1;

    // Сдвигаем ключи и дочерние узлы в родителе
    for (int j = parent->n; j > i; j--) {
        parent->children[j+1] = parent->children[j];
    }
    for (int j = parent->n-1; j >= i; j--) {
        parent->sizes[j+1] = parent->sizes[j];
        parent->blocks[j+1] = parent->blocks[j];
        parent->is_free[j+1] = parent->is_free[j];
    }

    // Добавляем средний ключ и новый узел в родителя
    parent->children[i+1] = new_node;
    parent->sizes[i] = child->sizes[T-1];
    parent->blocks[i] = child->blocks[T-1];
    parent->is_free[i] = child->is_free[T-1];
    parent->n++;
}

static void insert_nonfull(BNode* node, size_t size, void* ptr) {
    int i = node->n - 1;

    if (node->leaf) {
        // Сдвигаем ключи, пока не найдём место
        while (i >= 0 && node->sizes[i] > size) {
            node->sizes[i+1] = node->sizes[i];
            node->blocks[i+1] = node->blocks[i];
            node->is_free[i+1] = node->is_free[i];
            i--;
        }
        node->sizes[i+1] = size;
        node->blocks[i+1] = ptr;
        node->is_free[i+1] = 0;
        node->n++;
    } else {
        // Находим дочерний узел
        while (i >= 0 && node->sizes[i] > size) i--;
        i++;
        if (node->children[i]->n == 2*T-1) {
            split_child(node, i, node->children[i]);
            if (node->sizes[i] < size) i++;
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
    if (!node) return NULL;

    int i = 0;
    while (i < node->n && node->blocks[i] < ptr) i++;
    if (i < node->n && node->blocks[i] == ptr) {
        *index = i;
        return node;
    }

    if (node->leaf) return NULL;
    return find_node(node->children[i], ptr, index);
}

void btree_remove(void* ptr) {
    int index;
    BNode* node = find_node(root, ptr, &index);
    if (node) {
        node->is_free[index] = 1;
        printf("[btree] Marked block %p as free\n", ptr);
    }
}

void* btree_find_best_fit(size_t size) {
    BNode* current = root;
    void* best_block = NULL;
    size_t best_size = SIZE_MAX;

    // Обходим дерево в глубину
    while (current) {
        for (int i = 0; i < current->n; i++) {
            if (current->is_free[i] && current->sizes[i] >= size && current->sizes[i] < best_size) {
                best_block = current->blocks[i];
                best_size = current->sizes[i];
            }
        }
        if (!current->leaf) {
            for (int i = 0; i <= current->n; i++) {
                // Рекурсивно проверяем все дочерние узлы
                void* candidate = btree_find_best_fit(size);
                if (candidate && best_size > size) {
                    best_block = candidate;
                    best_size = size;
                }
            }
        }
        break; // Пока обходим только корень, нужно доработать для полного обхода
    }

    if (best_block) {
        int index;
        BNode* node = find_node(root, best_block, &index);
        if (node) node->is_free[index] = 0;
    }
    return best_block;
}