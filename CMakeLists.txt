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
        --extra-root "${CMAKE_CURRENT_SOURCE_DIR}/obs"
        --generated "${SPIRE_GENERATED_DIR}"
    VERBATIM)
add_dependencies(check_device_core generate_content)

add_custom_target(check_game_layout ALL
    COMMAND "${Python_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/host/check_layout.py"
        --compiler "${CMAKE_CXX_COMPILER}" --root "${CMAKE_CURRENT_SOURCE_DIR}"
        --generated "${SPIRE_GENERATED_DIR}"
    VERBATIM)
add_dependencies(check_game_layout generate_content)

add_library(spire_core INTERFACE)
add_dependencies(spire_core check_game_layout)
target_include_directories(spire_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(spire_core INTERFACE cxx_std_17)
target_compile_options(spire_core INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-ffp-contract=off>
    $<$<COMPILE_LANGUAGE:CUDA>:-ffp-contract=off>)

pybind11_add_module(spire host/bindings.cpp)
target_link_libraries(spire PRIVATE spire_core)
add_dependencies(spire check_device_core)
add_dependencies(spire generate_snapshot)

add_executable(spire_benchmark host/benchmark.cpp)
target_link_libraries(spire_benchmark PRIVATE spire_core)
add_dependencies(spire_benchmark check_device_core)
add_custom_target(benchmark COMMAND spire_benchmark 100000 DEPENDS spire_benchmark USES_TERMINAL)

add_executable(spire_deep_benchmark host/deep_benchmark.cpp)
target_link_libraries(spire_deep_benchmark PRIVATE spire_core)
target_compile_definitions(spire_deep_benchmark PRIVATE
    SPIRE_DEEP_ACTIONS="${CMAKE_CURRENT_SOURCE_DIR}/tests/deep_benchmark.actions")
add_dependencies(spire_deep_benchmark check_device_core)

add_executable(spire_collect_roots tools/collect_roots.cpp)
target_link_libraries(spire_collect_roots PRIVATE spire_core)
add_dependencies(spire_collect_roots check_device_core)

include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    if(NOT CMAKE_CUDA_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "CUDA must be compiled by clang++, the compiler checked by check_device_core")
    endif()
    find_package(CUDAToolkit REQUIRED)
    add_library(spire_cuda STATIC cuda/batch.cu cuda/training_batch.cu)
    target_link_libraries(spire_cuda PUBLIC spire_core CUDA::cudart)
    set_target_properties(spire_cuda PROPERTIES CUDA_STANDARD 17 CUDA_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON)
    add_dependencies(spire_cuda check_device_core)
    pybind11_add_module(spire_gpu cuda/bindings.cpp)
    target_link_libraries(spire_gpu PRIVATE spire_cuda)
endif()

include(CTest)
if(BUILD_TESTING)
    pybind11_add_module(training_state tests/training_state.cpp)
    target_link_libraries(training_state PRIVATE spire_core)
    add_dependencies(training_state check_device_core generate_snapshot)
    add_test(NAME deep_benchmark_inputs COMMAND spire_deep_benchmark 1)
    if(CMAKE_CUDA_COMPILER)
        add_executable(test_cuda_faults tests/test_cuda_faults.cu)
        target_link_libraries(test_cuda_faults PRIVATE spire_cuda)
        set_target_properties(test_cuda_faults PROPERTIES CUDA_STANDARD 17 CUDA_STANDARD_REQUIRED ON)
        add_test(NAME test_cuda_faults COMMAND test_cuda_faults)
        set_tests_properties(test_cuda_faults PROPERTIES SKIP_RETURN_CODE 77)
        add_executable(test_cuda_parity tests/test_cuda_parity.cu)
        target_link_libraries(test_cuda_parity PRIVATE spire_cuda)
        target_compile_definitions(test_cuda_parity PRIVATE
            SPIRE_DEEP_ACTIONS="${CMAKE_CURRENT_SOURCE_DIR}/tests/deep_benchmark.actions")
        set_target_properties(test_cuda_parity PROPERTIES CUDA_STANDARD 17 CUDA_STANDARD_REQUIRED ON)
        add_test(NAME test_cuda_parity COMMAND test_cuda_parity)
        add_test(NAME cuda_python COMMAND "${Python_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_cuda_python.py")
        set_tests_properties(cuda_python PROPERTIES ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:spire>:${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    file(GLOB native_tests CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp")
    foreach(source IN LISTS native_tests)
        get_filename_component(test_name "${source}" NAME_WE)
        add_executable("${test_name}" "${source}")
        target_link_libraries("${test_name}" PRIVATE spire_core)
        target_compile_definitions("${test_name}" PRIVATE
            SPIRE_DEEP_ACTIONS="${CMAKE_CURRENT_SOURCE_DIR}/tests/deep_benchmark.actions")
        add_dependencies("${test_name}" check_device_core)
        add_test(NAME "${test_name}" COMMAND "${test_name}")
    endforeach()
    add_test(NAME host_tests COMMAND "${Python_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests" -p "test_*.py")
    add_test(NAME oracle_fixtures COMMAND "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_oracle.py")
    add_test(NAME run_coverage COMMAND "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_run_coverage.py" "${CMAKE_CURRENT_SOURCE_DIR}/tests/traces")
    add_test(NAME combat_replay COMMAND "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay.py" "${CMAKE_CURRENT_SOURCE_DIR}/tests/traces")
    set_tests_properties(host_tests combat_replay PROPERTIES
        ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:spire>:${CMAKE_CURRENT_SOURCE_DIR};OMP_NUM_THREADS=1;OPENBLAS_NUM_THREADS=1;VECLIB_MAXIMUM_THREADS=1")
endif()
