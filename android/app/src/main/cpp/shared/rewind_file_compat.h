#ifndef REWIND_FILE_COMPAT_H
#define REWIND_FILE_COMPAT_H

#include "rewind_file.h"

#if REWIND_FILE_USES_WRAPPER
#define PHYSFS_file                 rewind_file
#define PHYSFS_read                 rewind_file_read
#define PHYSFS_write                rewind_file_write
#define PHYSFS_seek                 rewind_file_seek
#define PHYSFS_tell                 rewind_file_tell
#define PHYSFS_fileLength           rewind_file_length
#define PHYSFS_eof                  rewind_file_eof
#define PHYSFS_writeSLE16           rewind_file_write_sle16
#define PHYSFS_writeSLE32           rewind_file_write_sle32
#define PHYSFSX_readInt             rewind_file_read_int
#define PHYSFSX_readShort           rewind_file_read_short
#define PHYSFSX_readByte            rewind_file_read_byte
#define PHYSFSX_readFix             rewind_file_read_fix
#define PHYSFSX_readFixAng          rewind_file_read_fixang
#define PHYSFSX_readSXE16           rewind_file_read_sxe16
#define PHYSFSX_readSXE32           rewind_file_read_sxe32
#define PHYSFSX_readVector(v, fp)   rewind_file_read_vector((fp), (v))
#define PHYSFSX_readVectorX         rewind_file_read_vector_x
#define PHYSFSX_readAngleVec(v, fp) rewind_file_read_anglevec((fp), (v))
#define PHYSFSX_readAngleVecX       rewind_file_read_anglevec_x
#define PHYSFSX_writeFix            rewind_file_write_fix
#define PHYSFSX_writeU8             rewind_file_write_u8
#define PHYSFSX_writeVector         rewind_file_write_vector
#endif

#endif /* REWIND_FILE_COMPAT_H */