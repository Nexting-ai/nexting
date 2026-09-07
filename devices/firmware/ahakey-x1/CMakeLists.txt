cmake_minimum_required(VERSION 3.20)
project(nexting_ahakey_adapter LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(NEXTING_DEVICES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_library(nexting_ahakey_adapter STATIC
  ahakey_proto.c
  nexting_ahakey_adapter.c
  nexting_ahakey_audio.c
  developer-kit/nexting_ahakey_port.c
  "${NEXTING_DEVICES_ROOT}/sdk/c/src/nexting_device.c"
  "${NEXTING_DEVICES_ROOT}/sdk/c/src/nexting_device_audio.c"
)
target_include_directories(nexting_ahakey_adapter
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/developer-kit"
    "${NEXTING_DEVICES_ROOT}/sdk/c/include"
)
target_compile_options(nexting_ahakey_adapter PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_executable(test_ahakey_proto tests/test_ahakey_proto.c)
target_link_libraries(test_ahakey_proto PRIVATE nexting_ahakey_adapter)
target_compile_options(test_ahakey_proto PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ahakey_proto COMMAND test_ahakey_proto)

add_executable(test_nexting_ahakey tests/test_ahakey_adapter.c)
target_link_libraries(test_nexting_ahakey PRIVATE nexting_ahakey_adapter)
target_compile_options(test_nexting_ahakey PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME nexting_ahakey_adapter COMMAND test_nexting_ahakey)

add_executable(test_nexting_ahakey_audio tests/test_ahakey_audio.c)
target_link_libraries(test_nexting_ahakey_audio PRIVATE nexting_ahakey_adapter)
target_compile_options(test_nexting_ahakey_audio PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME nexting_ahakey_audio COMMAND test_nexting_ahakey_audio)

add_executable(test_nexting_ahakey_port tests/test_ahakey_port.c)
target_link_libraries(test_nexting_ahakey_port PRIVATE nexting_ahakey_adapter)
target_compile_options(test_nexting_ahakey_port PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME nexting_ahakey_port COMMAND test_nexting_ahakey_port)
