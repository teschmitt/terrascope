cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(terrascope)

# Extract git commit hash for version info
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_RESULT
)
if(NOT GIT_RESULT EQUAL 0)
    set(GIT_COMMIT_HASH "unknown")
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/src/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)

file(GLOB_RECURSE app_sources
    src/*.cpp
    src/*.c
)
# Conditionally compiled sources are excluded from glob and added below
list(FILTER app_sources EXCLUDE REGEX "lora_mock\\.c")
list(FILTER app_sources EXCLUDE REGEX "sensor_bme280\\.c")
list(FILTER app_sources EXCLUDE REGEX "sensor_mock\\.c")

target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE
    ${CMAKE_BINARY_DIR}/generated
    src/
)

if(CONFIG_LORA_MOCK)
    target_sources(app PRIVATE src/drivers/lora_mock.c)
endif()

# Sensor backend: real BME280 when available in devicetree, mock otherwise
dt_comp_path(bme280_path COMPATIBLE "bosch,bme280")
if(DEFINED bme280_path)
    dt_node_has_status(bme280_okay PATH ${bme280_path} STATUS "okay")
else()
    set(bme280_okay FALSE)
endif()
if(bme280_okay)
    target_sources(app PRIVATE src/sensors/sensor_bme280.c)
else()
    target_sources(app PRIVATE src/sensors/sensor_mock.c)
endif()
