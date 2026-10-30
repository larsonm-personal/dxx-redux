foreach(
    pair IN
    ITEMS "objects/powerup.cpp|5312c9cd927adbfaa71d298aadeacee8bab7d5190a27db4534d55c4dbbf4d366"
          "include/vclip.h|995af8528e7838cdc5439c632beca3b4c2a5eef3996acb6147979a89644fb1ef"
          "include/powerup.h|b786e1bc642a7ad867c52e8df06a6c49a3c89d744513e22f9da91de04c77fd6d"
          "include/piggy.h|4ecf8941d1c81166051e74c38de877f45557d163ad0c4f92d9bcdeee21033a34"
          "include/audio.h|7a0977cfefbc5de6c4805665d9b251264732179b99eb5ccd8b908b192bf3779f"
          "render/objrender.cpp|d0e46d00ddbd98a559c5aef30eacc3a001af213b5ace00f036ba760fc67d71c7"
          "include/ogl_lib.h|5be28badbe117772281c6b656aa13da9f6e28428f8b795197da7cf8cb5548c06"
          "include/laser.h|789a8116443be2068c76cecc7f962d9ef44a80a36d7590bc0e1b8a987ce12ec7")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Static explosion reference changed: ${path}")
    endif()
endforeach()
# object.h is already pinned by the owning effect reader suite
set(constants "// Constants from the pinned source headers\n")
foreach(
    pair IN
    ITEMS "object.h|OBJ_EXPLOSION"
          "object.h|OBJ_FIREBALL"
          "object.h|OBJ_POWERUP"
          "object.h|OBJ_WEAPON"
          "laser.h|OMEGA_ID"
          "object.h|OBJ_MONSTERBALL"
          "object.h|RT_POWERUP"
          "object.h|RT_POLYOBJ"
          "vclip.h|MAX_ANIMATIONS_D2"
          "vclip.h|ANIM_POWERUP_DISAPPEARANCE"
          "powerup.h|MAX_POWERUP_TYPES_D2"
          "piggy.h|MAX_ADDON_BITMAP_FILES"
          "audio.h|SOUNDCLASS_GENERIC")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 name)
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/${path}" header)
    string(REGEX MATCH "#define[ \t]+${name}[ \t]+[0-9]+" value "${header}")
    if(NOT value)
        message(FATAL_ERROR "Missing static explosion constant: ${name}")
    endif()
    string(APPEND constants "${value}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_static_constants.h"
     CONTENT "${constants}")
set(extracted
    "// Actual pinned animation, lifecycle and bitmap selection; storage/render calls are adapters\n"
)
foreach(signature IN ITEMS "void UpdatePowerupClip (" "void CObject::DoPowerupFrame ("
                           "void DrawPowerup (")
    extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/powerup.cpp" "${signature}")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_static_explosion.h"
     CONTENT "${extracted}")
target_sources(xl_effect_reference PRIVATE tests/reference_static_explosion.cpp)

file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/render/objrender.cpp" render)
string(FIND "${render}" "if ((gameOpts->render.textures.bUseHires [0]" start)
if(start LESS 0)
    message(FATAL_ERROR "Missing source sprite policy start")
endif()
string(SUBSTRING "${render}" ${start} -1 tail)
string(FIND "${tail}" "if (bmi < 0)" end)
if(end LESS 0)
    message(FATAL_ERROR "Missing source sprite policy end")
endif()
string(SUBSTRING "${tail}" 0 ${end} policy)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_static_sprite.h"
     CONTENT "// Unchanged DrawObjectBitmap blend/opacity policy\n${policy}")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/ogl_lib.h" ogl)
string(REGEX MATCHALL "#define[ \t]+OGL_BLEND_[A-Z_]+[ \t]+[-0-9]+" blend_constants "${ogl}")
string(REPLACE ";" "\n" blend_constants "${blend_constants}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_static_blend_constants.h"
     CONTENT "${blend_constants}\n")
string(FIND "${ogl}" "inline void SetBlendMode (int32_t nBlendMode)" start)
if(start LESS 0)
    message(FATAL_ERROR "Missing source sprite blend function")
endif()
string(SUBSTRING "${ogl}" ${start} -1 tail)
string(FIND "${tail}" "inline GLenum SetDepthMode" end)
if(end LESS 0)
    message(FATAL_ERROR "Missing source sprite blend boundary")
endif()
string(SUBSTRING "${tail}" 0 ${end} blend)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_static_blend.h" CONTENT "${blend}")
