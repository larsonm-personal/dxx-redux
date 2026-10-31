file(SHA256 "${HOG}" original_hash)
if(NOT original_hash STREQUAL "4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303")
    message(FATAL_ERROR "Effect rejection control requires unchanged original Neptune")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user" "${OUTPUT}/fixtures")
# Initialize the real startup owner from disposable copies, never retail files
set(installation "${OUTPUT}/installation")
file(MAKE_DIRECTORY "${installation}")
file(GLOB base_files LIST_DIRECTORIES false "${DATA}/*")
foreach(base_file IN LISTS base_files)
    get_filename_component(base_name "${base_file}" NAME)
    configure_file("${base_file}" "${installation}/${base_name}" COPYONLY)
endforeach()
configure_file("${HOG}" "${OUTPUT}/missions/neptunxl.hog" COPYONLY)
execute_process(COMMAND "${FIXTURE}" --effect-fixtures "${OUTPUT}/fixtures" RESULT_VARIABLE result
                OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Effect inspection fixtures failed: ${log}${error}")
endif()
set(particle_world_args)
set(particle_original_args)
if(PUMO_PACKAGE)
    file(SHA256 "${PUMO_PACKAGE}/missions/pmines.hog" pumo_hash)
    file(SHA256 "${PUMO_PACKAGE}/missions/pmines.mn2" descriptor_hash)
    file(SHA256 "${FIRE_IMAGE}" fire_hash)
    if(NOT pumo_hash STREQUAL "6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043"
       OR NOT descriptor_hash STREQUAL
          "45b4138e0a0d49ab8ee09334b7cfa97c0ec75ac6c6b6406b21eba28ddaa19cfd"
       OR NOT fire_hash STREQUAL "b7bd821c02ba917a4cb0ff503f2e83d4e2007c19f0450728525df2f3af7f738f")
        message(
            FATAL_ERROR
                "Original fire qualification requires unchanged Pumo archive/descriptor and XL atlas"
        )
    endif()
    list(APPEND particle_original_args -pumo-package "${PUMO_PACKAGE}" -particle-original-image
         "${FIRE_IMAGE}")
endif()
if(REFERENCE)
    list(APPEND particle_world_args -particle-world-export "${OUTPUT}/particle-world.txt")
endif()
execute_process(
    COMMAND
        "${ENGINE}" -effect-inspection-test "${OUTPUT}/fixtures" -hogdir "${installation}"
        -particle-installation-fixture "${installation}" -extra-dir "${OUTPUT}" -userdir
        "${OUTPUT}/preview-user" -nosound -nomusic ${particle_world_args} ${particle_original_args}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE log
    ERROR_VARIABLE error
    TIMEOUT 80)
file(WRITE "${OUTPUT}/controls.log" "${log}${error}")
string(FIND "${log}" "XL effect inspection gate passed" witness)
if(NOT result EQUAL 0 OR witness LESS 0)
    message(FATAL_ERROR "Effect inspection gate failed (${result}): ${log}${error}")
endif()
if(PUMO_PACKAGE)
    string(FIND "${log}" "Original Pumo native admission:" original_witness)
    if(original_witness LESS 0)
        message(FATAL_ERROR "Original Pumo native admission witness missing")
    endif()
    file(SHA256 "${PUMO_PACKAGE}/missions/pmines.hog" final_pumo)
    file(SHA256 "${PUMO_PACKAGE}/missions/pmines.mn2" final_descriptor)
    if(NOT final_pumo STREQUAL pumo_hash OR NOT final_descriptor STREQUAL descriptor_hash)
        message(FATAL_ERROR "Original Pumo package changed during fire qualification")
    endif()
endif()
foreach(version IN ITEMS 23 25 27)
    string(
        FIND "${log}"
             "XL effect inspection version ${version}: stock/XL rejection and supported recovery passed"
             witness)
    if(witness LESS 0)
        message(FATAL_ERROR "Missing effect version ${version} rejection/recovery witness")
    endif()
endforeach()
string(FIND "${log}" "XL particle native ownership:" particle_witness)
if(particle_witness LESS 0)
    message(FATAL_ERROR "Missing native particle ownership witness")
endif()
string(FIND "${log}" "XL transparency native frame:" transparency_witness)
if(transparency_witness LESS 0)
    message(FATAL_ERROR "Missing native shared transparency frame witness")
endif()
string(FIND "${log}" "XL particle image resources:" image_witness)
if(image_witness LESS 0)
    message(FATAL_ERROR "Missing native particle image resource witness")
endif()
if(particle_world_args)
    execute_process(
        COMMAND "${REFERENCE}" --particle-world "${OUTPUT}/particle-world.txt"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE log
        ERROR_VARIABLE error
        TIMEOUT 30)
    file(WRITE "${OUTPUT}/particle-world-reference.log" "${log}${error}")
    string(FIND "${log}" "Pinned loaded particle world:" witness)
    if(NOT result EQUAL 0 OR witness LESS 0)
        message(
            FATAL_ERROR "Loaded particle world source comparison failed (${result}): ${log}${error}"
        )
    endif()
endif()
file(SHA256 "${HOG}" final_hash)
if(NOT final_hash STREQUAL original_hash)
    message(FATAL_ERROR "Original Neptune changed during effect rejection control")
endif()
