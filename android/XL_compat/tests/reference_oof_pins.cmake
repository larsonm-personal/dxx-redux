# Exact Git blobs at 2576a98b5de310e3ac7c9777df2677d1627b8bae and their CRLF equivalents Hash raw
# bytes: no normalization can conceal mixed endings, BOMs or content changes
foreach(
    pair IN
    ITEMS
        "models/oofread.cpp|fe10165610d46b26a270715e4e2f0ecdca7ca61e9079805e5b3ffc8aa1095d17|97cc190e8d901d8ce49be52464727801caf2aab8c958916fa3903e369794a91d"
        "render/oofrender.cpp|7a82ee47b2ecd29e37456355dcfc03692da76a198067cb74f1f31a752f904abb|97bdbddd6771e5ed55014fba48e13945591fe044e326ab153c1c038d74168e26"
        "3d/oof2model.cpp|acbcc5867ca48f9c660aa7b6da8796c8749fd5bb0a044485e21b1c948827f032|aee6218b50428f975cd7b551a5cd749cb74bab421bf491e2c853ecdea62160e0"
)
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 path)
    list(GET parts 1 expected_lf)
    list(GET parts 2 expected_crlf)
    file(SHA256 "${XL_REFERENCE_GEOMETRY_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected_lf AND NOT actual STREQUAL expected_crlf)
        message(FATAL_ERROR "OOF reference changed: ${path}")
    endif()
endforeach()
