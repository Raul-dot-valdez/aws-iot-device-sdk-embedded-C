# Cross-compile toolchain for the PAPR GD32 ports.
#
# Use with:
#   cmake -S papr -B build \
#         -DCMAKE_TOOLCHAIN_FILE=papr/cmake/arm-none-eabi.cmake \
#         -DPAPR_TARGET=GD32E517RE -DGD32E51X_SDK_DIR=...
#
# Assumes arm-none-eabi-gcc is on PATH (apt: gcc-arm-none-eabi).

set(CMAKE_SYSTEM_NAME       Generic)
set(CMAKE_SYSTEM_PROCESSOR  arm)

find_program(CMAKE_C_COMPILER   arm-none-eabi-gcc)
find_program(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
find_program(CMAKE_AR           arm-none-eabi-ar)
find_program(CMAKE_OBJCOPY      arm-none-eabi-objcopy)
find_program(CMAKE_OBJDUMP      arm-none-eabi-objdump)
find_program(CMAKE_SIZE         arm-none-eabi-size)

if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR
        "arm-none-eabi-gcc not found on PATH. Install gcc-arm-none-eabi "
        "(Ubuntu: apt install gcc-arm-none-eabi) or extend PATH.")
endif()

# A test-link with --specs=nosys.specs would need to know which slot we're
# linking; skip the test by building a static library during compiler probe.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
