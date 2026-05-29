# cmake version to be used
cmake_minimum_required(VERSION 3.5)

# project name
project(nac-abe
        VERSION 0.1
        DESCRIPTION "NDN Name-based access control - attribute based encryption library")

# flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if (HAVE_TESTS)
    add_compile_definitions(HAVE_TESTS)
    enable_testing()
endif(HAVE_TESTS)

# dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(NDN_CXX REQUIRED libndn-cxx>=0.8.1)
find_package(OpenSSL REQUIRED)

set(OPENABE_ROOT "" CACHE PATH "OpenABE installation prefix")
set(OPENABE_RELIC_ROOT "" CACHE PATH "OpenABE bundled RELIC/OpenSSL dependency prefix")
if (OPENABE_ROOT)
    find_path(OPENABE_INCLUDE_DIR
            NAMES openabe/openabe.h
            HINTS ${OPENABE_ROOT}
            PATH_SUFFIXES include
            NO_DEFAULT_PATH)
else()
    find_path(OPENABE_INCLUDE_DIR
            NAMES openabe/openabe.h
            PATH_SUFFIXES include)
endif()
if (NOT OPENABE_INCLUDE_DIR)
    message(FATAL_ERROR "OpenABE headers not found; set -DOPENABE_ROOT=/path/to/openabe-prefix")
endif()
if (OPENABE_ROOT)
    find_library(OPENABE_LIBRARY
            NAMES openabe
            HINTS ${OPENABE_ROOT}
            PATH_SUFFIXES lib lib64
            NO_DEFAULT_PATH
            REQUIRED)
else()
    find_library(OPENABE_LIBRARY
            NAMES openabe
            PATH_SUFFIXES lib lib64
            REQUIRED)
endif()
if (OPENABE_ROOT OR OPENABE_RELIC_ROOT)
    find_library(RELIC_LIBRARY
            NAMES relic
            HINTS ${OPENABE_ROOT} ${OPENABE_RELIC_ROOT}
            PATH_SUFFIXES lib lib64 deps/root/lib
            NO_DEFAULT_PATH)
    find_library(RELIC_EC_LIBRARY
            NAMES relic_ec
            HINTS ${OPENABE_ROOT} ${OPENABE_RELIC_ROOT}
            PATH_SUFFIXES lib lib64 deps/root/lib
            NO_DEFAULT_PATH)
else()
    find_library(RELIC_LIBRARY
            NAMES relic
            PATH_SUFFIXES lib lib64 deps/root/lib)
    find_library(RELIC_EC_LIBRARY
            NAMES relic_ec
            PATH_SUFFIXES lib lib64 deps/root/lib)
endif()

set(NAC_ABE_RUNTIME_LIBRARY_DIRS)
foreach(_library ${OPENABE_LIBRARY} ${RELIC_LIBRARY} ${RELIC_EC_LIBRARY})
    if (_library)
        get_filename_component(_library_dir "${_library}" DIRECTORY)
        list(APPEND NAC_ABE_RUNTIME_LIBRARY_DIRS "${_library_dir}")
    endif()
endforeach()
if (OPENABE_ROOT)
    list(APPEND NAC_ABE_RUNTIME_LIBRARY_DIRS
            "${OPENABE_ROOT}/lib"
            "${OPENABE_ROOT}/lib64"
            "${OPENABE_ROOT}/deps/root/lib")
endif()
if (OPENABE_RELIC_ROOT)
    list(APPEND NAC_ABE_RUNTIME_LIBRARY_DIRS
            "${OPENABE_RELIC_ROOT}/lib"
            "${OPENABE_RELIC_ROOT}/lib64")
endif()
list(REMOVE_DUPLICATES NAC_ABE_RUNTIME_LIBRARY_DIRS)

# files
file(GLOB_RECURSE NAC_ABE_LIB_SOURCE_FILES "src/*.cpp" "src/**/*.cpp")

# include
include_directories(${NDN_CXX_INCLUDE_DIRS})
if (OPENABE_INCLUDE_DIR)
    include_directories(${OPENABE_INCLUDE_DIR})
endif()
include_directories(src)
include_directories(build/src)

add_compile_definitions(SYSCONFDIR="${CMAKE_INSTALL_PREFIX}/etc")
add_compile_definitions(NAC_ABE_CMAKE_BUILD)

# link
link_directories(${NDN_CXX_LIBRARY_DIRS})

# target
add_library(nac-abe SHARED ${NAC_ABE_LIB_SOURCE_FILES})
target_compile_options(nac-abe PUBLIC ${NDN_CXX_CFLAGS})
target_link_libraries(nac-abe PUBLIC ${NDN_CXX_LIBRARIES} ${OPENABE_LIBRARY})
if (RELIC_LIBRARY)
    target_link_libraries(nac-abe PUBLIC ${RELIC_LIBRARY})
endif()
if (RELIC_EC_LIBRARY)
    target_link_libraries(nac-abe PUBLIC ${RELIC_EC_LIBRARY})
endif()
if (NAC_ABE_RUNTIME_LIBRARY_DIRS)
    set_target_properties(nac-abe PROPERTIES
            BUILD_RPATH "${NAC_ABE_RUNTIME_LIBRARY_DIRS}"
            INSTALL_RPATH "${NAC_ABE_RUNTIME_LIBRARY_DIRS}")
endif()

add_subdirectory(tests)
add_subdirectory(examples)

# install
set(PKG_CONFIG_REQUIRES
        "libndn-cxx >= ${NDN_CXX_VERSION}"
        )
set(PKG_CONFIG_LIBS
        "-L\${libdir} -l${PROJECT_NAME}"
        )
set(PKG_CONFIG_CFLAGS
        "-I\${includedir} -DNAC_ABE_CMAKE_BUILD"
        )
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake-pkgconfig.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.pc")

include(GNUInstallDirs)
install(TARGETS nac-abe
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nac-abe
        )
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nac-abe
        FILES_MATCHING PATTERN "*.h*")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.pc"
        DESTINATION lib/pkgconfig)
