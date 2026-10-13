cmake_minimum_required(VERSION 3.20)
project(spire LANGUAGES CXX)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The host build requires clang for the device-C++ AST check")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
execute_process(
    COMMAND "${Python_EXECUTABLE}" -m pybind11 --cmakedir
    OUTPUT_VARIABLE pybind11_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
find_package(pybind11 CONFIG REQUIRED)

set(SPIRE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/spire_generated")
file(GLOB_RECURSE content_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.def")
set(content_headers
    "${SPIRE_GENERATED_DIR}/ids.hpp"
    "${SPIRE_GENERATED_DIR}/bases.hpp"
    "${SPIRE_GENERATED_DIR}/declarations.hpp"
    "${SPIRE_GENERATED_DIR}/content.hpp")
add_custom_command(OUTPUT ${content_headers}
    COMMAND "${Python_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/host/generate_content.py"
        --root "${CMAKE_CURRENT_SOURCE_DIR}" --output "${SPIRE_GENERATED_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/host/generate_content.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/inventory/units.json" ${content_sources}
    VERBATIM)
add_custom_target(generate_content DEPENDS ${content_headers})

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/spire_host_generated/snapshot.hpp"
    COMMAND "${Python_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/host/generate_snapshot.py"
        --root "${CMAKE_CURRENT_SOURCE_DIR}" --output "${CMAKE_CURRENT_BINARY_DIR}/spire_host_generated/snapshot.hpp"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/host/generate_snapshot.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/inventory/snapshot-fields.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/inventory/units.json" ${content_sources}
    VERBATIM)
add_custom_target(generate_snapshot DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/spire_host_generated/snapshot.hpp")

add_custom_target(check_device_core ALL
    COMMAND "${Python_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_device_core.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/core" --compiler "${CMAKE_CXX_COMPILER}"
        --generated "${SPIRE_GENERATED_DIR}"
    VERBATIM)
add_dependencies(check_device_core generate_content)

add_library(spire_core INTERFACE)
target_include_directories(spire_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(spire_core INTERFACE cxx_std_17)
target_compile_options(spire_core INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-ffp-contract=off>
    $<$<COMPILE_LANGUAGE:CUDA>:--fmad=false>)

pybind11_add_module(spire host/bindings.cpp)
target_link_libraries(spire PRIVATE spire_core)
add_dependencies(spire check_device_core)
add_dependencies(spire generate_snapshot)

add_executable(spire_benchmark host/benchmark.cpp)
target_link_libraries(spire_benchmark PRIVATE spire_core)
add_dependencies(spire_benchmark check_device_core)
add_custom_target(benchmark COMMAND spire_benchmark 100000 DEPENDS spire_benchmark USES_TERMINAL)

include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    add_library(spire_cuda STATIC cuda/batch.cu)
    target_link_libraries(spire_cuda PUBLIC spire_core)
    set_target_properties(spire_cuda PROPERTIES CUDA_STANDARD 17 CUDA_STANDARD_REQUIRED ON)
    add_dependencies(spire_cuda check_device_core)
endif()

include(CTest)
if(BUILD_TESTING)
    if(CMAKE_CUDA_COMPILER)
        add_executable(test_cuda_faults tests/test_cuda_faults.cu)
        target_link_libraries(test_cuda_faults PRIVATE spire_cuda)
        set_target_properties(test_cuda_faults PROPERTIES CUDA_STANDARD 17 CUDA_STANDARD_REQUIRED ON)
        add_test(NAME test_cuda_faults COMMAND test_cuda_faults)
        set_tests_properties(test_cuda_faults PROPERTIES SKIP_RETURN_CODE 77)
    endif()
    file(GLOB native_tests CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp")
    foreach(source IN LISTS native_tests)
        get_filename_component(test_name "${source}" NAME_WE)
        add_executable("${test_name}" "${source}")
        target_link_libraries("${test_name}" PRIVATE spire_core)
        add_dependencies("${test_name}" check_device_core)
        add_test(NAME "${test_name}" COMMAND "${test_name}")
    endforeach()
    add_test(NAME host_tests COMMAND "${Python_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests" -p "test_*.py")
    add_test(NAME combat_replay COMMAND "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay.py" "${CMAKE_CURRENT_SOURCE_DIR}/tests/traces")
    set_tests_properties(host_tests combat_replay PROPERTIES
        ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:spire>")
endif()
