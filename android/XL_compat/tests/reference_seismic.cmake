foreach(
    pair IN
    ITEMS "weapons/seismic.cpp|2c0e2c45cfef16b7713d63ff4a92368720733405613af929867ec433dd49bacc"
          "main/trigger.cpp|74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852"
          "include/maths.h|43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Seismic source differs: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/weapons/seismic.cpp" source)
set(extracted "// Actual pinned seismic functions\n")
foreach(name IN ITEMS ESHAKER_SHAKE_TIME MAX_ESHAKER_DETONATES)
    string(REGEX MATCH "#define[ \t]+${name}[^\n]+" definition "${source}")
    if(NOT definition)
        message(FATAL_ERROR "Missing seismic constant: ${name}")
    endif()
    string(APPEND extracted "${definition}\n")
endforeach()
string(
    APPEND
    extracted
    "fix eshakerDetonateTimes[MAX_ESHAKER_DETONATES]{};\nfloat eshakerDetonateScales[MAX_ESHAKER_DETONATES]{};\n"
)
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h" math_header)
string(REGEX MATCH "inline float FRound[^\n]+" rounding "${math_header}")
string(APPEND extracted "${rounding}\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/include/maths.h"
                      "static inline int32_t SRandShort (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "int32_t CTrigger::DoShakeMine (")
foreach(
    signature IN
    ITEMS "void InitShakerDetonates ("
          "static void RockPlayerShip ("
          "void RockTheMineFrame ("
          "void InitSeismicDisturbances ("
          "int32_t StartSeismicDisturbance ("
          "void SeismicDisturbanceFrame ("
          "void ShakerRockStuff ("
          "void DoSeismicStuff (")
    string(FIND "${source}" "${signature}" start)
    if(start LESS 0)
        message(FATAL_ERROR "Missing seismic function: ${signature}")
    endif()
    string(SUBSTRING "${source}" ${start} -1 tail)
    # RockPlayerShip's closing brace has trailing whitespace in the pinned file
    string(REGEX MATCH "\n}[ \t]*\n" ending "${tail}")
    if(NOT ending)
        message(FATAL_ERROR "Missing seismic function boundary: ${signature}")
    endif()
    string(FIND "${tail}" "${ending}" end)
    string(LENGTH "${ending}" ending_length)
    math(EXPR length "${end} + ${ending_length}")
    string(SUBSTRING "${tail}" 0 ${length} function_body)
    string(APPEND extracted "${function_body}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_seismic.h" CONTENT "${extracted}")
target_sources(xl_named_audio_source PRIVATE tests/reference_seismic.cpp)
