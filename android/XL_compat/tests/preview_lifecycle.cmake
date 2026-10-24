include("${CMAKE_CURRENT_LIST_DIR}/stock_probe.cmake")
configure_file("${fixture}/xlprobe.rl2" "${fixture}/stockprobe.rl2" COPYONLY)
configure_file("${LEVEL}" "${fixture}/d2leva-1.rl2" COPYONLY)
set(mission_test_args)
if(DEFINED CROSS_FROM)
    if(NOT DEFINED BRANCH OR NOT DEFINED CROSS_TO OR DEFINED VISIT)
        message(
            FATAL_ERROR "Portal crossing requires an earned branch checkpoint and both endpoints")
    endif()
    list(APPEND mission_test_args -xl-test-cross "${CROSS_FROM}:${CROSS_TO}")
endif()
if(DEFINED VISIT)
    if(NOT NAVIGATION OR ESCAPE OR DEFINED BRANCH)
        message(FATAL_ERROR "Secret entry requires an earned key/reactor phase")
    endif()
    list(APPEND mission_test_args -xl-test-visit "${VISIT}")
endif()
if(DEFINED BRANCH)
    if(NOT NAVIGATION OR REACTOR OR ESCAPE)
        message(FATAL_ERROR "Branch coverage requires only the earned red-key phase")
    endif()
    list(APPEND mission_test_args -xl-test-branch "${BRANCH}")
endif()
if(ESCAPE AND NOT REACTOR)
    message(FATAL_ERROR "Escape coverage requires the earned reactor checkpoint")
endif()
if(ESCAPE)
    list(APPEND mission_test_args -xl-test-escape)
endif()
if(REACTOR AND NOT NAVIGATION)
    message(FATAL_ERROR "Reactor coverage requires the earned red-key navigation checkpoint")
endif()
if(NAVIGATION)
    if(NOT PILOT_DESCRIPTOR OR NOT PILOT_HOG)
        message(FATAL_ERROR "Normal navigation requires the original mission descriptor and HOG")
    endif()
    list(APPEND mission_test_args -xl-test-navigation)
    if(REACTOR)
        list(APPEND mission_test_args -xl-test-reactor)
    endif()
endif()
if(PILOT_DESCRIPTOR AND PILOT_HOG)
    file(MAKE_DIRECTORY "${fixture}/missions")
    configure_file("${PILOT_DESCRIPTOR}" "${fixture}/missions/d2-xl.mn2" COPYONLY)
    file(CREATE_LINK "${PILOT_HOG}" "${fixture}/missions/d2-xl.hog" COPY_ON_ERROR)
    list(APPEND mission_test_args -xl-test-save)
endif()
execute_process(
    COMMAND
        "${PREVIEW}" -xl-pilot -pilot-hog "${PILOT_HOG}" -xl-static-light-preview -xl-source
        "${LEVEL}" -fixture-path "${fixture}/xlprobe.rl2" -hogdir "${DATA}" -extra-dir "${fixture}"
        -mission xlprobe -userdir "${fixture}/preview-user" ${mission_test_args}
    RESULT_VARIABLE result
    TIMEOUT 60
    OUTPUT_FILE "${fixture}/preview.log"
    ERROR_FILE "${fixture}/preview-error.log")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "XL preview lifecycle failed: ${result}; see ${fixture}")
endif()
file(READ "${fixture}/preview.log" report)
if(DEFINED CROSS_FROM AND NOT report MATCHES
                          "Normal linked portal: from=${CROSS_FROM} to=${CROSS_TO} crossed=1")
    message(FATAL_ERROR "Linked portal was not physically crossed")
endif()
if(DEFINED VISIT
   AND NOT report MATCHES
       "Normal secret entry: target=${VISIT} actual=${VISIT} frames=[1-9][0-9]* dead=0")
    message(FATAL_ERROR "Normal-input physical secret entry was not established")
endif()
if(NOT report MATCHES "Corona geometry: visible=112 solid-wall controls=148; GPU output unverified")
    message(FATAL_ERROR "Corona visibility and solid-wall controls did not finish")
endif()
if(NOT
   report
   MATCHES
   "Texture emitters: total=112 chromatic=24; native light switching and texture/wall removal verified; rendering unverified"
)
    message(FATAL_ERROR "Authored texture emitter state coverage did not finish")
endif()
if(ESCAPE AND NOT report MATCHES
              "Normal escape recording: earned checkpoint and ordered flight inputs exported")
    message(FATAL_ERROR "Earned escape recording was not exported")
endif()
if(NOT
   report
   MATCHES
   "XL multiplayer admission: original and translated preview rejected in network/cooperative modes, stock loader and single-player recovery passed"
)
    message(FATAL_ERROR "XL multiplayer admission boundary was not verified")
endif()
if(NOT
   report
   MATCHES
   "All 14 authored-opacity illusions pass native AI/object visibility and weapon-object collision queries with solid-boundary controls"
)
    message(FATAL_ERROR "Authored-material targeting coverage did not finish")
endif()
if(DEFINED BRANCH)
    if(NOT report MATCHES
       "Normal branch persistence: trigger=${BRANCH} targets and earned resources restored"
       OR NOT EXISTS "${fixture}/preview-user/xl-normal-branch.sav")
        message(FATAL_ERROR "Branch checkpoint persistence was not verified")
    endif()
endif()
if(DEFINED BRANCH
   AND NOT
       report
       MATCHES
       "Normal branch navigation: trigger=${BRANCH} frames=[0-9]+ dead=0 activated=1 targets_passable=1"
)
    message(FATAL_ERROR "Authored branch did not activate with passable linked targets")
endif()
if(ESCAPE AND NOT report MATCHES
              "Normal escape navigation: frames=[0-9]+ dead=0 completed=1 level=1")
    message(FATAL_ERROR "Timed escape did not reach native completion within pilot scope")
endif()
if(REACTOR AND (NOT report MATCHES "Normal reactor navigation: frames=[0-9]+ dead=0 destroyed=1"
                OR NOT EXISTS "${fixture}/preview-user/xl-normal-reactor.sav"))
    message(FATAL_ERROR "Normal reactor objective did not finish with its earned checkpoint")
endif()
if(NAVIGATION AND NOT report MATCHES "Normal red-key navigation: frames=[0-9]+ dead=0 red_key=1")
    message(FATAL_ERROR "Normal red-key navigation did not establish a living key pickup")
endif()
if(NAVIGATION AND NOT EXISTS "${fixture}/preview-user/xl-normal-key.sav")
    message(FATAL_ERROR "Normal navigation did not preserve its actual player checkpoint")
endif()
if(NOT report MATCHES "recover passed")
    message(FATAL_ERROR "XL preview lifecycle did not reach final assertion")
endif()
if(mission_test_args)
    if(NOT report MATCHES "Original mission save: identity"
       OR NOT EXISTS "${fixture}/preview-user/xl-pilot-state.sav")
        message(
            FATAL_ERROR "Original mission save coverage did not finish in isolated write directory")
    endif()
endif()
file(SHA256 "${LEVEL}" after_source_hash)
file(SHA256 "${fixture}/xlprobe.rl2" restored_hash)
file(SHA256 "${fixture}/stockprobe.rl2" fixture_hash)
if(NOT after_source_hash STREQUAL original_hash OR NOT restored_hash STREQUAL fixture_hash)
    message(FATAL_ERROR "Lifecycle test changed the source or failed to restore its fixture")
endif()
message(STATUS "XL preview lifecycle passed; rendering and gameplay remain unverified")
