#ifndef DXX_XL_MISSION_RESOURCE_SCOPE_H
#define DXX_XL_MISSION_RESOURCE_SCOPE_H

#include <physfs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Zero-initialized lease owned by the retained native mission descriptor */
typedef struct xl_mission_resource_scope {
	char private_mount[64];
	int owns_mount;
} xl_mission_resource_scope;

/* A NULL archive selects loose resources from the descriptor's exact directory */
PHYSFS_file *xl_mission_resource_open(xl_mission_resource_scope *scope,
                                      const char *archive, const char *directory, const char *resource);
/* Busy private members retain the lease; borrowed live archives are never unmounted */
int xl_mission_resource_release(xl_mission_resource_scope *scope, const char *archive);

#ifdef __cplusplus
}
#endif
#endif
