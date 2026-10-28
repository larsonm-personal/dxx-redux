#ifndef DXX_OBJECT_CAPACITY_H
#define DXX_OBJECT_CAPACITY_H

/* Macros are expanded after the including unit has read object.h */
#ifdef DXX_BUILD_DESCENT_II
#define NETWORK_OBJECT_CAPACITY DXX_CLASSIC_MAX_OBJECTS
#else
#define NETWORK_OBJECT_CAPACITY MAX_OBJECTS
#endif
#ifdef DXX_XL_RUNTIME
#ifdef __cplusplus
extern "C" {
#endif
int object_capacity(void);
#ifdef __cplusplus
}
#endif
#else
#define object_capacity() MAX_OBJECTS
#endif

#endif
