# audio/audio.cpp is already hash-pinned by the enclosing named-audio owner
set(extracted "// Actual pinned stock-sample converter, using independent buffer storage\n")
foreach(
    signature IN
    ITEMS "static int32_t ResampleFormat (" "static int32_t ResampleRate (uint16_t*"
          "static int32_t ResampleRate (uint8_t*" "template<typename _T>"
          "int32_t CAudioChannel::Resample (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/audio/audio.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_classic_sample.h"
     CONTENT "${extracted}")
target_sources(xl_named_audio_source PRIVATE tests/reference_classic_sample.cpp)
