#include "set.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Set, implemented as B tree. See https://en.wikipedia.org/wiki/B-tree
 */

typedef struct set_node {
    set *home_set; // pointer to the set that this is a part of
    uintptr_t data; // pointer to data
    struct set_node *parent;
    struct set_node **children; //NULL if leaf
    uint8_t n_keys; // number of keys stored in the node. Max = tree order - 1.
                    // Number of children = number of keys + 1
} set_node;

struct set {
    size_t elem_size;
    set_less_t less;
    set_node* root;
    uint8_t order; // Knuth order of tree. Equal to max number of children
};

/* Sets home_set, parent, n_keys, and allocates space for data. If not leaf,
 * also allocates space for children. Otherwise sets chilren to NULL
 * NOTES:
 *   - data remains unititialized.
 *   - children, if not NULL, remains unititialized.
 */
void set_node_init(set_node *node, set *home_set, set_node* parent,
        size_t n_keys, bool is_leaf) {
    uint8_t order = home_set->order;
    node->home_set = home_set;
    node->data = (uintptr_t)calloc(order - 1, home_set->elem_size);
    node->children = is_leaf ? NULL : calloc(order, home_set -> elem_size);
    node->parent = parent;
    node->n_keys = n_keys;
}

void set_init(set *s, uint8_t order, set_less_t less, size_t elem_size) {
    s->elem_size = elem_size;
    s->less = less;
    s->root = NULL;
    s->order = order;
}

void set_tree_free(set_node *node) {
    // recursively free children
    for (size_t child = 0; child < node->n_keys + 1; ++child) {
        set_tree_free(node->children[child]);
    }
    // free stored data
    free((void *)node->data);
}

void set_free(set *s) {
    set_tree_free(s->root);
}

bool set_tree_contains(set_node *node, void *elem, size_t elem_size,
        void *copy_out, set_less_t less) {
    size_t elem_index; // point where elem would go in data
    void *stored; // pointer to stored data at elem_index
    for(size_t elem_index = 0; elem_index < node->n_keys; ++elem_index) {
        if (less(stored, elem)) {
            stored = (void *)(node->data + elem_index * elem_size);
            break;
        }
    }
    if (less(elem, stored)) { // stored == elem; we've found it
        if(copy_out) {
            memcpy(copy_out, stored, elem_size);
        }
        return true;
    }
    if (node->children) { // elem in children[elem_index]
        return set_tree_contains(node->children[elem_index], elem, elem_size,
                copy_out, less);
    }
    else { // we are leaf, elem not found
        return false;
    }
}

bool set_contains(set *s, void *elem, void *copy_out) {
    if (s->root) {
        return set_tree_contains(s->root, elem, s->elem_size, copy_out,
                s->less);
    }
    else { // empty set
        return false;
    }
}

// forward declare insertion
void set_insert_in_node(set_node* node, void *elem, size_t elem_size,
        set_node *right_child, set_less_t less);

// Simple insert case: node not full so just insert in current node
void set_insert_in_node_simple(set_node* node, void *elem, size_t elem_size,
        size_t elem_index, set_node *right_child, set_less_t less) {
    ++node->n_keys;
    uintptr_t elem_addr = node->data + elem_index * elem_size;
    // move elements after target
    memmove((void *)(elem_addr + elem_size), (void *)elem_addr,
            (node->n_keys - elem_index) * elem_size);
    memcpy((void *)elem_addr, elem, elem_size);
    if (node->children) {
        node->children[node->n_keys] = right_child;
    }
}

