cmake_minimum_required(VERSION 3.28...4.4.2)
project(injector_core VERSION 0.3.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(yaml-cpp REQUIRED)

set (SRC
    src/explicit_instantiation.cpp
    src/rng.cpp
    src/scenario_parser.cpp
    src/window.cpp
    )

option(INJECTOR_CORE_SANITIZE
    "Build injector_core and its tests with AddressSanitizer and UBSan" OFF)

# Warnings, and sanitizers when INJECTOR_CORE_SANITIZE is ON, for the code this
# project builds: the library and its tests. They are PRIVATE so they never
# reach injector_node or the exported target. Optimisation is left to
# CMAKE_BUILD_TYPE (Debug: -g, Release: -O3 -DNDEBUG or /O2 /DNDEBUG).
function(injector_core_target_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES Clang) # Clang and AppleClang
        target_compile_options(${target} PRIVATE
            -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic
            -Wno-weak-vtables -Wno-c++20-extensions -Wno-padded
            # TODO remove and fix warnings
            -Wno-unsafe-buffer-usage
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wfloat-equal -Wmissing-declarations
        )
    endif()

    if(INJECTOR_CORE_SANITIZE)
        if(MSVC)
            target_compile_options(${target} PRIVATE /fsanitize=address)
            # MSVC AddressSanitizer requires incremental linking to be disabled
            target_link_options(${target} PRIVATE /INCREMENTAL:NO)
        else()
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

add_library(injector_core ${SRC} )

add_library(injector_core::injector_core ALIAS injector_core)
set_target_properties(injector_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

injector_core_target_options(injector_core)
if(INJECTOR_CORE_SANITIZE AND NOT MSVC)
    # An instrumented library needs the sanitizer runtime in whatever links it.
    target_link_options(injector_core INTERFACE -fsanitize=address,undefined)
endif()

target_compile_features(injector_core PUBLIC cxx_std_23) # Require C++23
target_include_directories(injector_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# keep yaml-cpp to this library only
target_link_libraries(injector_core PRIVATE yaml-cpp::yaml-cpp)

install(TARGETS injector_core EXPORT injector_coreTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/injector_core)

install(EXPORT injector_coreTargets
    FILE injector_coreTargets.cmake
    NAMESPACE injector_core::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/injector_core)

configure_package_config_file(cmake/injector_coreConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/injector_coreConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/injector_core)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/injector_coreConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/injector_coreConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/injector_coreConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/injector_core)

option(INJECTOR_CORE_BUILD_TESTS "Build unit tests" ON)
if(INJECTOR_CORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# To enable clangd to find the compile_commands.json file
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  file(CREATE_LINK
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
    SYMBOLIC
  )
endif()