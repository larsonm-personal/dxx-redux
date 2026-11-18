foreach(
    pair IN
    ITEMS
        "main/producers.cpp|c79263fcd98f686d99dfcc8f5e12aade60f70f75cc25544795fbab71c93318b6"
        "main/trigger.cpp|74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852"
        "gameio/savegame.cpp|4784770058b2d4880c33a81eb1a7611738959113a80bce48de248eee7a54a795"
        "objects/createobject.cpp|5d5a50c7c2fa3799f9e45c0e9e5a12afd3ebfbb165a1dbb0b792dd38adb8bbf3"
        "objects/powerup.cpp|5312c9cd927adbfaa71d298aadeacee8bab7d5190a27db4534d55c4dbbf4d366")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Equipment reference differs from audited revision: ${path}")
    endif()
endforeach()
set(extracted "// Extracted XL equipment production and powerup creation\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "inline int32_t CTrigger::HasTarget (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp" "CWall *FindTriggerWall (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "int32_t FindTriggerSegSide (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "int32_t ObjTriggerIsValid (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/trigger.cpp"
                      "int32_t FindTriggerTarget (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp"
                      "void SetEquipmentMakerStates (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp"
                      "int32_t GetObjProducerObjType (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp"
                      "void CreateObjectProducerEffect (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/createobject.cpp"
                      "int32_t CreatePowerup (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/main/producers.cpp"
                      "void EquipmentMakerHandler (")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_equipment.h" CONTENT "${extracted}")
add_executable(xl_equipment_reference tests/reference_equipment.cpp)
target_compile_features(xl_equipment_reference PRIVATE cxx_std_17)
target_include_directories(xl_equipment_reference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
set(extracted "// Actual pinned single-player pickup dispatch and multiplayer gates\n")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/powerup.cpp"
                      "int32_t PickupHoardOrb (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/powerup.cpp" "int32_t PickupFlag (")
extract_face_function("${XL_REFERENCE_GEOMETRY_ROOT}/objects/powerup.cpp" "int32_t DoPowerup (")
file(STRINGS "${XL_REFERENCE_GEOMETRY_ROOT}/objects/powerup.cpp" pickup_handlers
     REGEX "^pickupHandler \\[POW_(TURBO|MEGAWOW|MONSTERBALL|HOARD_ORB|BLUEFLAG|REDFLAG)\\]")
list(LENGTH pickup_handlers pickup_handler_count)
if(NOT pickup_handler_count EQUAL 6)
    message(FATAL_ERROR "Pinned inert equipment handler assignments differ")
endif()
list(JOIN pickup_handlers "\n" pickup_handler_text)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xl_reference_equipment_pickup.h"
     CONTENT "${extracted}\nvoid initialize_pickup_handlers() {\n${pickup_handler_text}\n}\n")
target_sources(xl_equipment_reference PRIVATE tests/reference_equipment_pickup.cpp)
