# Third-party libraries may have substandard code.

# Put all targets defined here and in added subfolders under "contrib/" folder in GUI-based IDEs by default.
# Some of third-party projects may override CMAKE_FOLDER or FOLDER property of their targets, so they will
# appear not in "contrib/" as originally planned here.
get_filename_component (_current_dir_name "${CMAKE_CURRENT_LIST_DIR}" NAME)
if (CMAKE_FOLDER)
    set (CMAKE_FOLDER "${CMAKE_FOLDER}/${_current_dir_name}")
else ()
    set (CMAKE_FOLDER "${_current_dir_name}")
endif ()
unset (_current_dir_name)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")

if (WITH_COVERAGE)
  set (WITHOUT_COVERAGE_LIST ${WITHOUT_COVERAGE})
  separate_arguments(WITHOUT_COVERAGE_LIST)
  # disable coverage for contib files and build with optimisations
  add_compile_options(-O3 -DNDEBUG -finline-functions -finline-hint-functions ${WITHOUT_COVERAGE_LIST})
endif()

if (SANITIZE STREQUAL "undefined")
    # 3rd-party libraries usually not intended to work with UBSan.
    add_compile_options(-fno-sanitize=undefined)
endif()

set_property(DIRECTORY PROPERTY EXCLUDE_FROM_ALL 1)

add_subdirectory (boost-cmake)
add_subdirectory (cctz-cmake)
add_subdirectory (dragonbox-cmake)
add_subdirectory (FastMemcpy)
add_subdirectory (jemalloc-cmake)
add_subdirectory (poco-cmake)
add_subdirectory (double-conversion-cmake)

add_subdirectory (cityhash102)

add_subdirectory (boringssl-cmake)
add_library(OpenSSL::Crypto ALIAS crypto)
add_library(OpenSSL::SSL ALIAS ssl)

set(GOOGLETEST_VERSION 1.14.0) # master
add_subdirectory(${RaftKeeper_SOURCE_DIR}/contrib/googletest/googletest ${CMAKE_CURRENT_BINARY_DIR}/googletest)
target_compile_definitions (gtest INTERFACE GTEST_HAS_POSIX_RE=0)

add_subdirectory(libxml2-cmake)
add_subdirectory (fmtlib-cmake)
add_subdirectory(nuraft-cmake)
