# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ccd_itf_pico C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

file(GLOB CCD_FILES "${CMAKE_CURRENT_LIST_DIR}/../epson_ccd_pio/*.c")

# Add executable. Default name is the project name, version 0.1

add_executable(ccd_itf_pico ccd_itf_pico.c ${CCD_FILES})

pico_set_program_name(ccd_itf_pico "ccd_itf_pico")
pico_set_program_version(ccd_itf_pico "0.1")

# Generate PIO header
pico_generate_pio_header(ccd_itf_pico "${CMAKE_CURRENT_LIST_DIR}/../epson_ccd_pio/pio_ccd.pio")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(ccd_itf_pico 0)
pico_enable_stdio_usb(ccd_itf_pico 1)

# Add the standard library to the build
target_link_libraries(ccd_itf_pico
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(ccd_itf_pico PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        "${CMAKE_CURRENT_LIST_DIR}/../epson_ccd_pio"
)

# Add any user requested libraries
target_link_libraries(ccd_itf_pico 
        hardware_pio
        )

pico_add_extra_outputs(ccd_itf_pico)

