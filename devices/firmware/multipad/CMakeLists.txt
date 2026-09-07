cmake_minimum_required(VERSION 3.20)
project(nexting_multipad_adapter LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(NEXTING_DEVICES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_library(nexting_multipad_adapter STATIC
  nexting_multipad_adapter.c
  "${NEXTING_DEVICES_ROOT}/sdk/c/src/nexting_device.c"
)
target_include_directories(nexting_multipad_adapter
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${NEXTING_DEVICES_ROOT}/sdk/c/include"
)
target_compile_options(nexting_multipad_adapter PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_executable(test_nexting_multipad tests/test_adapter.c)
target_link_libraries(test_nexting_multipad PRIVATE nexting_multipad_adapter)
target_compile_options(test_nexting_multipad PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME nexting_multipad_adapter COMMAND test_nexting_multipad)
