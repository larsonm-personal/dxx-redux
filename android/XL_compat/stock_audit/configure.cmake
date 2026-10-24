# Opt-in CMAKE_PROJECT_INCLUDE for a separate D2 audit build tree only
if(PROJECT_NAME STREQUAL "dxx-redux-d2")
    function(xl_configure_stock_audit)
        set(audit_target dxx-redux-d2-headless-metadata)
        get_target_property(stock_sources ${audit_target} SOURCES)
        list(FILTER stock_sources EXCLUDE REGEX "headless_metadata_dump_main\\.cpp$")
        set_property(TARGET ${audit_target} PROPERTY SOURCES "${stock_sources}")
        target_sources(
            ${audit_target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/audit.cpp"
                                    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../inventory.cpp")
        target_compile_features(${audit_target} PRIVATE cxx_std_17)
        set_target_properties(${audit_target} PROPERTIES OUTPUT_NAME xl_stock_audit)
    endfunction()
    cmake_language(DEFER CALL xl_configure_stock_audit)
endif()
