foreach(
    pair IN
    ITEMS "ogl/fbuffer.cpp|e87f0525548fa11a39ecb6ef3780437166fcaa63c373bc866dc65cf7f9f3d154"
          "ogl/rendertextures.cpp|a78a260d492457c8bde0ab776936ac803410c71af0ca3d99820343158f19685f"
          "include/fbuffer.h|867d8ab76d5f740294b1116381b8bd140f1486de665d00cad8a4de232676af43")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Camera framebuffer source changed: ${path}")
    endif()
endforeach()
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/fbuffer.h" fbo_header)
string(REGEX MATCH "typedef struct tFrameBuffer \\{[^}]+\\} tFrameBuffer;" layout "${fbo_header}")
if(NOT layout)
    message(FATAL_ERROR "Missing source camera framebuffer layout")
endif()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_buffer_layout.h"
     CONTENT "#define MAX_COLOR_BUFFERS 16\n${layout}\n")
set(extracted "// Actual source camera color/depth allocation, attachment and selection\n")
foreach(signature IN ITEMS "int32_t CFBO::CreateColorBuffers (" "int32_t CFBO::CreateDepthBuffer ("
                           "void CFBO::AttachBuffers (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ogl/fbuffer.cpp" "${signature}")
endforeach()
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/ogl/rendertextures.cpp"
                      "GLuint COGL::CreateDepthTexture (")
string(REGEX MATCH "inline void SelectColorBuffers \\([^}]+}" selection "${fbo_header}")
if(NOT selection)
    message(FATAL_ERROR "Missing source camera buffer selection")
endif()
string(REPLACE "inline void SelectColorBuffers (" "void CFBO::SelectColorBuffers (" selection
               "${selection}")
string(APPEND extracted "\n${selection}\n")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_camera_buffers.h"
     CONTENT "${extracted}")
