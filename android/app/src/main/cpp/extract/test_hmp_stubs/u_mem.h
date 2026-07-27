#ifndef TEST_U_MEM_H
#define TEST_U_MEM_H

#include <stddef.h>

void *test_hmp_calloc(size_t count, size_t size);
void *test_hmp_malloc(size_t size);
void *test_hmp_realloc(void *ptr, size_t size);
void test_hmp_free(void *ptr);

#define d_calloc(count, size) test_hmp_calloc((count), (size))
#define d_malloc(size)        test_hmp_malloc(size)
#define d_realloc(ptr, size)  test_hmp_realloc((ptr), (size))
#define d_free(ptr)           test_hmp_free(ptr)

#endif
