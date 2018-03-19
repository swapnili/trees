#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include "radix-tree.h"

struct data {
	int val;
};

static int delete_radix_tree(struct radix_tree_root *myradix)
{
	struct radix_tree_iter iter;
	struct data *d;
	unsigned long indices[16];
	unsigned long index = 0;
	void **slot;
	int i, nr;

	do {
		nr = 0;
		radix_tree_for_each_slot(slot, myradix, &iter, index) {
			indices[nr] = iter.index;
			if (++nr == 16)
				break;
		}
		for (i = 0; i < nr; i++) {
			index = indices[i];
			d = radix_tree_delete(myradix, index);
			if (!d)
				return -EFAULT;
			free(d);
		}
	} while (nr > 0);

	return 0;
}

int main()
{
	struct data *d;
	int offset = 131072, err = -EFAULT, n = 100;
	unsigned long i;
	void **slot, *val;

	RADIX_TREE(myradix);
	radix_tree_init();

	/* insert */
	for (i=offset; i<=offset*n; i+=offset) {
		d = malloc(sizeof(*d));
		if (!d)
			return -ENOMEM;

		d->val = i;
		err = radix_tree_insert(&myradix, i, d);
		if (err) {
			printf("failed to insert %d\n", i);
			goto out;
		}
	}

	/* lookup */
	for (i=offset; i<=offset*n; i+=offset) {
		d = radix_tree_lookup(&myradix, i);
		if (!d) {
			printf("lookup of %lu failed\n", i);
			goto out;
		}
		if (i != d->val)
			printf("lookup: got wrong val: %lu != %d\n", i,
			       d->val);
	}

#if 0
	/* delete by index */
	for (i=offset; i<=offset*n; i+=offset) {
		void *val;

		val = radix_tree_delete(&myradix, i);
		if (val == NULL)
			printf("failed to delete index %lu\n", i);
		if (i != *(int *)val)
			printf("delete: got wrong val: %lu != %d\n", i,
			       *(int *)val);
	}
#endif

	/* delete whole radix tree */
	err = delete_radix_tree(&myradix);

out:
	return err;
}