// Complex insert case: split this node in two, and insert median value into
// parent node. If the max number of keys is odd, left node will end up with
// one more key than the right node. This reduces copying, and left-biases the
// data (which is slightly faster since we are using less for querying)
void set_insert_in_node_complex(set_node* node, void *elem, size_t elem_size,
        size_t elem_index, set_node *right_child, set_less_t less) {

    size_t n_old = (node->n_keys - 1) / 2 + 1; // ceiling of max_keys / 2
    size_t n_new = node->n_keys - n_old;
    // pivot is first elem which gets moved to right node (left insert)
    // or elem before first which gets moved to right node (right insert)
    uintptr_t pivot_addr = node->data + n_old * elem_size;
    // address where elem would appear in old array
    uintptr_t elem_addr = node->data + elem_index * elem_size;

    // allocate new right node. Current node becomes left node
    set_node *new_node = calloc(1, sizeof(set_node));
    set_node_init(new_node, node->home_set, node->parent, n_new, false);

    // copy data. Moved element ends up at pivot address
    if (elem_index <= n_old) { // elem goes in left (old) half
        // copy (pivot to end-of-data) to new node
        memcpy((void *)new_node->data, (void *)pivot_addr, n_new * elem_size);
        // memmove (elem_addr to pivot) forward one element
        memmove((void *)elem_addr + 1, (void *)elem_addr,
                pivot_addr - elem_addr);
        // memcpy element into its place
        memcpy((void *)elem_addr, elem, elem_size);
    }
    else { // elem goes in right (new) half
        uintptr_t insert_addr = new_node->data + (elem_index - n_old);
        // copy (pivot + 1 to elem_addr) to new node
        memcpy((void *)new_node->data, (void *)(pivot_addr + elem_size),
                insert_addr - new_node->data);
        // memcpy element to new node
        memcpy((void *)insert_addr, elem, elem_size);
        // memcpy (elem_addr to end of data) to new node
        memcpy((void *)(insert_addr + elem_size), (void *)elem_addr,
                (node->n_keys - elem_index) * elem_size);
    }

    // copy right-half children to new node
    if (node->children) {
        memcpy(new_node->children, node->children + n_old,
                (n_new + 1) * elem_size);
    }

    // update n_keys
    node->n_keys = n_old;
    new_node->n_keys = n_new;

    // insert pivot_elem into parent
    if (node->parent) {
        set_insert_in_node(node->parent, (void *)pivot_addr, elem_size,
            new_node, less);
    }
    else { // this is the root
        set_node* new_root = calloc(1, sizeof(set_node));
        set_node_init(new_node, node->home_set, NULL, 1, false);
        memcpy((void *)new_node->data, (void *)pivot_addr, elem_size);
        new_root->children[0] = node;
        new_root->children[1] = new_node;
    }
}

void set_insert_in_node(set_node* node, void *elem, size_t elem_size,
        set_node *right_child, set_less_t less) {
    // set elem_index to point where `elem` belongs in list of keys
    size_t elem_index;
    for(elem_index = 0; elem_index < node->n_keys; ++elem_index) {
        if (less((void *)(node->data + elem_index * elem_size), elem)) {
            break;
        }
    }
    size_t max_keys = node->home_set->order - 1;
    if (node->n_keys < max_keys) {
        set_insert_in_node_simple(node, elem, elem_size, elem_index,
                right_child, less);
    }
    else {
        set_insert_in_node_complex(node, elem, elem_size, elem_index,
                right_child, less);
    }
}

void set_tree_insert(set_node *node, void *elem, size_t elem_size,
        set_less_t less) {
    // if leaf, add to node
    if (node->children == NULL) {
        set_insert_in_node(node, elem, elem_size, NULL, less);
    }
    // pass to appropriate child
    uintptr_t data = node->data;
    for (size_t key = 0; key < node->n_keys; ++key) {
        void *stored = (void *)(data + key*elem_size);
        if (less(elem, stored)) {
            // do nothing if elem eqivalent to stored key
            if (less(stored, elem)) {
                return;
            }
            set_tree_insert(node->children[key], elem, elem_size, less);
            return;
        }
    }
    // if elem is not less than or equivalent to any of the stored keys,
    // then it is greater than all of them and must be passed to the rightmost
    // child
    set_tree_insert(node->children[node->n_keys], elem, elem_size, less);
}

void set_insert(set *s, void *elem) {
    if (s->root == NULL) {
        s->root = calloc(1, sizeof(set_node));
        set_node_init(s->root, s, NULL, 1, true);
        memcpy((void *)s->root->data, elem, s->elem_size);
    }
    else {
        set_tree_insert(s->root, elem, s->elem_size, s->less);
    }
}

void set_tree_map(set_node *node, size_t elem_size,
        void (*func)(void *, void *), void *extra) {
    // apply func to data
    for (size_t key = 0; key < node->n_keys; ++key) {
        void *stored = (void *)(node->data + key*elem_size);
        func(stored, extra);
    }
    // recursively apply func to children
    if(node->children) {
        for (size_t child = 0; child < node->n_keys + 1; ++child) {
            set_tree_map(node->children[child], elem_size, func, extra);
        }
    }
}

void set_map(set *s, void (*func)(void *, void *), void *extra) {
    set_tree_map(s->root, s->elem_size, func, extra);
}
