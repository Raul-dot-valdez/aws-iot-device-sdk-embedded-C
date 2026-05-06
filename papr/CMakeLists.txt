cmake_minimum_required(VERSION 3.16)
project(papr_firmware C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(papr_core STATIC
    src/papr_blower.c
    src/papr_battery.c
    src/papr_sensors.c
    src/papr_alarms.c
    src/papr_l6235.c
    src/papr_controller.c
)
target_include_directories(papr_core PUBLIC include)
target_compile_options(papr_core PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    -Wstrict-prototypes -Wmissing-prototypes
    -ffunction-sections -fdata-sections
)

# Reference build using the host-side HAL stub. Cross-compile by setting
# CMAKE_TOOLCHAIN_FILE for the target MCU (e.g. arm-none-eabi-gcc) and
# providing a vendor-specific HAL implementation in place of papr_hal_stub.c.
add_executable(papr_firmware
    src/main.c
    hal/papr_hal_stub.c
)
target_link_libraries(papr_firmware PRIVATE papr_core)
