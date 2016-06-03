#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct set set;
typedef bool (*set_less_t)(void *, void *);

/* Initialize set `s`, containing items of size `elem_size`, and implemented as
 * a B-Tree of Knuth order `order`. `order` shall be 2 or greater.
 * Uses `less` as internal weak-ordered comparison operator.
 * less(x, y) should return true if x < y
 */
void set_init(set *s, uint8_t order, set_less_t less,
        size_t elem_size);

/* Free the contents of the set `s`. If the set was dynamically allocated, the
 * set itself must still be free'd.
 */
void set_free(set *s);

/* See if `s` contains an element equivalent to `elem`. If so, `set_contains`
 * returns true; if not it returns false. If `copy_out` is not NULL, and an
 * element equivalent to `elem` is found, the contained element is copied to
 * the variable pointed at by `copy_out`.
 */
bool set_contains(set *s, void *elem, void *copy_out);

/* Insert `elem` into set `s`.
 */
void set_insert(set *s, void *elem);

/* Apply function `func` to every element in `s`. `func`'s first argument must
   be the item stored in a set. `func` must not modify items in the set in a
   way which alters their relative ordering. `extra` should contain any
   extra information that `func` needs, and is passed as the second argument to
   `func`.
 */
void set_map(set *s, void (*func)(void *, void *), void *extra);
