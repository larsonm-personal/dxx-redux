#ifndef DXX_REDUX_SAF_IO_CONTRACT_H
#define DXX_REDUX_SAF_IO_CONTRACT_H

#include <stdint.h>

int saf_io_resolve_seek(uint64_t offset, int64_t length, int64_t *position);

#endif
