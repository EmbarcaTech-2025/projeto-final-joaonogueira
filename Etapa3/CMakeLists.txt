# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Etapa3 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(Etapa3 
    app/main.c
    hal/aht10.c 
    hal/bh1750.c
)

pico_set_program_name(Etapa3 "Etapa3")
pico_set_program_version(Etapa3 "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Etapa3 0)
pico_enable_stdio_usb(Etapa3 1)

# Add the standard library to the build
target_link_libraries(Etapa3
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(Etapa3 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/include
)

# Add any user requested libraries
target_link_libraries(Etapa3 
        hardware_i2c
        pico_cyw43_arch_none
        )

pico_add_extra_outputs(Etapa3)

