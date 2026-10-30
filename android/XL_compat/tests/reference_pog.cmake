foreach(
    pair IN
    ITEMS
        "gameio/tga.cpp|9f1c15043e89acd2481eee1b0b9466e0c522c84ab1ae94a7195e1a22d99e687d"
        "gameio/addon_bitmaps.cpp|fba2bb3befbd5264788210b7c346b1203404c42a25790f0aff929bd6d67edc94"
        "effects/particle_images.cpp|1cd950eee425c26a8ac476a6a5245ce386e5b92ce00f5b52a600e83cfa9473f1"
        "include/particles.h|307e9edca466924df459b6ba5e6f0ec9d405967b38ed07021a9e561dfc29f70d"
        "gameio/loadtextures.cpp|bf1ae25e058d6ca745e5833ea0e2c0e425c5716571f6dc5f109a49d67c5452b6"
        "physics/visibility.cpp|9c85c23b1fec459abdc8115e26d031ed9223516d0b9f64f4474f3bddd03deab8"
        "main/texmerge.cpp|85acc8790e5c6ba84fb1802411408e2bd0ff592b2bf3e1fb3bffb89472ab87bc"
        "render/renderframe.cpp|56f059e9f02903cece0da2b95acbb2469bd65c9046d0a8539ee0708c632e3bbf"
        "include/bitmap.h|9044979682a8e219fa0526cc092150e562d04d2d772b23ce1e63d164d58c7024")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "POG reference changed: ${path}")
    endif()
endforeach()
set(extracted "// Actual pinned true-color conversion and pixel transparency\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/tga.cpp" "int32_t CTGAHeader::Read (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/tga.cpp" "void CTGA::ConvertToRGB (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/tga.cpp" "void CTGA::SetProperties (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/tga.cpp" "int32_t CTGA::ReadData (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/physics/visibility.cpp"
                      "int32_t PixelTranspType (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_pog.h" CONTENT "${extracted}")
set(extracted "// Actual pinned addon selection and particle image setup\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/gameio/addon_bitmaps.cpp"
                      "int32_t LoadAddonBitmap (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/effects/particle_images.cpp"
                      "int32_t CParticleImageManager::GetType (")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/effects/particle_images.cpp" particle_images)
# Load contains an unindented block in a disabled preprocessor branch Extract through its following
# separator, not the first unindented brace
string(FIND "${particle_images}" "int32_t CParticleImageManager::Load (" begin)
if(begin LESS 0)
    message(FATAL_ERROR "Missing particle image Load body")
endif()
string(SUBSTRING "${particle_images}" ${begin} -1 load_tail)
string(FIND "${load_tail}" "\n//" end)
if(end LESS 0)
    message(FATAL_ERROR "Missing particle image Load boundary")
endif()
string(SUBSTRING "${load_tail}" 0 ${end} load_body)
string(APPEND extracted "\n${load_body}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_images.h"
     CONTENT "${extracted}")
string(FIND "${particle_images}" "tParticleImageInfo particleImageInfo [" begin)
if(begin LESS 0)
    message(FATAL_ERROR "Missing active particle image table")
endif()
string(SUBSTRING "${particle_images}" ${begin} -1 table_tail)
string(FIND "${table_tail}" "\n#else" end)
if(end LESS 0)
    message(FATAL_ERROR "Missing active particle image table boundary")
endif()
string(SUBSTRING "${table_tail}" 0 ${end} table)
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/particles.h" particle_header)
set(extracted "// Actual pinned particle image constants and active quality table\n")
foreach(
    name IN
    ITEMS MAX_PARTICLE_QUALITY
          PARTICLE_TYPES
          SMOKE_PARTICLES
          BUBBLE_PARTICLES
          RAIN_PARTICLES
          SNOW_PARTICLES
          WATERFALL_PARTICLES
          FIRE_PARTICLES)
    string(REGEX MATCH "#define[ \t]+${name}[ \t]+[^\n]+" constant "${particle_header}")
    if(NOT constant)
        message(FATAL_ERROR "Missing particle image constant: ${name}")
    endif()
    string(APPEND extracted "${constant}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_image_table.h"
     CONTENT "${extracted}\n${table}\n")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/gameio/loadtextures.cpp" loader)
string(FIND "${loader}" "bm.AddFlags (bmh [i].flags & (BM_FLAGS_TO_COPY | BM_FLAG_TGA));" begin)
if(begin LESS 0)
    message(FATAL_ERROR "Missing original POG dimension setup")
