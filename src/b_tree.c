#include "b_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

BNode* root = NULL;
int tree_modified = 0;

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
        node->blocks[i] = NULL; // Initialize block pointers to NULL
        node->is_free[i] = 0;
    }
    printf("[btree] Created node %p (leaf=%d)\n", node, leaf);
    tree_modified = 1;
    return node;
}

static void split_child(BNode* parent, int i, BNode* child) {
    BNode* new_node = create_node(child->leaf);
    if (!new_node) {
        // Handle allocation failure for new_node if necessary
        perror("[btree] Failed to create node in split_child");
        return;
    }
    new_node->n = T-1;

    for (int j = 0; j < T-1; j++) {
        new_node->sizes[j] = child->sizes[j+T];
        new_node->blocks[j] = child->blocks[j+T];
        new_node->is_free[j] = child->is_free[j+T];
    }

    if (!child->leaf) {
        for (int j = 0; j < T; j++) {
            new_node->children[j] = child->children[j+T];
            child->children[j+T] = NULL; // Nullify moved children pointers
        }
    }

    child->n = T-1; // child now has T-1 keys

    // Shift children in parent to make space for new_node
    for (int j = parent->n; j > i; j--) { // Note: j > i, not j >= i
        parent->children[j+1] = parent->children[j];
    }
    parent->children[i+1] = new_node; // Link new_node

    // Shift keys in parent
    for (int j = parent->n-1; j >= i; j--) {
        parent->sizes[j+1] = parent->sizes[j];
        parent->blocks[j+1] = parent->blocks[j];
        parent->is_free[j+1] = parent->is_free[j];
    }

    // Copy middle key from child to parent
    parent->sizes[i] = child->sizes[T-1];
    parent->blocks[i] = child->blocks[T-1];
    parent->is_free[i] = child->is_free[T-1];
    parent->n++;

    // Nullify the moved key in the original child (optional, but good practice)
    // child->blocks[T-1] = NULL; // As its content is now in parent

    printf("[btree] Split child %p at index %d, new node %p\n", child, i, new_node);
    tree_modified = 1;
}

static void insert_nonfull(BNode* node, size_t size, void* ptr) {
    int i = node->n - 1;

    if (node->leaf) {
        // Find location for new key and shift greater keys
        while (i >= 0 && node->blocks[i] > ptr) { // Assuming B-Tree ordered by block addresses
            node->sizes[i+1] = node->sizes[i];
            node->blocks[i+1] = node->blocks[i];
            node->is_free[i+1] = node->is_free[i];
            i--;
        }
        node->sizes[i+1] = size;
        node->blocks[i+1] = ptr;
        node->is_free[i+1] = 0; // Mark as used
        node->n++;
        printf("[btree] Inserted block %p (size %zu) into leaf node %p\n", ptr, size, node);
        tree_modified = 1;
    } else {
        // Find child to insert into
        while (i >= 0 && node->blocks[i] > ptr) i--;
        i++; // Child index

        if (node->children[i]->n == 2*T-1) { // If child is full
            split_child(node, i, node->children[i]);
            // Determine which child the key now goes into after split
            if (node->blocks[i] < ptr) i++;
        }
        insert_nonfull(node->children[i], size, ptr);
    }
}

