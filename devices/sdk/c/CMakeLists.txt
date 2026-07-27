cmake_minimum_required(VERSION 3.20)
project(nexting_device_c VERSION 0.2.0 LANGUAGES C)

option(NEXTING_DEVICE_SANITIZE "Enable address and undefined behavior sanitizers" OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_program(NEXTING_DEVICE_NODE_EXECUTABLE node REQUIRED)
set(NEXTING_DEVICE_VECTOR_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../protocol/vectors/approval-v1.json")
set(NEXTING_DEVICE_VECTOR_HEADER
    "${CMAKE_CURRENT_BINARY_DIR}/generated_vectors.h")
execute_process(
  COMMAND "${NEXTING_DEVICE_NODE_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_vectors.mjs"
          "${NEXTING_DEVICE_VECTOR_SOURCE}"
          "${NEXTING_DEVICE_VECTOR_HEADER}"
  RESULT_VARIABLE NEXTING_DEVICE_VECTOR_RESULT
)
if(NOT NEXTING_DEVICE_VECTOR_RESULT EQUAL 0)
  message(FATAL_ERROR "Could not generate C protocol vectors")
endif()

set(NEXTING_DEVICE_STATUS_VECTOR_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../protocol/vectors/status-v1.json")
set(NEXTING_DEVICE_STATUS_VECTOR_HEADER
    "${CMAKE_CURRENT_BINARY_DIR}/generated_status_vectors.h")
execute_process(
  COMMAND "${NEXTING_DEVICE_NODE_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_vectors.mjs"
          "${NEXTING_DEVICE_STATUS_VECTOR_SOURCE}"
          "${NEXTING_DEVICE_STATUS_VECTOR_HEADER}"
  RESULT_VARIABLE NEXTING_DEVICE_STATUS_VECTOR_RESULT
)
if(NOT NEXTING_DEVICE_STATUS_VECTOR_RESULT EQUAL 0)
  message(FATAL_ERROR "Could not generate C status vectors")
endif()

set(NEXTING_DEVICE_INFO_VECTOR_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../protocol/vectors/device-info-v1.json")
set(NEXTING_DEVICE_INFO_VECTOR_HEADER
    "${CMAKE_CURRENT_BINARY_DIR}/generated_device_info_vectors.h")
execute_process(
  COMMAND "${NEXTING_DEVICE_NODE_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_vectors.mjs"
          "${NEXTING_DEVICE_INFO_VECTOR_SOURCE}"
          "${NEXTING_DEVICE_INFO_VECTOR_HEADER}"
  RESULT_VARIABLE NEXTING_DEVICE_INFO_VECTOR_RESULT
)
if(NOT NEXTING_DEVICE_INFO_VECTOR_RESULT EQUAL 0)
  message(FATAL_ERROR "Could not generate C Device Info vectors")
endif()

add_library(nexting_device STATIC src/nexting_device.c)
target_include_directories(nexting_device PUBLIC include)
target_compile_options(nexting_device PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(NEXTING_DEVICE_SANITIZE AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(nexting_device PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(nexting_device PUBLIC -fsanitize=address,undefined)
endif()

enable_testing()
foreach(test_name IN ITEMS codec stream relay status device_info)
  add_executable(test_${test_name} tests/test_${test_name}.c)
  target_include_directories(test_${test_name} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_libraries(test_${test_name} PRIVATE nexting_device)
  target_compile_options(test_${test_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME ${test_name} COMMAND test_${test_name})
endforeach()

if(NEXTING_DEVICE_SANITIZE AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_definitions(test_codec PRIVATE NEXTING_DEVICE_ASAN_TEST=1)
endif()
