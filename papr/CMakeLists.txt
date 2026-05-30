cmake_minimum_required(VERSION 3.16)
project(papr_firmware C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ----------------------------------------------------------------------------
# Target selection
#
#   -DPAPR_TARGET=HOST          (default) build with the simulation stub
#   -DPAPR_TARGET=GD32E517RE    GD32E517RE port (LQFP64, 512 KB)
#                               requires -DGD32E51X_SDK_DIR
#   -DPAPR_TARGET=GD32E503CE    GD32E503CE port (LQFP48, 256 KB)
#                               requires -DGD32E50X_SDK_DIR
#   Both MCU targets need -DCMAKE_TOOLCHAIN_FILE pointing at arm-none-eabi-gcc.
#
# Dual-bank / secure-boot options (MCU targets):
#   -DPAPR_APP_SLOT=A|B         link the application for flash slot A or B
#                               (single-image dev build when unset)
#   -DPAPR_BUILD_BOOTLOADER=ON  also build the standalone reference bootloader
# ----------------------------------------------------------------------------
set(PAPR_TARGET "HOST" CACHE STRING "Target platform")
set_property(CACHE PAPR_TARGET PROPERTY STRINGS HOST GD32E517RE GD32E503CE)
set(PAPR_APP_SLOT "" CACHE STRING "Application flash slot: A, B, or empty")
set_property(CACHE PAPR_APP_SLOT PROPERTY STRINGS "" A B)
option(PAPR_BUILD_BOOTLOADER "Build the standalone reference bootloader" OFF)

add_library(papr_core STATIC
    src/papr_blower.c
    src/papr_battery.c
    src/papr_sensors.c
    src/papr_alarms.c
    src/papr_l6235.c
    src/papr_sdp810.c
    src/papr_gdy1124.c
    src/papr_ble.c
    src/papr_keypad.c
    src/papr_energy.c
    src/papr_ota.c
    src/papr_provision.c
    src/papr_factory.c
    src/papr_sha256.c
    src/papr_secure.c
    src/papr_controller.c
)
target_include_directories(papr_core PUBLIC include)
target_compile_options(papr_core PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    -Wstrict-prototypes -Wmissing-prototypes
    -ffunction-sections -fdata-sections
)

if(PAPR_TARGET STREQUAL "HOST")
    add_executable(papr_firmware
        src/main.c
        hal/papr_hal_stub.c
    )
    target_link_libraries(papr_firmware PRIVATE papr_core)

elseif(PAPR_TARGET STREQUAL "GD32E517RE")
    if(NOT DEFINED GD32E51X_SDK_DIR)
        message(FATAL_ERROR
            "PAPR_TARGET=GD32E517RE requires -DGD32E51X_SDK_DIR pointing at "
            "the GigaDevice firmware library (containing CMSIS/, "
            "GD32E51x_standard_peripheral/, and a startup_gd32e51x.s).")
    endif()

    add_executable(papr_firmware
        src/main.c
        hal/gd32e517re/papr_hal_gd32e517re.c
        ${GD32E51X_SDK_DIR}/CMSIS/GD/GD32E51x/Source/system_gd32e51x.c
        ${GD32E51X_SDK_DIR}/CMSIS/GD/GD32E51x/Source/GCC/startup_gd32e51x_xd.s
    )
    target_compile_definitions(papr_firmware PRIVATE
        GD32E51X
        GD32E517
        HXTAL_VALUE=25000000
    )
    target_include_directories(papr_firmware PRIVATE
        hal/gd32e517re
        ${GD32E51X_SDK_DIR}/CMSIS
        ${GD32E51X_SDK_DIR}/CMSIS/GD/GD32E51x/Include
        ${GD32E51X_SDK_DIR}/GD32E51x_standard_peripheral/Include
    )
    file(GLOB GD32_PERIPH_SRC
        ${GD32E51X_SDK_DIR}/GD32E51x_standard_peripheral/Source/*.c)
    target_sources(papr_firmware PRIVATE ${GD32_PERIPH_SRC})

    # Pick the linker script: a specific slot for dual-bank/secure-boot, or
    # the single-image dev layout when PAPR_APP_SLOT is unset.
    if(PAPR_APP_SLOT STREQUAL "A")
        set(PAPR_APP_LD ${CMAKE_CURRENT_SOURCE_DIR}/linker/app_slot_a.ld)
    elseif(PAPR_APP_SLOT STREQUAL "B")
        set(PAPR_APP_LD ${CMAKE_CURRENT_SOURCE_DIR}/linker/app_slot_b.ld)
    else()
        set(PAPR_APP_LD ${CMAKE_CURRENT_SOURCE_DIR}/hal/gd32e517re/gd32e517re.ld)
    endif()

    target_compile_options(papr_firmware PRIVATE
        -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
        -ffunction-sections -fdata-sections -Os)
    target_link_options(papr_firmware PRIVATE
        -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
        -Wl,--gc-sections
        -L${CMAKE_CURRENT_SOURCE_DIR}/linker
        -Wl,-T,${PAPR_APP_LD}
        --specs=nano.specs --specs=nosys.specs)

    target_link_libraries(papr_firmware PRIVATE papr_core)

    # ---- standalone reference bootloader -----------------------------------
    if(PAPR_BUILD_BOOTLOADER)
        add_executable(papr_bootloader
            boot/papr_boot.c
            src/papr_sha256.c
            ${GD32E51X_SDK_DIR}/CMSIS/GD/GD32E51x/Source/system_gd32e51x.c
            ${GD32E51X_SDK_DIR}/CMSIS/GD/GD32E51x/Source/GCC/startup_gd32e51x_xd.s
        )
        target_compile_definitions(papr_bootloader PRIVATE
            GD32E51X GD32E517 HXTAL_VALUE=25000000)
        target_include_directories(papr_bootloader PRIVATE
            include boot
            ${GD32E51X_SDK_DIR}/CMSIS
            ${GD32E51X_SDK_DIR}/CMSIS/GD/GD32E51x/Include
            ${GD32E51X_SDK_DIR}/GD32E51x_standard_peripheral/Include)
        # The bootloader needs only the FMC peripheral driver beyond CMSIS.
        target_sources(papr_bootloader PRIVATE
            ${GD32E51X_SDK_DIR}/GD32E51x_standard_peripheral/Source/gd32e51x_fmc.c)
        target_compile_options(papr_bootloader PRIVATE
            -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
            -ffunction-sections -fdata-sections -Os)
        target_link_options(papr_bootloader PRIVATE
            -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
            -Wl,--gc-sections
            -L${CMAKE_CURRENT_SOURCE_DIR}/linker
            -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/linker/bootloader.ld
            --specs=nano.specs --specs=nosys.specs)
    endif()

elseif(PAPR_TARGET STREQUAL "GD32E503CE")
    # Alternative MCU port — smaller LQFP48 / 256 KB sibling. The application
    # code is identical (papr_core); only the HAL, pinmap, and flash map differ.
    if(NOT DEFINED GD32E50X_SDK_DIR)
        message(FATAL_ERROR
            "PAPR_TARGET=GD32E503CE requires -DGD32E50X_SDK_DIR pointing at "
            "the GigaDevice GD32E50x firmware library (containing CMSIS/, "
            "GD32E50x_standard_peripheral/, and a startup_gd32e503_xb.s).")
    endif()

    add_executable(papr_firmware
        src/main.c
        hal/gd32e503ce/papr_hal_gd32e503ce.c
        ${GD32E50X_SDK_DIR}/CMSIS/GD/GD32E50x/Source/system_gd32e50x.c
        ${GD32E50X_SDK_DIR}/CMSIS/GD/GD32E50x/Source/GCC/startup_gd32e503_xb.s
    )
    target_compile_definitions(papr_firmware PRIVATE
        GD32E50X
        GD32E503
        HXTAL_VALUE=25000000
    )
    target_include_directories(papr_firmware PRIVATE
        hal/gd32e503ce
        ${GD32E50X_SDK_DIR}/CMSIS
        ${GD32E50X_SDK_DIR}/CMSIS/GD/GD32E50x/Include
        ${GD32E50X_SDK_DIR}/GD32E50x_standard_peripheral/Include
    )
    file(GLOB GD32E50X_PERIPH_SRC
        ${GD32E50X_SDK_DIR}/GD32E50x_standard_peripheral/Source/*.c)
    target_sources(papr_firmware PRIVATE ${GD32E50X_PERIPH_SRC})

    if(PAPR_APP_SLOT STREQUAL "A")
        set(PAPR_APP_LD ${CMAKE_CURRENT_SOURCE_DIR}/hal/gd32e503ce/app_slot_a.ld)
    elseif(PAPR_APP_SLOT STREQUAL "B")
        set(PAPR_APP_LD ${CMAKE_CURRENT_SOURCE_DIR}/hal/gd32e503ce/app_slot_b.ld)
    else()
        set(PAPR_APP_LD ${CMAKE_CURRENT_SOURCE_DIR}/hal/gd32e503ce/gd32e503ce.ld)
    endif()

    target_compile_options(papr_firmware PRIVATE
        -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
        -ffunction-sections -fdata-sections -Os)
    target_link_options(papr_firmware PRIVATE
        -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
        -Wl,--gc-sections
        -L${CMAKE_CURRENT_SOURCE_DIR}/linker
        -Wl,-T,${PAPR_APP_LD}
        --specs=nano.specs --specs=nosys.specs)

    target_link_libraries(papr_firmware PRIVATE papr_core)

    if(PAPR_BUILD_BOOTLOADER)
        add_executable(papr_bootloader
            boot/papr_boot.c
            src/papr_sha256.c
            ${GD32E50X_SDK_DIR}/CMSIS/GD/GD32E50x/Source/system_gd32e50x.c
            ${GD32E50X_SDK_DIR}/CMSIS/GD/GD32E50x/Source/GCC/startup_gd32e503_xb.s
        )
        # The GD32E503CE pin-map overrides PAPR_SLOT_* — propagate the same
        # geometry into the bootloader so both sides agree on the flash layout.
        target_compile_definitions(papr_bootloader PRIVATE
            GD32E50X GD32E503 HXTAL_VALUE=25000000
            PAPR_FLASH_PAGE=1024U
            PAPR_SLOT_SIZE=114688U          # 112 KB
            PAPR_SLOT_B_ADDR=0x08024000U)
        target_include_directories(papr_bootloader PRIVATE
            include boot
            ${GD32E50X_SDK_DIR}/CMSIS
            ${GD32E50X_SDK_DIR}/CMSIS/GD/GD32E50x/Include
            ${GD32E50X_SDK_DIR}/GD32E50x_standard_peripheral/Include)
        target_sources(papr_bootloader PRIVATE
            ${GD32E50X_SDK_DIR}/GD32E50x_standard_peripheral/Source/gd32e50x_fmc.c)
        target_compile_options(papr_bootloader PRIVATE
            -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
            -ffunction-sections -fdata-sections -Os)
        target_link_options(papr_bootloader PRIVATE
            -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
            -Wl,--gc-sections
            -L${CMAKE_CURRENT_SOURCE_DIR}/linker
            -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/hal/gd32e503ce/bootloader.ld
            --specs=nano.specs --specs=nosys.specs)
    endif()

else()
    message(FATAL_ERROR "Unknown PAPR_TARGET=${PAPR_TARGET}")
endif()