endif()
string(SUBSTRING "${loader}" ${begin} -1 tail)
string(FIND "${tail}" "bm.SetBPP (bHaveTGA ? 4 : 1);" end)
string(LENGTH "bm.SetBPP (bHaveTGA ? 4 : 1);" suffix)
math(EXPR length "${end} + ${suffix}")
string(SUBSTRING "${tail}" 0 ${length} dimensions)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_pog_dimensions.h"
     CONTENT "${dimensions}\n")
add_executable(xl_pog_reference tests/reference_pog.cpp)
target_link_libraries(xl_pog_reference PRIVATE xl_inventory)
target_include_directories(xl_pog_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME xl_pog_reference COMMAND xl_pog_reference)
if(TARGET xl_pog_gpu_test)
    foreach(
        pair IN
        ITEMS
            "include/ogl_lib.h|5be28badbe117772281c6b656aa13da9f6e28428f8b795197da7cf8cb5548c06"
            "effects/particle_buffer.cpp|b4d4cf4468e79300d58aeb28071f1964ff8e7c48c1e517cec247f3d4ab3878e7"
    )
        string(REPLACE "|" ";" parts "${pair}")
        list(GET parts 0 path)
        list(GET parts 1 expected)
        file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
        if(NOT actual STREQUAL expected)
            message(FATAL_ERROR "Particle blend source changed: ${path}")
        endif()
    endforeach()
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/ogl_lib.h" ogl_header)
    set(extracted "// Actual particle blend selection and OpenGL factor dispatch\n")
    string(REGEX MATCH "#define[ \t]+PARTICLE_TYPES[ \t]+[^\n]+" constant "${particle_header}")
    string(APPEND extracted "${constant}\n")
    foreach(name IN ITEMS OGL_BLEND_ALPHA OGL_BLEND_ADD OGL_BLEND_ADD_WEAK OGL_BLEND_REPLACE
                          OGL_BLEND_MULTIPLY OGL_BLEND_ALPHA_CONTROLLED)
        string(REGEX MATCH "#define[ \t]+${name}[ \t]+[^\n]+" constant "${ogl_header}")
        if(NOT constant)
            message(FATAL_ERROR "Missing source blend constant: ${name}")
        endif()
        string(APPEND extracted "${constant}\n")
    endforeach()
    string(FIND "${ogl_header}" "inline void SetBlendMode (int32_t" begin)
    string(FIND "${ogl_header}" "inline GLenum SetDepthMode (" end)
    if(begin LESS 0 OR end LESS begin)
        message(FATAL_ERROR "Missing source GL blend dispatch")
    endif()
    math(EXPR length "${end} - ${begin}")
    string(SUBSTRING "${ogl_header}" ${begin} ${length} blend_body)
    string(
        APPEND
        extracted
        "struct ParticleBlendSource {\nstd::array<GLenum, 2> factors{};\nvoid SetBlendMode(GLenum a, GLenum b) { factors = {a,b}; }\n${blend_body}\n};\n"
    )
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/effects/particle_buffer.cpp" buffer_source)
    string(REGEX MATCH "ogl.SetBlendMode \\(\\(m_nType < PARTICLE_TYPES\\)[^;]+;" blend_call
                 "${buffer_source}")
    if(NOT blend_call)
        message(FATAL_ERROR "Missing source particle blend call")
    endif()
    string(
        APPEND
        extracted
        "static std::array<GLenum, 2> source_particle_blend(int m_nType, int m_bEmissive) { ParticleBlendSource ogl; ${blend_call} return ogl.factors; }\n"
    )
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_particle_blend.h"
         CONTENT "${extracted}")
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/main/texmerge.cpp" merge)
    string(FIND "${merge}" "const char *texMergeFS [6] = {" begin)
    string(FIND "${merge}" "const char *texMergeFSData =" end)
    if(begin LESS 0 OR end LESS begin)
        message(FATAL_ERROR "Missing actual XL texture-merge shader arrays")
    endif()
    math(EXPR length "${end} - ${begin}")
    string(SUBSTRING "${merge}" ${begin} ${length} source_shaders)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_pog_shaders.h"
         CONTENT "${source_shaders}")
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/render/renderframe.cpp" frame)
    string(FIND "${frame}" "glAlphaFunc (GL_GEQUAL, (float) 0.005);" alpha)
    if(alpha LESS 0)
        message(FATAL_ERROR "Source frame alpha threshold changed")
    endif()
    target_include_directories(xl_pog_gpu_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_definitions(xl_pog_gpu_test PRIVATE XL_POG_SHADER_REFERENCE)
endif()
