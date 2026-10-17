#ifndef DXX_REDUX_DXA_ADMISSION_LIMITS_H
#define DXX_REDUX_DXA_ADMISSION_LIMITS_H

/*
 * DXA admission contract.  Keep the matching constants in ExtractionLimits.kt
 * and DxaTextureScanner.kt synchronized with this file.
 */
#define DXX_DXA_MAX_SOURCE_BYTES         (2ull * 1024ull * 1024ull * 1024ull)
#define DXX_DXA_MAX_ENTRY_BYTES          (512ull * 1024ull * 1024ull)
#define DXX_DXA_MAX_TOTAL_EXPANDED_BYTES (2ull * 1024ull * 1024ull * 1024ull)
#define DXX_DXA_MAX_EXPANSION_RATIO      1000ull
#define DXX_DXA_MAX_ARCHIVE_ENTRIES      4096u
#define DXX_DXA_MAX_DIRECTORIES          4096u
#define DXX_DXA_MAX_TEXTURE_ENTRIES      4096u
#define DXX_DXA_MAX_PATH_DEPTH           64u
#define DXX_DXA_MAX_PATH_BYTES           4096u
#define DXX_DXA_MAX_TOTAL_PATH_BYTES     (1024u * 1024u)

#endif
