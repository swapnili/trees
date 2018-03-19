/*
 * Copyright (C) 2001 Momchil Velikov
 * Portions Copyright (C) 2001 Christoph Hellwig
 * Copyright (C) 2006 Nick Piggin
 * Copyright (C) 2012 Konstantin Khlebnikov
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* userspace radix-tree implementation  ported from linux kernel */

#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "radix-tree.h"

static unsigned long height_to_maxindex[RADIX_TREE_MAX_PATH + 1];

static unsigned long __maxindex(unsigned int height)
{
	unsigned int width = height * RADIX_TREE_MAP_SHIFT;
	int shift = RADIX_TREE_INDEX_BITS - width;

	if (shift < 0)
		return ~0UL;
	if (shift >= BITS_PER_LONG)
		return 0UL;
	return ~0UL >> shift;
}

static void radix_tree_init_maxindex(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(height_to_maxindex); i++)
		height_to_maxindex[i] = __maxindex(i);
}

static inline unsigned long radix_tree_maxindex(unsigned int height)
{
	return height_to_maxindex[height];
}

static struct radix_tree_node *
radix_tree_node_alloc(struct radix_tree_root *root)
{
	struct radix_tree_node *ret = NULL;

	ret = malloc(sizeof(struct radix_tree_node));
	return ret;
}

static int radix_tree_extend(struct radix_tree_root *root, unsigned long index)
{
	struct radix_tree_node *node;
	struct radix_tree_node *slot;
	unsigned int height;

	/* Figure out what the height should be.  */
	height = root->height + 1;
	while (index > radix_tree_maxindex(height))
		height++;

	if (root->rnode == NULL) {
		root->height = height;
		goto out;
	}

	do {
		unsigned int newheight;
		if (!(node = radix_tree_node_alloc(root)))
			return -ENOMEM;

		/* Increase the height.  */
		newheight = root->height+1;
		node->path = newheight;
		node->count = 1;
		node->parent = NULL;
		slot = root->rnode;
		if (newheight > 1) {
			slot = indirect_to_ptr(slot);
			slot->parent = node;
		}
		node->slots[0] = slot;
	        node = ptr_to_indirect(node);
		root->rnode = node;
		root->height = newheight;
	} while (height > root->height);

out:
	return 0;
}

int __radix_tree_create(struct radix_tree_root *root, unsigned long index,
			struct radix_tree_node **nodep,	void ***slotp)
{
	struct radix_tree_node *node = NULL, *slot;
	unsigned int height, shift, offset;
	int error;

	/* Make sure the tree is high enough.  */
	if (index > radix_tree_maxindex(root->height)) {
		error = radix_tree_extend(root, index);
		if (error)
			return error;
	}

	slot = indirect_to_ptr(root->rnode);

	height = root->height;
	shift = (height-1) * RADIX_TREE_MAP_SHIFT;

	offset = 0;
	while (height > 0) {
		if (slot == NULL) {
			/* Have to add a child node.  */
			if (!(slot = radix_tree_node_alloc(root)))
				return -ENOMEM;
			slot->path = height;
			slot->parent = node;
			if (node) {
				node->slots[offset] = slot;
				node->count++;
				slot->path |= offset << RADIX_TREE_HEIGHT_SHIFT;
			} else
				root->rnode = ptr_to_indirect(slot);
		}

		/* Go a level down */
		offset = (index >> shift) & RADIX_TREE_MAP_MASK;
		node = slot;
		slot = node->slots[offset];
		shift -= RADIX_TREE_MAP_SHIFT;
		height--;
	}

	if (nodep)
		*nodep = node;
	if (slotp)
		*slotp = node ? node->slots + offset : (void **)&root->rnode;
	return 0;
}

int radix_tree_insert(struct radix_tree_root *root, unsigned long index, void
		      *item)
{
	struct radix_tree_node *node;
        void **slot;
	int error;

	error = __radix_tree_create(root, index, &node,	&slot);
	if (error)
		return error;
	if (*slot != NULL)
		return -EEXIST;
	*slot = item;

	if (node)
		node->count++;

	return 0;
}

void *__radix_tree_lookup(struct radix_tree_root *root, unsigned long index,
			  struct radix_tree_node **nodep, void ***slotp)
{
	struct radix_tree_node *node, *parent;
	unsigned int height, shift;
	void **slot;

	node = root->rnode;
	if (node == NULL)
		return NULL;

	if (!radix_tree_is_indirect_ptr(node)) {
		if (index > 0)
			return NULL;
		if (nodep)
			*nodep = NULL;
		if (slotp)
			*slotp = (void **)&root->rnode;
		return node;
	}
	node = indirect_to_ptr(node);

	height = node->path & RADIX_TREE_HEIGHT_MASK;
	if (index > radix_tree_maxindex(height))
		return NULL;

	shift = (height-1) * RADIX_TREE_MAP_SHIFT;

	do {
		parent = node;
		slot = node->slots + ((index >> shift) & RADIX_TREE_MAP_MASK);
		node = *slot;
		if (node == NULL)
			return NULL;

		shift -= RADIX_TREE_MAP_SHIFT;
		height--;
	} while (height > 0);

