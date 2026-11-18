#ifndef DXX_XL_HOG_SELECTION_H
#define DXX_XL_HOG_SELECTION_H
#include "hog_format.h"

struct dxx_hog_lookup {
	const char *name;
	size_t ordinal;
};

// Match CHogFile::QuickSort, including exchanges between equal names. Recurse
// on the smaller partition so an unbalanced input cannot exhaust the stack.
static inline void dxx_hog_sort(struct dxx_hog_lookup *order, ptrdiff_t left, ptrdiff_t right)
{
	while (left < right) {
		ptrdiff_t low = left, high = right;
		const char *pivot = order[left + (right - left) / 2].name;
		do {
			while (dxx_hog_name_compare(order[low].name, pivot) < 0) ++low;
			while (dxx_hog_name_compare(order[high].name, pivot) > 0) --high;
			if (low <= high) {
				struct dxx_hog_lookup saved = order[low];
				order[low++] = order[high];
				order[high--] = saved;
			}
		} while (low <= high);
		if (high - left < right - low) {
			dxx_hog_sort(order, left, high);
			left = low;
		} else {
			dxx_hog_sort(order, low, right);
			right = high;
		}
	}
}

// The caller supplies all physical names in file order and count output bytes.
// CHogFile::BinSearch probes the midpoint of the full inclusive search range.
static inline void dxx_hog_select(struct dxx_hog_lookup *order, size_t count, unsigned char *selected)
{
	size_t i = 0;
	if (!count) return;
	memset(selected, 0, count);
	dxx_hog_sort(order, 0, (ptrdiff_t) count - 1);
	while (i < count) {
		size_t end = i + 1, winner = i;
		while (end < count && !dxx_hog_name_compare(order[i].name, order[end].name)) ++end;
		if (end - i > 1) {
			size_t left = 0, right = count - 1;
			for (;;) {
				winner = left + (right - left) / 2;
				if (winner < i) left = winner + 1;
				else if (winner >= end) right = winner - 1;
				else break;
			}
		}
		selected[order[winner].ordinal] = 1;
		i = end;
	}
}
#endif
