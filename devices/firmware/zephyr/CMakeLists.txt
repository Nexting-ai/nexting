cmake_minimum_required(VERSION 3.20.0)

if(BOARD MATCHES "^xiao_ble")
  set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/xiao_ble.overlay)
elseif(BOARD MATCHES "^xiao_esp32c3")
  set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/xiao_esp32c3.overlay)
elseif(BOARD MATCHES "^xiao_esp32s3")
  set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/xiao_esp32s3.overlay)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nexting_device_reference)

target_sources(app PRIVATE
  src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/c/src/nexting_device.c
)
target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/c/include
)
