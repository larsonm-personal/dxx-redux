#ifndef DXX_XL_MISSION_PREPARE_H
#define DXX_XL_MISSION_PREPARE_H

#include <physfs.h>
#include "mission.h"

#ifdef DXX_XL_RUNTIME
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mission_preparation mission_preparation;

/* Retain the native descriptor parse and identity allocation before publication
 * Custom D2 descriptors expose their complete parsed level/secret tables
 * Builtin and original-D1 selection retain the existing native commit paths */
mission_preparation *mission_prepare_by_name(const char *name);
mission_preparation *mission_prepare_from_current_dir(const char *descriptor);
const Mission *mission_prepared_descriptor(const mission_preparation *prepared);
const char *mission_prepared_descriptor_text(const mission_preparation *prepared);
const char *mission_prepared_archive(const mission_preparation *prepared);
const char *mission_prepared_directory(const mission_preparation *prepared);
const char *mission_prepared_level(const mission_preparation *prepared, int level);
/* Candidate HOG members are opened through an isolated mount, never live scope
 * The caller must close all returned files before commit or discard */
PHYSFS_file *mission_prepared_open(mission_preparation *prepared, const char *resource);
/* Consume and clear the handle; a busy private mount leaves it owned by caller */
int mission_commit_prepared(mission_preparation **prepared);
int mission_discard_prepared(mission_preparation **prepared);
#ifdef __cplusplus
}
#endif
#endif
#endif
