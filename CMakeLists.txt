
cmake_minimum_required(VERSION 3.20)

project(servo_manager C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(MCU_FLAGS
    -mcpu=cortex-m3
    -mthumb
)

set(LINKER_SCRIPT
    "${CMAKE_SOURCE_DIR}/STM32F103C8TX_FLASH.ld"
)

set(PROJECT_SOURCES
    Startup/startup_stm32f103c8tx.s
    Sources/main.c
    Sources/syscalls.c
    Sources/sysmem.c
    Sources/system_stm32f1xx.c
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    SUFFIX ".elf"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    STM32F103xB
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/STM32CubeF1/Drivers/CMSIS/Include"
    "${CMAKE_SOURCE_DIR}/STM32CubeF1/Drivers/CMSIS/Device/ST/STM32F1xx/Include"
)

target_compile_options(${PROJECT_NAME} PRIVATE
    ${MCU_FLAGS}
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
    -Os
)

target_link_options(${PROJECT_NAME} PRIVATE
    ${MCU_FLAGS}
    --specs=nano.specs
    --specs=nosys.specs
    "-T${LINKER_SCRIPT}"
    -Wl,-Map=${PROJECT_NAME}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    c
    m
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${PROJECT_NAME}>
            "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex"
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}>
            "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin"
)
