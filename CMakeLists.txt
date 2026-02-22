cmake_minimum_required(VERSION 3.14)

project(nova_nonnull VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
option(NOVA_BUILD_TESTS "Build unit tests" ON)
if(NOVA_BUILD_TESTS)
    if (NOT COMMAND CPMAddPackage)
        # CPM.cmake downloader
        set(CPM_DOWNLOAD_VERSION 0.40.2)
        if(NOT EXISTS "${CMAKE_BINARY_DIR}/cmake/CPM.cmake")
          message(STATUS "Downloading CPM.cmake ${CPM_DOWNLOAD_VERSION}...")
          file(DOWNLOAD
               "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
               "${CMAKE_BINARY_DIR}/cmake/CPM.cmake"
          )
        endif()
        include("${CMAKE_BINARY_DIR}/cmake/CPM.cmake")
    endif()

    if (NOT TARGET Catch2::Catch2WithMain)
        CPMAddPackage(
            NAME Catch2
            GITHUB_REPOSITORY catchorg/Catch2
            VERSION 3.13.0
        )
    endif()

    add_executable(nova_nonnull_tests tests/test_non_null.cpp)
    target_link_libraries(nova_nonnull_tests PRIVATE nova::nonnull Catch2::Catch2WithMain)

    enable_testing()
    add_test(NAME nova_nonnull_tests COMMAND nova_nonnull_tests)
endif()
