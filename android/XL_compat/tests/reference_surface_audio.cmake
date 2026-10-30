# objectaudio.cpp is hash-pinned by reference_audio_ownership.cmake
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/include/wall.h" wall_hash)
if(NOT wall_hash STREQUAL "08c59bf8c299ea05967f806b92b3d03b4226914ef747ae1ef5afa4154c70c2bd")
    message(FATAL_ERROR "Surface audio doorway constants changed")
endif()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/wall.h" wall)
string(REGEX MATCHALL "#define[ \t]+WID_(PASSABLE|VISIBLE|TRANSPARENT)_FLAG[ \t]+[0-9]+" flags
             "${wall}")
list(LENGTH flags count)
if(NOT count EQUAL 3)
    message(FATAL_ERROR "Missing surface audio doorway flags")
endif()
string(REPLACE ";" "\n" flags "${flags}")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/sounds.h" sounds)
string(REGEX MATCH "#define[ \t]+SOUND_FORCEFIELD_HUM[ \t]+[0-9]+" forcefield "${sounds}")
if(NOT forcefield)
    message(FATAL_ERROR "Missing source forcefield sound")
endif()
set(extracted
    "// Actual pinned surface selection and initializer; storage/geometry/audio calls are adapters\n${flags}\n${forcefield}\n"
)
extract_audio_owner("static int32_t SideIsSoundSource (")
extract_audio_owner("void SetSoundSources (")
file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/objects/object.cpp" object_hash)
if(NOT object_hash STREQUAL "e71b0a906cd686549c783563ab93365d1bbf5cf05f08c1971ef08c7116c67a64")
    message(FATAL_ERROR "Ambient object list insertion source changed")
endif()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/object.cpp"
                      "void CObject::Link (tObjListRef& ref, int32_t nLink)")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/audio/objectaudio.cpp" source)
string(REGEX MATCH "if \\(nOrgSound == SOUND_FORCEFIELD_HUM\\)[ \t\r\n]+maxVolume = [^;]+;" scaling
             "${source}")
if(NOT scaling)
    message(FATAL_ERROR "Missing source forcefield surface scaling")
endif()
string(APPEND extracted
       "\nint SurfaceScaledVolume(int nOrgSound, int maxVolume) { ${scaling} return maxVolume; }\n")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_surface_audio.h"
     CONTENT "${extracted}")
target_sources(xl_named_audio_source PRIVATE tests/reference_surface_audio.cpp)
