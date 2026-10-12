cmake_minimum_required(VERSION 3.16)

# The caller writes a one-package manifest from the checked-in import/source oracles
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/dxx-verified-dependencies.cmake")
dxx_verified_cached_file(SHAREWARE package cached_package)
configure_file("${cached_package}" "${DXX_SHAREWARE_ARCHIVE_PATH}" COPYONLY)
