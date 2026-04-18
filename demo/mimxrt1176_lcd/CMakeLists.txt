CMAKE_MINIMUM_REQUIRED (VERSION 3.10.0)
project(mimxrt1176_lcd)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

string(CONCAT ARCH_FLAGS
    "-mcpu=cortex-m7 "
    "-mfpu=fpv5-d16 "
    "-mfloat-abi=hard "
    "-mthumb "
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARCH_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARCH_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${ARCH_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ARCH_FLAGS}")

add_compile_definitions(
    CPU_MIMXRT1176DVMAA
    CPU_MIMXRT1176DVMAA_cm7
    XIP_BOOT_HEADER_DCD_ENABLE=1
    USE_SDRAM
    DATA_SECTION_IS_CACHEABLE=1
    XIP_EXTERNAL_FLASH=1
    XIP_BOOT_HEADER_ENABLE=1
    SDK_OS_BAREMETAL
    SDK_DEBUGCONSOLE=0
    __MCUXPRESSO
    __USE_CMSIS
    SERIAL_PORT_TYPE_UART=1
    __NEWLIB__
)

add_compile_options(
    # -fmacro-prefix-map="$(<D)"=
    -g3
)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linkscript/mimxrt1176_lcd.ld)

add_link_options(
    -nostdlib
    LINKER:--sort-section=alignment
    LINKER:--cref
    -T ${LINKER_SCRIPT}
)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_ASM_FLAGS_DEBUG "${CMAKE_ASM_FLAGS_DEBUG} -O0")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os")
set(CMAKE_ASM_FLAGS_RELEASE "${CMAKE_ASM_FLAGS_RELEASE} -Os")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/xip
    ${CMAKE_CURRENT_SOURCE_DIR}/device
    ${CMAKE_CURRENT_SOURCE_DIR}/CMSIS
    ${CMAKE_CURRENT_SOURCE_DIR}/component/uart
    ${CMAKE_CURRENT_SOURCE_DIR}/utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/board
    ${CMAKE_CURRENT_SOURCE_DIR}/component/lists
    ${CMAKE_CURRENT_SOURCE_DIR}/startup
    ${CMAKE_CURRENT_SOURCE_DIR}/video
    ${CMAKE_CURRENT_SOURCE_DIR}/component/serial_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

add_subdirectory(../../ blib3d)

add_subdirectory(../ demo)

add_executable(mimxrt1176_lcd)

file(GLOB mimxrt1176_lcd_xip
     "xip/*.S"
     "xip/*.h"
     "xip/*.c"
     "xip/*.hpp"
     "xip/*.cpp"
)

file(GLOB mimxrt1176_lcd_video
     "video/*.S"
     "video/*.h"
     "video/*.c"
     "video/*.hpp"
     "video/*.cpp"
)

file(GLOB mimxrt1176_lcd_utilities
     "utilities/*.S"
     "utilities/*.h"
     "utilities/*.c"
     "utilities/*.hpp"
     "utilities/*.cpp"
)

file(GLOB mimxrt1176_lcd_startup
     "startup/*.S"
     "startup/*.h"
     "startup/*.c"
     "startup/*.hpp"
     "startup/*.cpp"
)

file(GLOB mimxrt1176_lcd_drivers
     "drivers/*.S"
     "drivers/*.h"
     "drivers/*.c"
     "drivers/*.hpp"
     "drivers/*.cpp"
)

file(GLOB mimxrt1176_lcd_device
     "device/*.S"
     "device/*.h"
     "device/*.c"
     "device/*.hpp"
     "device/*.cpp"
)

file(GLOB mimxrt1176_lcd_component_uart
     "component/uart/*.S"
     "component/uart/*.h"
     "component/uart/*.c"
     "component/uart/*.hpp"
     "component/uart/*.cpp"
)

file(GLOB mimxrt1176_lcd_component_serial_manager
     "component/serial_manager/*.S"
     "component/serial_manager/*.h"
     "component/serial_manager/*.c"
     "component/serial_manager/*.hpp"
     "component/serial_manager/*.cpp"
)

file(GLOB mimxrt1176_lcd_component_lists
     "component/lists/*.S"
     "component/lists/*.h"
     "component/lists/*.c"
     "component/lists/*.hpp"
     "component/lists/*.cpp"
)

file(GLOB mimxrt1176_lcd_board
     "board/board.*"
     "board/clock_config.*"
     "board/dcd.*"
     "board/elcdif_support.*"
     "board/peripherals.*"
     "board/pin_mux.*"
     "board/xmcd.*"
)

file(GLOB mimxrt1176_lcd_source
     "source/*.S"
     "source/*.h"
     "source/*.c"
     "source/*.hpp"
     "source/*.cpp"
)

target_sources(mimxrt1176_lcd PRIVATE
    ${mimxrt1176_lcd_xip}
    ${mimxrt1176_lcd_video}
    ${mimxrt1176_lcd_utilities}
    ${mimxrt1176_lcd_startup}
    ${mimxrt1176_lcd_drivers}
    ${mimxrt1176_lcd_device}
    ${mimxrt1176_lcd_component_uart}
    ${mimxrt1176_lcd_component_serial_manager}
    ${mimxrt1176_lcd_component_lists}
    ${mimxrt1176_lcd_board}
    ${mimxrt1176_lcd_source}
)

# target_include_directories(mimxrt1176_lcd
#     PRIVATE
# )

target_link_libraries(mimxrt1176_lcd PRIVATE
    blib3d
    demo
)

# target_compile_definitions(mimxrt1176_lcd
#     PRIVATE
# )

set_target_properties(mimxrt1176_lcd PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})
