foreach(
    pair IN
    ITEMS
        "weapons/createweapon.cpp|ba5b11cbed8606ef260cb100af15cd8e39c4a16ef79866ac5113135d3559ccdd"
        "effects/fireball.cpp|e91c43142cbb1f85c0274aac19e97c95d62e95bc5aa31c1d2537db2113387bf4"
        "objects/object.cpp|e71b0a906cd686549c783563ab93365d1bbf5cf05f08c1971ef08c7116c67a64"
        "include/laser.h|789a8116443be2068c76cecc7f962d9ef44a80a36d7590bc0e1b8a987ce12ec7"
        "include/segment.h|4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667"
        "physics/collide.cpp|d10f88f82e5d283077914881d1acb719f06785a86be22e50617d92d6fce47bc9")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Combat audio source differs: ${path}")
    endif()
endforeach()
set(constants "")
foreach(pair IN ITEMS "laser.h|[A-Z_0-9]+_ID" "segment.h|OBJ_[A-Z_]+" "sounds.h|SOUND_[A-Z_]+"
                      "piggy.h|SND_ADDON_[A-Z_]+")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 pattern)
    file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/include/${path}" header)
    string(REGEX MATCHALL "#define[ \t]+${pattern}[ \t]+[0-9]+[^\n]*" definitions "${header}")
    string(REPLACE ";" "\n" definitions "${definitions}")
    string(APPEND constants "${definitions}\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_combat_constants.h"
     CONTENT "${constants}")
set(extracted "// Actual pinned weapon firing function; options/storage/audio calls are adapters\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/weapons/createweapon.cpp"
                      "void CreateWeaponSound (")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/objects/object.cpp" objects)
string(REGEX MATCHALL "m_weaponInfo \\[[^\n]+\\|= [^\n]+OBJ_IS_SPLASHDMG_WEAPON[^;]*" assignments
             "${objects}")
list(LENGTH assignments count)
if(NOT count EQUAL 8)
    message(FATAL_ERROR "Splash weapon classification changed")
endif()
string(REPLACE ";" ";\n" assignments "${assignments}")
string(APPEND extracted "\nvoid CObject::InitSplash() { ${assignments}; }\n")
string(REGEX MATCH "bool CObject::IsSplashDamageWeapon \\(void\\)[ \r\n]*\\{[^}]*\\}"
             classification "${objects}")
if(NOT classification)
    message(FATAL_ERROR "Missing source splash classification")
endif()
string(APPEND extracted "${classification}\n")
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/effects/fireball.cpp" fireball)
file(READ "${XL_REFERENCE_GEOMETRY_ROOT}/physics/collide.cpp" collision)
foreach(kind IN ITEMS Weapon Actor Water)
    if(kind STREQUAL "Weapon")
        set(source "${fireball}")
        set(pattern
            "audio.CreateObjectSound \\(gameStates.app.bD1Mission[^\n]+IsSplashDamageWeapon[^;\n]+")
    elseif(kind STREQUAL "Actor")
        set(source "${fireball}")
        set(pattern
            "audio.CreateObjectSound \\(gameStates.app.bD1Mission[^\n]+SOUND_BADASS_EXPLOSION_ACTOR[^;\n]+"
        )
    else()
        set(source "${collision}")
        set(pattern "audio.CreateObjectSound \\(IsSplashDamageWeapon[^;\n]+")
    endif()
    string(REGEX MATCHALL "${pattern}" calls "${source}")
    list(LENGTH calls count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "Missing or ambiguous ${kind} explosion audio call")
    endif()
    string(
        APPEND extracted
        "\nvoid CObject::${kind}Sound() { CObject *pExplObj = this; (void)pExplObj; ${calls}; }\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_combat_audio.h"
     CONTENT "${extracted}")
target_sources(xl_named_audio_source PRIVATE tests/reference_combat_audio.cpp)
