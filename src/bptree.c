#include "bptree.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int split;
    int promoted_key;
    BpNode *right;
    int inserted_new;
} InsertResult;

static BpNode *node_create(bool is_leaf)
{
    BpNode *node = calloc(1, sizeof(BpNode));

    if (node != NULL) {
        node->is_leaf = is_leaf;
    }

    return node;
}

static void node_destroy(BpNode *node)
{
    if (node == NULL) {
        return;
    }

    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) {
            node_destroy(node->children[i]);
        }
    }

    free(node);
}

void bptree_init(BPlusTree *tree)
{
    tree->root = NULL;
    tree->size = 0;
}

void bptree_destroy(BPlusTree *tree)
{
    node_destroy(tree->root);
    tree->root = NULL;
    tree->size = 0;
}

static int leaf_find_position(const BpNode *leaf, int key)
{
    int pos = 0;

    while (pos < leaf->num_keys && leaf->keys[pos] < key) {
        pos++;
    }

    return pos;
}

static InsertResult leaf_insert(BpNode *leaf, int key, size_t value)
{
    InsertResult result = {0, 0, NULL, 0};
    int pos = leaf_find_position(leaf, key);

    if (pos < leaf->num_keys && leaf->keys[pos] == key) {
        leaf->values[pos] = value;
        return result;
    }

    result.inserted_new = 1;

    if (leaf->num_keys < BPTREE_MAX_KEYS) {
        for (int i = leaf->num_keys; i > pos; i--) {
            leaf->keys[i] = leaf->keys[i - 1];
            leaf->values[i] = leaf->values[i - 1];
        }
        leaf->keys[pos] = key;
        leaf->values[pos] = value;
        leaf->num_keys++;
        return result;
    }

    int temp_keys[BPTREE_MAX_KEYS + 1];
    size_t temp_values[BPTREE_MAX_KEYS + 1];
    int total = BPTREE_MAX_KEYS + 1;

    for (int i = 0, j = 0; i < total; i++) {
        if (i == pos) {
            temp_keys[i] = key;
            temp_values[i] = value;
        } else {
            temp_keys[i] = leaf->keys[j];
            temp_values[i] = leaf->values[j];
            j++;
        }
    }

    BpNode *right = node_create(true);
    if (right == NULL) {
        result.inserted_new = 0;
        return result;
    }

    int left_count = total / 2;
    int right_count = total - left_count;

    leaf->num_keys = left_count;
    for (int i = 0; i < left_count; i++) {
        leaf->keys[i] = temp_keys[i];
        leaf->values[i] = temp_values[i];
    }

    right->num_keys = right_count;
    for (int i = 0; i < right_count; i++) {
        right->keys[i] = temp_keys[left_count + i];
        right->values[i] = temp_values[left_count + i];
    }

    right->next = leaf->next;
    leaf->next = right;

    result.split = 1;
    result.promoted_key = right->keys[0];
    result.right = right;
    return result;
}

static int child_index_for_key(const BpNode *node, int key)
{
    int idx = 0;

    while (idx < node->num_keys && key >= node->keys[idx]) {
        idx++;
    }

    return idx;
}

static InsertResult node_insert(BpNode *node, int key, size_t value)
{
    if (node->is_leaf) {
        return leaf_insert(node, key, value);
    }

    int child_idx = child_index_for_key(node, key);
    InsertResult child_result = node_insert(node->children[child_idx], key, value);

    if (!child_result.split) {
        return child_result;
    }

    InsertResult result = {0, 0, NULL, child_result.inserted_new};

    if (node->num_keys < BPTREE_MAX_KEYS) {
        for (int i = node->num_keys; i > child_idx; i--) {
            node->keys[i] = node->keys[i - 1];
        }
        for (int i = node->num_keys + 1; i > child_idx + 1; i--) {
            node->children[i] = node->children[i - 1];
        }
        node->keys[child_idx] = child_result.promoted_key;
        node->children[child_idx + 1] = child_result.right;
        node->num_keys++;
        return result;
    }

    int temp_keys[BPTREE_MAX_KEYS + 1];
    BpNode *temp_children[BPTREE_MAX_KEYS + 2];
    int total_keys = BPTREE_MAX_KEYS + 1;

    for (int i = 0, j = 0; i < total_keys; i++) {
        if (i == child_idx) {
            temp_keys[i] = child_result.promoted_key;
        } else {
            temp_keys[i] = node->keys[j];
            j++;
        }
    }

    for (int i = 0, j = 0; i < total_keys + 1; i++) {
        if (i == child_idx + 1) {
            temp_children[i] = child_result.right;
        } else {
            temp_children[i] = node->children[j];
            j++;
        }
    }

    BpNode *right = node_create(false);
    if (right == NULL) {
        result.inserted_new = 0;
        return result;
    }

    int mid = total_keys / 2;
    int left_keys = mid;
    int right_keys = total_keys - mid - 1;

    node->num_keys = left_keys;
    for (int i = 0; i < left_keys; i++) {
        node->keys[i] = temp_keys[i];
    }
    for (int i = 0; i <= left_keys; i++) {
        node->children[i] = temp_children[i];
    }
    for (int i = left_keys + 1; i < BPTREE_MAX_KEYS + 1; i++) {
        node->children[i] = NULL;
    }

    right->num_keys = right_keys;
    for (int i = 0; i < right_keys; i++) {
        right->keys[i] = temp_keys[mid + 1 + i];
    }
    for (int i = 0; i <= right_keys; i++) {
        right->children[i] = temp_children[mid + 1 + i];
    }

    result.split = 1;
    result.promoted_key = temp_keys[mid];
    result.right = right;
    return result;
}

int bptree_insert(BPlusTree *tree, int key, size_t value)
{
    if (tree->root == NULL) {
        tree->root = node_create(true);
        if (tree->root == NULL) {
            return 0;
        }
    }

    InsertResult result = node_insert(tree->root, key, value);

    if (result.inserted_new) {
        tree->size++;
    }

    if (!result.split) {
        return 1;
    }

    BpNode *new_root = node_create(false);
    if (new_root == NULL) {
        return 0;
    }

    new_root->num_keys = 1;
    new_root->keys[0] = result.promoted_key;
    new_root->children[0] = tree->root;
    new_root->children[1] = result.right;
    tree->root = new_root;
    return 1;
}

bool bptree_search(const BPlusTree *tree, int key, size_t *value_out)
{
    BpNode *node = tree->root;

    while (node != NULL && !node->is_leaf) {
        int idx = child_index_for_key(node, key);
        node = node->children[idx];
    }

    if (node == NULL) {
        return false;
    }

    int pos = leaf_find_position(node, key);
    if (pos < node->num_keys && node->keys[pos] == key) {
        if (value_out != NULL) {
            *value_out = node->values[pos];
        }
        return true;
    }

    return false;
}
