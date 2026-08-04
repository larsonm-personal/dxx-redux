#include "saf_io_contract.h"

int saf_io_resolve_seek(uint64_t offset, int64_t length, int64_t *position)
{
	if (!position || length < 0 || offset > (uint64_t) length) return 0;
	*position = (int64_t) offset;
	return 1;
}
