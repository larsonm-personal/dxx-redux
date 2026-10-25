file(SHA256 "${HOG}" hog_hash)
file(SHA256 "${DESCRIPTOR}" descriptor_hash)
if(NOT hog_hash STREQUAL "e89acaad09cefa0c09fc4b487c56b6077477cfe20169f364fe54f627040b01e7"
   OR NOT descriptor_hash STREQUAL
      "8a2946c7f786f4834e1c9e303a8d4248b419b89479a750d0d6c1917fbe1b6317")
    message(FATAL_ERROR "Descriptor test requires original Observatory HOG and descriptor")
endif()
file(MAKE_DIRECTORY "${OUTPUT}/missions" "${OUTPUT}/preview-user")
configure_file("${HOG}" "${OUTPUT}/missions/obsrv-xl.hog" COPYONLY)
configure_file("${DESCRIPTOR}" "${OUTPUT}/missions/obsrv-xl.mn2" COPYONLY)
execute_process(
    COMMAND "${ENGINE}" -descriptor-test observatory -hogdir "${DATA}" -extra-dir "${OUTPUT}"
            -userdir "${OUTPUT}/preview-user" -nosound -nomusic
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 60)
file(WRITE "${OUTPUT}/descriptor.log" "${stdout}${stderr}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Descriptor test failed (${result}): ${stdout}${stderr}")
endif()
file(SHA256 "${HOG}" final_hog_hash)
if(NOT
   stdout
   MATCHES
   "XL descriptor base initialization: stale robot definitions reset, owned baseline captured, extra-HAM dialect not invoked"
)
    message(FATAL_ERROR "Descriptor resource initialization coverage did not finish")
endif()
file(SHA256 "${DESCRIPTOR}" final_descriptor_hash)
if(NOT stdout MATCHES
   "Mission archive preflight: corrupt and foreign-owned HOGs preserve prior state")
    message(FATAL_ERROR "Native companion archive controls did not finish")
endif()
if(NOT stdout MATCHES "late corruption/loss retain prior mission and search order"
   OR NOT stdout MATCHES
      "Mission archive handoff: busy previous archive rejects replacement and loose-level selection"
)
    message(FATAL_ERROR "Native archive handoff preservation controls did not finish")
endif()
if(NOT stdout MATCHES
   "Mission identity preflight: object and path allocation failures retain prior mission")
    message(FATAL_ERROR "Mission identity allocation failure controls did not finish")
endif()
if(NOT stdout MATCHES "No-damage fusion input controls: protected fresh input does not charge")
    message(FATAL_ERROR "Native no-damage fusion input controls did not finish")
endif()
if(NOT stdout MATCHES "No-damage native fusion frames: protected timer queues charge")
    message(FATAL_ERROR "Native no-damage fusion frame controls did not finish")
endif()
if(NOT stdout MATCHES "No-damage native guided controls: fresh protected fire retains control")
    message(FATAL_ERROR "Native no-damage guided controls did not finish")
endif()
if(NOT stdout MATCHES "No-damage native bump controls: player/clutter and player/player dispatch")
    message(FATAL_ERROR "Native no-damage bump controls did not finish")
endif()
if(NOT stdout MATCHES "No-damage native splash controls: radius explosion retains identical force")
    message(FATAL_ERROR "Native no-damage splash controls did not finish")
endif()
if(NOT stdout MATCHES "No-damage native wall controls: hit-segment damage suppression")
    message(FATAL_ERROR "Native no-damage wall controls did not finish")
endif()
if(NOT stdout MATCHES "No-damage native firing controls: primary and fresh missiles/bombs blocked")
    message(FATAL_ERROR "Native no-damage firing controls did not finish")
endif()
if(NOT stdout MATCHES
   "No-damage native collision controls: protected damage and impacts preserve state")
    message(FATAL_ERROR "Native no-damage collision controls did not finish")
endif()
if(NOT stdout MATCHES
   "17/36 native layout control: name, geometry, reserved bits and packed light indices verified")
    message(FATAL_ERROR "Native 17/36 layout control did not finish")
endif()
if(NOT
   stdout
   MATCHES
   "Android acquisition failure: late archive loss preserves active ownership and reset state; fresh preparation recovers"
)
    message(FATAL_ERROR "Android late acquisition failure recovery did not finish")
endif()
if(NOT
   stdout
   MATCHES
   "Android mount preflight: corrupt archives, duplicate paths and foreign ownership rejected before reset; active context and reselection preserved"
)
    message(FATAL_ERROR "Android mount preflight state preservation did not finish")
endif()
if(NOT
   stdout
   MATCHES
   "Descriptor commit uses owned validated fields despite source replacement; text order and fallback preserved"
)
    message(FATAL_ERROR "Validated descriptor ownership was not verified")
endif()
if(NOT
   stdout
   MATCHES
   "Descriptor preflight: malformed candidates retain prior mission, archive and owned resource baseline"
)
    message(FATAL_ERROR "Malformed descriptor state preservation was not verified")
endif()
if(NOT final_hog_hash STREQUAL hog_hash OR NOT final_descriptor_hash STREQUAL descriptor_hash)
    message(FATAL_ERROR "Original descriptor sources changed")
endif()
message(STATUS "${stdout}")
