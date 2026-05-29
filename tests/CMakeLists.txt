# cmake version to be used
cmake_minimum_required(VERSION 3.5)

if (HAVE_TESTS)
    enable_testing()

    # Prefer CMake's FindBoost module over BoostConfig.cmake. Some systems have
    # stale /usr/local Boost config files that point to removed libraries, while
    # the distro Boost packages are valid and complete.
    set(Boost_NO_BOOST_CMAKE ON)
    find_package(Boost REQUIRED COMPONENTS unit_test_framework filesystem system)
    # set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    # set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

    file(GLOB_RECURSE test_source "*.cpp" "unit-tests/*.cpp")
    add_executable(unit-tests ${test_source})
    target_include_directories(unit-tests PUBLIC .)
    target_link_libraries(unit-tests PUBLIC nac-abe)
    target_link_libraries(unit-tests PUBLIC ${OPENABE_LIBRARY})
    if (RELIC_LIBRARY)
        target_link_libraries(unit-tests PUBLIC ${RELIC_LIBRARY})
    endif()
    if (RELIC_EC_LIBRARY)
        target_link_libraries(unit-tests PUBLIC ${RELIC_EC_LIBRARY})
    endif()
    target_link_libraries(unit-tests PUBLIC
            Boost::unit_test_framework
            Boost::filesystem
            Boost::system)
    set(NAC_ABE_TEST_RUNTIME_LIBRARY_DIRS "${CMAKE_BINARY_DIR}")
    if (NAC_ABE_RUNTIME_LIBRARY_DIRS)
        list(APPEND NAC_ABE_TEST_RUNTIME_LIBRARY_DIRS ${NAC_ABE_RUNTIME_LIBRARY_DIRS})
    endif()
    list(REMOVE_DUPLICATES NAC_ABE_TEST_RUNTIME_LIBRARY_DIRS)
    if (NAC_ABE_RUNTIME_LIBRARY_DIRS)
        set_target_properties(unit-tests PROPERTIES
                BUILD_RPATH "${NAC_ABE_TEST_RUNTIME_LIBRARY_DIRS}"
                INSTALL_RPATH "${NAC_ABE_TEST_RUNTIME_LIBRARY_DIRS}")
        string(REPLACE ";" ":" NAC_ABE_TEST_LIBRARY_PATH "${NAC_ABE_TEST_RUNTIME_LIBRARY_DIRS}")
        if (DEFINED ENV{LD_LIBRARY_PATH})
            set(NAC_ABE_TEST_LIBRARY_PATH "${NAC_ABE_TEST_LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH}")
        endif()
    endif()

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/unit-tests/trust-schema.conf
            ${CMAKE_BINARY_DIR}/trust-schema.conf COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/unit-tests/example-trust-anchor.cert
            ${CMAKE_BINARY_DIR}/example-trust-anchor.cert COPYONLY)

    add_test(NAME unit-tests COMMAND unit-tests -l test_suite -x)
    set_tests_properties(unit-tests PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    if (NAC_ABE_TEST_LIBRARY_PATH)
        set_tests_properties(unit-tests PROPERTIES
                ENVIRONMENT "LD_LIBRARY_PATH=${NAC_ABE_TEST_LIBRARY_PATH}")
    endif()

    if (BUILD_EXAMPLES)
        add_test(NAME example-tests COMMAND bash ${CMAKE_SOURCE_DIR}/examples/run-examples.sh ${CMAKE_BINARY_DIR})
    endif (BUILD_EXAMPLES)
endif (HAVE_TESTS)