void btree_insert(size_t size, void* ptr) {
    if (!root) {
        root = create_node(1); // Create root as leaf
        if(!root) {
            perror("[btree] Failed to create root node");
            return;
        }
        root->sizes[0] = size;
        root->blocks[0] = ptr;
        root->is_free[0] = 0;
        root->n = 1;
        printf("[btree] Inserted block %p (size %zu) as root\n", ptr, size);
        tree_modified = 1;
        return;
    }

    if (root->n == 2*T-1) { // If root is full
        BNode* new_root = create_node(0); // New root is internal
        if(!new_root) {
             perror("[btree] Failed to create new root node during split");
            return;
        }
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

    for (int k = 0; k < depth; k++) printf("  ");
    printf("↳ (%p) [ ", node); // Print node address for debugging
    for (int k = 0; k < node->n; k++) {
        printf("%zu@%p(%s) ", node->sizes[k], node->blocks[k], node->is_free[k] ? "free" : "used");
    }
    printf("] %s (n=%d)\n", node->leaf ? "leaf" : "internal", node->n);

    if (!node->leaf) {
        for (int k = 0; k <= node->n; k++) {
            print_node(node->children[k], depth + 1);
        }
    }
}

void btree_debug() {
    printf("[DEBUG] B-tree structure (T=%d):\n", T);
    print_node(root, 0);
}

static int find_key_or_subtree(BNode* node, void* ptr, int* found_in_node) {
    int i = 0;
    *found_in_node = 0;
    while (i < node->n) {
        if (node->blocks[i] == ptr) { // Сначала проверяем на точное совпадение
            *found_in_node = 1;
            return i;
        }
        if (node->blocks[i] > ptr) { // Предполагаем, что указатели можно так сравнивать для порядка
            break; // ptr должен быть в children[i]
        }
        i++;
    }
    // Если дошли до конца или нашли ключ больше (i указывает на место/ребенка)
    return i;
}


BNode* find_node(BNode* current_node, void* ptr, int* index_in_node) {
    if (!current_node) {
        // printf("[btree] find_node: current_node is NULL for ptr %p\n", ptr);
        return NULL;
    }

    int found_here;
    int i = find_key_or_subtree(current_node, ptr, &found_here);

    if (found_here) { // Key is in current_node
        *index_in_node = i;
        printf("[btree] Found block %p at index %d in node %p\n", ptr, i, current_node);
        return current_node;
    }

    // Key not in current_node
    if (current_node->leaf) { // And it's a leaf
        // printf("[btree] Block %p not found in leaf node %p\n", ptr, current_node);
        return NULL; // Key not found
    }

    return find_node(current_node->children[i], ptr, index_in_node);
}


static void borrow_from_prev(BNode* parent_node, int child_idx) {
    BNode* child = parent_node->children[child_idx];
    BNode* sibling = parent_node->children[child_idx-1];

    // Shift all keys and children in child one step to the right
    for (int i = child->n-1; i >= 0; i--) {
        child->sizes[i+1] = child->sizes[i];
        child->blocks[i+1] = child->blocks[i];
        child->is_free[i+1] = child->is_free[i];
    }
    if (!child->leaf) {
        for (int i = child->n; i >= 0; i--) {
            child->children[i+1] = child->children[i];
        }
    }

    // Move key from parent to child
    child->sizes[0] = parent_node->sizes[child_idx-1];
    child->blocks[0] = parent_node->blocks[child_idx-1];
    child->is_free[0] = parent_node->is_free[child_idx-1];

    // Move rightmost child of sibling to child's first child pointer
    if (!child->leaf) {
        child->children[0] = sibling->children[sibling->n];
        sibling->children[sibling->n] = NULL; // Nullify moved pointer
    }

    // Move rightmost key of sibling to parent
    parent_node->sizes[child_idx-1] = sibling->sizes[sibling->n-1];
    parent_node->blocks[child_idx-1] = sibling->blocks[sibling->n-1];
    parent_node->is_free[child_idx-1] = sibling->is_free[sibling->n-1];

    child->n++;
    sibling->n--;
    tree_modified = 1;
    printf("[btree] Borrowed from prev sibling for child %p at index %d\n", child, child_idx);
}

static void borrow_from_next(BNode* parent_node, int child_idx) {
    BNode* child = parent_node->children[child_idx];
    BNode* sibling = parent_node->children[child_idx+1];

    // Move key from parent to child's last key
    child->sizes[child->n] = parent_node->sizes[child_idx];
    child->blocks[child->n] = parent_node->blocks[child_idx];
    child->is_free[child->n] = parent_node->is_free[child_idx];

    // Move leftmost child of sibling to child's last child pointer
    if (!child->leaf) {
        child->children[child->n+1] = sibling->children[0];
        // sibling->children[0] = NULL; // Will be shifted
    }

    // Move leftmost key of sibling to parent
    parent_node->sizes[child_idx] = sibling->sizes[0];
    parent_node->blocks[child_idx] = sibling->blocks[0];
    parent_node->is_free[child_idx] = sibling->is_free[0];

    // Shift keys and children in sibling one step to the left
    for (int i = 0; i < sibling->n-1; i++) {
        sibling->sizes[i] = sibling->sizes[i+1];
        sibling->blocks[i] = sibling->blocks[i+1];
        sibling->is_free[i] = sibling->is_free[i+1];
    }
    if (!sibling->leaf) {
        for (int i = 0; i < sibling->n; i++) { // up to n, not n+1
            sibling->children[i] = sibling->children[i+1];
        }
         sibling->children[sibling->n] = NULL; // Nullify last moved child ptr
    }


    child->n++;
    sibling->n--;
    tree_modified = 1;
    printf("[btree] Borrowed from next sibling for child %p at index %d\n", child, child_idx);
}
// В merge_nodes, убедимся, что индексы для children верны:
static void merge_nodes(BNode* parent_node, int idx_of_key_in_parent) {
    BNode* child = parent_node->children[idx_of_key_in_parent]; // Это левый ребенок (в который сливаем)
    BNode* sibling = parent_node->children[idx_of_key_in_parent+1]; // Это правый ребенок (из которого сливаем)

    // Сохраняем исходное количество ключей/детей в 'child' перед тем, как он начнет расти
    int original_child_n = child->n; // Должно быть T-1

    // 1. Ключ из parent_node спускается в child
    child->sizes[original_child_n] = parent_node->sizes[idx_of_key_in_parent];
    child->blocks[original_child_n] = parent_node->blocks[idx_of_key_in_parent];
    child->is_free[original_child_n] = parent_node->is_free[idx_of_key_in_parent];
    child->n++; // Теперь у child original_child_n + 1 ключей (T ключей)

    // 2. Ключи из sibling копируются в child
    for (int i = 0; i < sibling->n; i++) {
        child->sizes[child->n] = sibling->sizes[i];
        child->blocks[child->n] = sibling->blocks[i];
        child->is_free[child->n] = sibling->is_free[i];
        child->n++;
    }
    // Теперь у child original_child_n + 1 + sibling->n ключей ( T-1 + 1 + T-1 = 2T-1 ключей)

    // 3. Если не листья, дети из sibling копируются в child
    if (!child->leaf) {
        // У child было original_child_n+1 детей (T детей) до копирования детей sibling.
        // Индекс первого нового ребенка в child будет original_child_n + 1.
        for (int i = 0; i <= sibling->n; i++) {
            child->children[original_child_n + 1 + i] = sibling->children[i];
            sibling->children[i] = NULL; // Обнуляем указатель в sibling
        }
    }

    // 4. Удаляем ключ и указатель на sibling из parent_node
    for (int i = idx_of_key_in_parent; i < parent_node->n - 1; i++) {
        parent_node->sizes[i] = parent_node->sizes[i+1];
        parent_node->blocks[i] = parent_node->blocks[i+1];
        parent_node->is_free[i] = parent_node->is_free[i+1];
    }
    // Сдвигаем указатели на детей в parent_node
    for (int i = idx_of_key_in_parent + 1; i < parent_node->n; i++) { // Начиная с указателя ПОСЛЕ удаленного sibling
        parent_node->children[i] = parent_node->children[i+1];
    }
    parent_node->children[parent_node->n] = NULL;
    parent_node->n--;


    printf("[btree] Merged child %p (was children[%d]) and sibling %p. Freed sibling %p.\n",
           child, idx_of_key_in_parent, sibling, sibling);
    free(sibling); 
    tree_modified = 1;
}


// Вызывается для parent_node, для его дочернего узла по child_idx_in_parent, который может быть недозаполнен.
static void fix_underflow(BNode* parent_node, int child_idx_in_parent) {
    if (!parent_node) return; // Не должно происходить при правильном вызове
    BNode* child = parent_node->children[child_idx_in_parent];

    if (!child || child->n >= T) {
        // Если узел имеет T или более ключей, он не недозаполнен согласно цели.
        return;
    }

    printf("[btree] Fixing underflow for child %p (index %d in parent %p), current n=%d\n", child, child_idx_in_parent, parent_node, child->n);

    // Случай 1: Заимствование у предыдущего брата
    if (child_idx_in_parent > 0 && parent_node->children[child_idx_in_parent-1]->n >= T) {
        borrow_from_prev(parent_node, child_idx_in_parent);
    }
    // Случай 2: Заимствование у следующего брата
    else if (child_idx_in_parent < parent_node->n && parent_node->children[child_idx_in_parent+1]->n >= T) {
        borrow_from_next(parent_node, child_idx_in_parent);
    }
    // Случай 3: Слияние
    else {
        // Определяем, с каким братом сливаться (предпочтительнее правый, если доступен, иначе левый)
        if (child_idx_in_parent < parent_node->n) { // Слияние с правым братом
            merge_nodes(parent_node, child_idx_in_parent);
        } else if (child_idx_in_parent > 0) { // Слияние с левым братом
            merge_nodes(parent_node, child_idx_in_parent-1);
        }
        if (parent_node == root && parent_node->n == 0) {
            // После merge_nodes, child - это объединенный узел, который должен быть единственным дочерним.
            root = parent_node->children[0];
            free(parent_node); // Освобождаем старый корень
            printf("[btree] New root is %p\n", root);
            tree_modified = 1;
        }
    }
}

static void remove_entry_from_leaf(BNode* leaf_node, int index_in_leaf, int actually_free_payload) {
    if (!leaf_node || !leaf_node->leaf || index_in_leaf < 0 || index_in_leaf >= leaf_node->n) {
        printf("[btree] Invalid args to remove_entry_from_leaf: node %p, index %d, n %d\n", leaf_node, index_in_leaf, leaf_node ? leaf_node->n : -1);
        return;
    }

    void* block_ptr_in_leaf = leaf_node->blocks[index_in_leaf];

    if (actually_free_payload && block_ptr_in_leaf) {
        printf("[btree] System freeing block %p from leaf %p at index %d (flag=true)\n", block_ptr_in_leaf, leaf_node, index_in_leaf);
        free(block_ptr_in_leaf);
    } else if (!actually_free_payload && block_ptr_in_leaf) {
        printf("[btree] Removing entry for block %p from leaf %p at index %d (flag=false, block NOT freed here)\n", block_ptr_in_leaf, leaf_node, index_in_leaf);
    }
    
    leaf_node->blocks[index_in_leaf] = NULL; // Запись удаляется в любом случае

    for (int i = index_in_leaf; i < leaf_node->n - 1; i++) {
        leaf_node->sizes[i] = leaf_node->sizes[i + 1];
        leaf_node->blocks[i] = leaf_node->blocks[i + 1];
        leaf_node->is_free[i] = leaf_node->is_free[i + 1];
    }
    leaf_node->sizes[leaf_node->n - 1] = 0;
    leaf_node->blocks[leaf_node->n - 1] = NULL;
    leaf_node->is_free[leaf_node->n - 1] = 0;
    leaf_node->n--;
    tree_modified = 1;
}

// Gets predecessor: finds rightmost key in subtree rooted at node->children[child_idx_for_key]
static void get_predecessor(BNode* parent_node, int child_idx_for_key, BNode** pred_node_out, int* pred_idx_out) {
    BNode* current = parent_node->children[child_idx_for_key];
    while (!current->leaf) {
        current = current->children[current->n]; // Go to rightmost child
    }
    *pred_node_out = current;
    *pred_idx_out = current->n - 1; // Rightmost key in leaf
}

// Gets successor: finds leftmost key in subtree rooted at node->children[child_idx_for_key + 1]
static void get_successor(BNode* parent_node, int child_idx_for_key, BNode** succ_node_out, int* succ_idx_out) {
    BNode* current = parent_node->children[child_idx_for_key + 1];
    while (!current->leaf) {
        current = current->children[0]; // Go to leftmost child
    }
    *succ_node_out = current;
    *succ_idx_out = 0; // Leftmost key in leaf
}

static void btree_remove_recursive(BNode* current_node, void* ptr_to_delete, int actually_free_payload) {
    if (!current_node) return;

    int found_in_current;
    int idx = find_key_or_subtree(current_node, ptr_to_delete, &found_in_current);

    if (found_in_current) { // Ключ ptr_to_delete находится в current_node
        if (current_node->leaf) {
            printf("[btree_rec] Removing %p from leaf %p at index %d (free_payload=%d)\n", ptr_to_delete, current_node, idx, actually_free_payload);
            remove_entry_from_leaf(current_node, idx, actually_free_payload); // Используем флаг
        } else { // Ключ во внутреннем узле current_node
            printf("[btree_rec] Removing %p from internal node %p at index %d (free_payload=%d)\n", ptr_to_delete, current_node, idx, actually_free_payload);
            BNode* left_child = current_node->children[idx];
            BNode* right_child = current_node->children[idx+1];

            if (left_child && left_child->n >= T) { // Случай 1: Предшественник из левого ребенка
                BNode* pred_node;
                int pred_idx;
                get_predecessor(current_node, idx, &pred_node, &pred_idx);

                // Блок ptr_to_delete (== current_node->blocks[idx]) заменяется.
                // Освобождаем его, если текущий рекурсивный вызов должен это сделать.
                if (actually_free_payload) {
                     printf("[btree_rec] System freeing current internal block %p (ptr_to_delete) before replacing with pred %p\n", current_node->blocks[idx], pred_node->blocks[pred_idx]);
                     if (current_node->blocks[idx] == ptr_to_delete) { // Доп. проверка, что освобождаем именно то, что должны
                        free(current_node->blocks[idx]);
                     } else {
                        printf("[btree_rec] WARNING: Mismatch! ptr_to_delete %p != current_node->blocks[idx] %p\n", ptr_to_delete, current_node->blocks[idx]);
                        // Возможно, стоит освободить ptr_to_delete, если он гарантированно является тем, что нужно
                     }
                }

                // Заменяем ключ в current_node на предшественника
                current_node->sizes[idx] = pred_node->sizes[pred_idx];
                current_node->blocks[idx] = pred_node->blocks[pred_idx]; // Блок предшественника перемещается вверх
                current_node->is_free[idx] = pred_node->is_free[pred_idx];
                
                btree_remove_recursive(left_child, current_node->blocks[idx], 0 /* НЕ освобождать этот payload */);

            } else if (right_child && right_child->n >= T) { // Случай 2: Преемник из правого ребенка
                BNode* succ_node;
                int succ_idx;
                get_successor(current_node, idx, &succ_node, &succ_idx);

                if (actually_free_payload) {
                    printf("[btree_rec] System freeing current internal block %p (ptr_to_delete) before replacing with succ %p\n", current_node->blocks[idx], succ_node->blocks[succ_idx]);
                     if (current_node->blocks[idx] == ptr_to_delete) {
                        free(current_node->blocks[idx]);
                     } else {
                        printf("[btree_rec] WARNING: Mismatch! ptr_to_delete %p != current_node->blocks[idx] %p\n", ptr_to_delete, current_node->blocks[idx]);
                     }
                }

                current_node->sizes[idx] = succ_node->sizes[succ_idx];
                current_node->blocks[idx] = succ_node->blocks[succ_idx]; // Блок преемника перемещается вверх
                current_node->is_free[idx] = succ_node->is_free[succ_idx];
                
                btree_remove_recursive(right_child, current_node->blocks[idx], 0 /* НЕ освобождать этот payload */);

            } else { // Случай 3: Слияние левого ребенка, ключа из current_node и правого ребенка
                void* key_to_delete_in_merged_child = current_node->blocks[idx]; // Это ptr_to_delete

                printf("[btree_rec] Merging children of node %p around key %p (idx %d)\n", current_node, key_to_delete_in_merged_child, idx);
                                
                merge_nodes(current_node, idx); 
                
                btree_remove_recursive(current_node->children[idx], key_to_delete_in_merged_child, actually_free_payload);
            }
        }
    } else { // Ключ ptr_to_delete не в current_node, должен быть в дочернем узле current_node->children[idx]
        if (!found_in_current) { // Явно используем флаг, чтобы отделить от предыдущего if-блока
            if (current_node->leaf) {
                // Если мы здесь, значит find_node нашел блок, а рекурсивный поиск - нет, или это ошибка логики.
                printf("[btree_rec] Ptr %p NOT found by find_key_or_subtree in leaf %p (idx %d), but should exist. Aborting path.\n", ptr_to_delete, current_node, idx);
                return;
            }
    
            // Определяем, в какого ребенка спускаться (idx уже есть от find_key_or_subtree)
            BNode* child_to_descend = current_node->children[idx];
    
            if (!child_to_descend) { // Такого быть не должно в корректном B-дереве, если узел не листовой
                 printf("[btree_rec] Error: child_to_descend (children[%d]) is NULL for ptr %p at non-leaf node %p.\n", idx, ptr_to_delete, current_node);
                 return;
            }
    
            // Если у дочернего узла, в который мы собираемся спуститься, T-1 ключей (минимальное количество)
            if (child_to_descend->n < T) { // T-1 ключ == child->n == T-1. Если < T, значит, child->n == T-1
                printf("[btree_rec] Child %p (idx %d in parent %p) has n=%d (T-1 keys), calling fix_underflow to ensure it has >= T keys or is merged.\n", child_to_descend, idx, current_node, child_to_descend->n);
                fix_underflow(current_node, idx); // fix_underflow вызывается для родителя current_node, чтобы исправить его ребенка children[idx]
                btree_remove_recursive(current_node, ptr_to_delete, actually_free_payload);
                return; // Важно! Предотвращаем двойной спуск.
            }
            // Если у ребенка достаточно ключей (>= T), просто спускаемся
            btree_remove_recursive(child_to_descend, ptr_to_delete, actually_free_payload);
        }
    } 
}

// Основная функция удаления
void btree_remove(void* ptr) {
    if (!root) {
        printf("[btree] Tree is empty, cannot remove %p\n", ptr);
        return;
    }

    int temp_idx;
    BNode* node_check = find_node(root, ptr, &temp_idx); // find_node должен быть надежным
    if (!node_check || (node_check->blocks[temp_idx] != ptr && !node_check->is_free[temp_idx])) { // Добавил !is_free для более точной проверки "не найден"
        printf("[btree] Block %p not found in tree or consistency issue. Cannot remove.\n", ptr);
        return;
    }
    // Если блок найден, но помечен как is_free, это может быть состояние для субаллокатора.
    // btree_remove подразумевает полное удаление и освобождение.
    // if (node_check->is_free[temp_idx]) {
    //     printf("[btree] Block %p is marked as 'is_free'. Proceeding with full removal.\n", ptr);
    // }

    printf("[btree] Attempting to remove block %p from tree.\n", ptr);
    btree_remove_recursive(root, ptr, 1 /* initial call: actually_free_payload is true */);

    if (root && root->n == 0 && !root->leaf && root->children[0]) {
        BNode* old_root = root;
        root = root->children[0];
        printf("[btree] Root %p became empty, new root is child %p.\n", old_root, root);
        free(old_root);
        tree_modified = 1;
    } else if (root && root->n == 0 && root->leaf) {
        printf("[btree] Root (leaf) %p became empty. Tree is now empty.\n", root);
        free(root);
        root = NULL;
        tree_modified = 1;
    }
    printf("[btree] Finished removal of block %p.\n", ptr);
}


void btree_full_free(void* ptr) {
    btree_remove(ptr); // btree_remove now handles full deallocation
}

void btree_cleanup_node(BNode* node) {
    if (!node) return;

    // Recursively cleanup children first
    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++) {
            btree_cleanup_node(node->children[i]);
            node->children[i] = NULL; // Good practice
        }
    }

    // Free blocks held by this node
    for (int i = 0; i < node->n; i++) {
        if (node->blocks[i]) {
            printf("[btree_cleanup] Freeing block %p (size %zu, is_free %d) from node %p\n",
                   node->blocks[i], node->sizes[i], node->is_free[i], node);
            free(node->blocks[i]); // System free the data block
            node->blocks[i] = NULL;
        }
    }
    // Free the B-Tree node structure itself
    printf("[btree_cleanup] Freeing BNode structure %p\n", node);
    free(node);
}