	if (nodep)
		*nodep = parent;
	if (slotp)
		*slotp = slot;
	return node;
}

void *radix_tree_lookup(struct radix_tree_root *root, unsigned long index)
{
	return __radix_tree_lookup(root, index, NULL, NULL);
}

void **radix_tree_lookup_slot(struct radix_tree_root *root, unsigned long index)
{
	void **slot;

	if (!__radix_tree_lookup(root, index, NULL, &slot))
		return NULL;
	return slot;
}

void **radix_tree_next_chunk(struct radix_tree_root *root, struct
			     radix_tree_iter *iter)
{
	unsigned shift;
	struct radix_tree_node *rnode, *node;
	unsigned long index, offset, height;

	index = iter->next_index;
	if (!index && iter->index)
		return NULL;

	rnode = root->rnode;
	if (radix_tree_is_indirect_ptr(rnode)) {
		rnode = indirect_to_ptr(rnode);
	} else if (rnode && !index) {
		/* Single-slot tree */
		iter->index = 0;
		iter->next_index = 1;
		return (void **)&root->rnode;
	} else
		return NULL;

restart:
	height = rnode->path & RADIX_TREE_HEIGHT_MASK;
	shift = (height - 1) * RADIX_TREE_MAP_SHIFT;
	offset = index >> shift;

	/* Index outside of the tree */
	if (offset >= RADIX_TREE_MAP_SIZE)
		return NULL;

	node = rnode;
	while (1) {
		if (!node->slots[offset]) {
			/* Hole detected */
			while (++offset < RADIX_TREE_MAP_SIZE) {
				if (node->slots[offset])
					break;
			}
			index &= ~((RADIX_TREE_MAP_SIZE << shift) - 1);
			index += offset << shift;
			/* Overflow after ~0UL */
			if (!index)
				return NULL;
			if (offset == RADIX_TREE_MAP_SIZE)
				goto restart;
		}

		/* This is leaf-node */
		if (!shift)
			break;

		node = node->slots[offset];
		if (node == NULL)
			goto restart;
		shift -= RADIX_TREE_MAP_SHIFT;
		offset = (index >> shift) & RADIX_TREE_MAP_MASK;
	}

	/* Update the iterator state */
	iter->index = index;
	iter->next_index = (index | RADIX_TREE_MAP_MASK) + 1;

	return node->slots + offset;
}

static void radix_tree_node_free(struct radix_tree_node *node)
{
	node->slots[0] = NULL;
	node->count = 0;

	free(node);
}

static inline void radix_tree_shrink(struct radix_tree_root *root)
{
	/* try to shrink tree height */
	while (root->height > 0) {
		struct radix_tree_node *to_free = root->rnode;
		struct radix_tree_node *slot;

		to_free = indirect_to_ptr(to_free);

		/*
		 * The candidate node has more than one child, or its child
		 * is not at the leftmost slot, we cannot shrink.
		 */
		if (to_free->count != 1)
			break;
		if (!to_free->slots[0])
			break;

		slot = to_free->slots[0];
		if (root->height > 1) {
			slot->parent = NULL;
			slot = ptr_to_indirect(slot);
		}
		root->rnode = slot;
		root->height--;

		if (root->height == 0)
			*((unsigned long *)&to_free->slots[0]) |=
				RADIX_TREE_INDIRECT_PTR;

		radix_tree_node_free(to_free);
	}
}

bool __radix_tree_delete_node(struct radix_tree_root *root, struct
			      radix_tree_node *node)
{
	bool deleted = false;

	do {
		struct radix_tree_node *parent;

		if (node->count) {
			if (node == indirect_to_ptr(root->rnode)) {
				radix_tree_shrink(root);
				if (root->height == 0)
					deleted = true;
			}
			return deleted;
		}

		parent = node->parent;
		if (parent) {
			unsigned int offset;

			offset = node->path >> RADIX_TREE_HEIGHT_SHIFT;
			parent->slots[offset] = NULL;
			parent->count--;
		} else {
			root->height = 0;
			root->rnode = NULL;
		}

		radix_tree_node_free(node);
		deleted = true;

		node = parent;
	} while (node);

	return deleted;
}

void *radix_tree_delete_item(struct radix_tree_root *root,
			     unsigned long index, void *item)
{
	struct radix_tree_node *node;
	unsigned int offset;
	void **slot;
	void *entry;
	int tag;

	entry = __radix_tree_lookup(root, index, &node, &slot);
	if (!entry)
		return NULL;

	if (item && entry != item)
		return NULL;

	if (!node) {
		root->rnode = NULL;
		return entry;
	}

	offset = index & RADIX_TREE_MAP_MASK;
	node->slots[offset] = NULL;
	node->count--;

	__radix_tree_delete_node(root, node);

	return entry;
}

void *radix_tree_delete(struct radix_tree_root *root, unsigned long index)
{
	return radix_tree_delete_item(root, index, NULL);
}

void radix_tree_init(void)
{
	radix_tree_init_maxindex();
}

