#ifndef BPTREE_H
#define BPTREE_H

#include <stddef.h>
#include <stdbool.h>

#define BPTREE_MAX_KEYS 31

typedef struct BpNode {
    bool is_leaf;
    int num_keys;
    int keys[BPTREE_MAX_KEYS];
    struct BpNode *children[BPTREE_MAX_KEYS + 1];
    size_t values[BPTREE_MAX_KEYS];
    struct BpNode *next;
} BpNode;

typedef struct {
    BpNode *root;
    size_t size;
} BPlusTree;

void bptree_init(BPlusTree *tree);
void bptree_destroy(BPlusTree *tree);
int bptree_insert(BPlusTree *tree, int key, size_t value);
bool bptree_search(const BPlusTree *tree, int key, size_t *value_out);

#endif
