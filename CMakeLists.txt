cmake_minimum_required(VERSION 3.25)

project(nova_nonnull LANGUAGES CXX)

if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_MSVC_DEBUG_INFORMATION_FORMAT)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded")
endif()

if (PROJECT_IS_TOP_LEVEL)
    add_custom_target(nova_nonnull_project_files SOURCES
        .clang-tidy
        .pre-commit-config.yaml
        _clang-format
        README.md
        LICENSE
        .github/workflows/ci.yml
    )
endif()

# Project layout
add_library(nova_nonnull INTERFACE)
add_library(nova::nonnull ALIAS nova_nonnull)

target_sources(nova_nonnull PUBLIC FILE_SET HEADERS
    FILES include/nova/non_null.hpp)

target_include_directories(nova_nonnull INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Testing
option(NOVA_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
if(NOVA_BUILD_TESTS)
    if (NOT COMMAND CPMAddPackage)
        set(CPM_DOWNLOAD_VERSION 0.42.3)
        if(NOT EXISTS "${CMAKE_BINARY_DIR}/cmake/CPM.cmake")
            message(STATUS "Downloading CPM.cmake ${CPM_DOWNLOAD_VERSION}...")
            file(DOWNLOAD
                "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
                "${CMAKE_BINARY_DIR}/cmake/CPM.cmake"
                TLS_VERIFY ON
                EXPECTED_HASH SHA256=a609e875fd532b067174250f6abbc3dac22fe2d64869783fb1e80bda1625c844
            )
        endif()
        include("${CMAKE_BINARY_DIR}/cmake/CPM.cmake")
    endif()

    if (NOT TARGET Catch2::Catch2WithMain)
        block()
            set(CMAKE_UNITY_BUILD ON)
            CPMAddPackage(
                NAME Catch2
                GITHUB_REPOSITORY catchorg/Catch2
                VERSION 3.15.0
            )
        endblock()
    endif()

    add_executable(nova_nonnull_tests
        tests/test_non_null.cpp
        tests/test_non_null_function.cpp
    )
    target_link_libraries(nova_nonnull_tests PRIVATE nova::nonnull Catch2::Catch2WithMain)

    enable_testing()
    add_test(NAME nova_nonnull_tests COMMAND nova_nonnull_tests)


    add_executable(nova_nonnull_asan_take_tests
        tests/asan_take.cpp
    )
    target_link_libraries(nova_nonnull_asan_take_tests PRIVATE nova::nonnull )
    target_compile_options(nova_nonnull_asan_take_tests PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-fsanitize=address;-fno-omit-frame-pointer>
        $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/fsanitize=address>
    )

    target_link_options(nova_nonnull_asan_take_tests PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-fsanitize=address>
        $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-static-libasan>
    )

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(nova_nonnull_asan_take_tests PRIVATE NDEBUG)

        add_test(NAME nova_nonnull_asan_take_tests
            COMMAND ${CMAKE_COMMAND} -E env $<TARGET_FILE:nova_nonnull_asan_take_tests>
        )
        set_tests_properties(nova_nonnull_asan_take_tests PROPERTIES
            PASS_REGULAR_EXPRESSION ".*AddressSanitizer: use-after-poison.*"
        )
    endif()
endif()