void btree_cleanup() {
    if (root) {
        btree_cleanup_node(root);
        root = NULL;
        tree_modified = 1; // Indicate tree structure changed (it's gone)
        printf("[btree] Cleaned up B-tree.\n");
    } else {
        printf("[btree] Cleanup called on an empty tree.\n");
    }
}

static void search_best_fit_recursive(BNode* node, size_t size,
                                      void** best_block_ptr, size_t* best_diff_ptr,
                                      int* best_index_ptr, BNode** best_node_ptr) {
    if (!node) return;

    for (int i = 0; i < node->n; i++) {
        if (node->is_free[i] && node->blocks[i] && node->sizes[i] >= size) {
            size_t current_diff = node->sizes[i] - size;
            if (current_diff < *best_diff_ptr) {
                *best_block_ptr = node->blocks[i];
                *best_diff_ptr = current_diff;
                *best_index_ptr = i;
                *best_node_ptr = node;
                if (current_diff == 0) return; // Exact fit found, no need to search further
            }
        }
    }

    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++) {
            // Pruning: if best_diff is already 0 (exact fit), stop.
            if (*best_diff_ptr == 0) return;
            search_best_fit_recursive(node->children[i], size, best_block_ptr, best_diff_ptr, best_index_ptr, best_node_ptr);
        }
    }
}

void* btree_find_best_fit(size_t size) {
    if (!root || size == 0) return NULL;

    void* best_block = NULL;
    size_t best_diff = SIZE_MAX; // Using SIZE_MAX from <stdint.h>
    int best_index = -1;
    BNode* best_node = NULL;

    search_best_fit_recursive(root, size, &best_block, &best_diff, &best_index, &best_node);

    if (best_block) {
        printf("[btree] Found best fit block %p (actual size %zu) for requested size %zu in node %p at index %d.\n",
               best_block, best_node->sizes[best_index], size, best_node, best_index);
        best_node->is_free[best_index] = 0; // Mark as used
        // Optionally, if the chosen block is much larger, consider splitting it
        // and adding the remainder as a new free block (more complex).
        tree_modified = 1;
        return best_block;
    }

    printf("[btree] No suitable free block found for size %zu.\n", size);
    return NULL;
}