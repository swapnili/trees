#ifndef _RADIX_TREE_H
#define _RADIX_TREE_H

#include <stdbool.h>

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

#define BITS_PER_LONG	64

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define RADIX_TREE_INDIRECT_PTR     1

static inline int radix_tree_is_indirect_ptr(void *ptr)
{
	return (int)((unsigned long)ptr & RADIX_TREE_INDIRECT_PTR);
}

#define RADIX_TREE_MAP_SHIFT	6
#define RADIX_TREE_MAP_SIZE	(1UL << RADIX_TREE_MAP_SHIFT)
#define RADIX_TREE_MAP_MASK	(RADIX_TREE_MAP_SIZE-1)

#define RADIX_TREE_INDEX_BITS	(8 /* CHAR_BIT */ * sizeof(unsigned long))
#define RADIX_TREE_MAX_PATH	(DIV_ROUND_UP(RADIX_TREE_INDEX_BITS, \
					  RADIX_TREE_MAP_SHIFT))

/* Height component in node->path */
#define RADIX_TREE_HEIGHT_SHIFT (RADIX_TREE_MAX_PATH + 1)
#define RADIX_TREE_HEIGHT_MASK  ((1UL << RADIX_TREE_HEIGHT_SHIFT) - 1)

/* Internally used bits of node->count */
#define RADIX_TREE_COUNT_SHIFT  (RADIX_TREE_MAP_SHIFT + 1)
#define RADIX_TREE_COUNT_MASK   ((1UL << RADIX_TREE_COUNT_SHIFT) - 1)

static inline void *ptr_to_indirect(void *ptr)
{
	return (void *)((unsigned long)ptr | RADIX_TREE_INDIRECT_PTR);
}

static inline void *indirect_to_ptr(void *ptr)
{
	return (void *)((unsigned long)ptr & ~RADIX_TREE_INDIRECT_PTR);
}

struct radix_tree_root {
	unsigned int	height;
	struct radix_tree_node	*rnode;
};

struct radix_tree_node {
	unsigned int    path; /* Offset in parent & height from the bottom */
	unsigned int    count;
	struct radix_tree_node *parent;
	void *private_data;
	void *slots[RADIX_TREE_MAP_SIZE];
};

#define RADIX_TREE(name)	\
	struct radix_tree_root name = { .rnode = NULL, .height = 0}

#define INIT_RADIX_TREE(root)		\
	do {				\
		(root)->rnode = NULL;	\
		(root)->height = 0;	\
	} while (0)

int __radix_tree_create(struct radix_tree_root *root, unsigned long index,
			struct radix_tree_node **nodep,	void ***slotp);

int radix_tree_insert(struct radix_tree_root *, unsigned long, void *);

void *__radix_tree_lookup(struct radix_tree_root *root, unsigned long index,
			  struct radix_tree_node **nodep, void ***slotp);
void *radix_tree_lookup(struct radix_tree_root *root, unsigned long index);
void **radix_tree_lookup_slot(struct radix_tree_root *root, unsigned long
			      index);

bool __radix_tree_delete_node(struct radix_tree_root *root, struct
			      radix_tree_node *node);
void *radix_tree_delete_item(struct radix_tree_root *, unsigned long, void *);
void *radix_tree_delete(struct radix_tree_root *, unsigned long);

void radix_tree_init(void);

#endif
