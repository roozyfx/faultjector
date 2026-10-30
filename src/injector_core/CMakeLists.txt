cmake_minimum_required(VERSION 3.40...4.4.2)
project(injector_core VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(yaml-cpp REQUIRED)

add_library(injector_core src/scenario_parser.cpp
    include/fault_parameters.h)
add_library(injector_core::injector_core ALIAS injector_core)
set_target_properties(injector_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_features(injector_core PUBLIC cxx_std_23) # Require C++23
target_include_directories(injector_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# keep yaml-cpp to this library only
target_link_libraries(injector_core PRIVATE yaml-cpp::yaml-cpp)

install(TARGETS injector_core EXPORT injector_coreTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

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
