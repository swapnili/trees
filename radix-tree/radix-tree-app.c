#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include "radix-tree.h"

int main()
{
	int offset = 131072, err = -EFAULT, n = 10000;
	unsigned long i;

	RADIX_TREE(myradix);
	radix_tree_init();

	/* insert */
	for (i=offset; i<=offset*n; i+=offset) {
		err = radix_tree_insert(&myradix, i, (void *)&i);
		if (err) {
			printf("failed to insert %d\n", i);
			goto out;
		}
	}

	/* lookup */
	for (i=offset; i<=offset*n; i+=offset) {
		void *val;

		val = radix_tree_lookup(&myradix, i);
		if (val == NULL) {
			printf("lookup of %lu failed\n", i);
			goto out;
		}
		if (i != *(int *)val)
			printf("lookup: got wrong val: %lu != %d\n", i,
			       *(int *)val);
	}

	/* delete */
	for (i=offset; i<=offset*n; i+=offset) {
		void *val;

		val = radix_tree_delete(&myradix, i);
		if (val == NULL)
			printf("failed to delete index %lu\n", i);
		if (i != *(int *)val)
			printf("delete: got wrong val: %lu != %d\n", i,
			       *(int *)val);
	}
out:
	return err;
}
