# =============================================================================
# CMake build for the BACnet B-LSC (Life Safety Controller) C++ example.
#
# This example links the CAS BACnet Stack through the CASBACnetStack::Adapter CMake
# target (submodules/cas-bacnet-stack/adapters/cpp) and calls the stack's C API
# (BACnetStack_*, via CASBACnetStackAdapter.h) directly. It builds and ships in the
# adapter's default SOURCE mode: the stack's source/*.cpp are compiled straight into
# the executable, so there is no library or DLL to build, ship, or install first and
# the build is the same two commands on every platform. (The adapter also offers
# STATIC and DLL modes - see its CMakeLists.txt - but they need a prebuilt library
# and this example does not use them.)
#
# Build:
#   cmake -B build -S .
#   cmake --build build --config Release --parallel
# Run (from the example folder):
#   ./build/BACnetExampleBLSC          (Linux/macOS)
#   .\build\Release\BACnetExampleBLSC.exe   (Windows)
# =============================================================================

cmake_minimum_required(VERSION 3.15)
project(BACnetProfileExample_CPP_B_LSC CXX C)


# Default to a Release build.
#
# Without this, a single-config generator (Unix Makefiles / Ninja - the default
# on Linux and macOS) leaves CMAKE_BUILD_TYPE EMPTY, and the documented
# `cmake --build build --config Release` is SILENTLY IGNORED there: --config
# only means anything to a multi-config generator like Visual Studio. The result
# is that the whole stack (~600 files) compiles at -O0 and the customer never
# finds out. Windows is unaffected, which is exactly why this survived so long.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Locate the pieces this project ships -----------------------------------
# The shared helper is vendored into this repository at ./common.
if(NOT DEFINED EXAMPLES_COMMON_DIR)
    set(EXAMPLES_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common")
endif()

# The CAS BACnet Stack is a git submodule of this repository, at
# ./submodules/cas-bacnet-stack. Run `git submodule update --init --recursive`
# after cloning. (Override with -D CAS_STACK_DIR=... to use a stack elsewhere.)
if(NOT DEFINED CAS_STACK_DIR)
    set(CAS_STACK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/submodules/cas-bacnet-stack")
endif()
if(NOT EXISTS "${CAS_STACK_DIR}/adapters/cpp/CMakeLists.txt")
    message(FATAL_ERROR
        "CAS BACnet Stack adapter not found under: ${CAS_STACK_DIR}\n"
        "The stack is a git submodule. After cloning this repository, run:\n"
        "    git submodule update --init --recursive\n"
        "Or pass -D CAS_STACK_DIR=/path/to/cas-bacnet-stack to use a stack elsewhere.")
endif()

# --- The CAS BACnet Stack, via its CMake adapter target ---------------------
# CAS_BACNET_STACK_LINK is the adapter's own cache variable (SOURCE/STATIC/DLL,
# default SOURCE, which is what this example builds with). To override it, set it
# BEFORE add_subdirectory() so the adapter target picks it up. See
# submodules/cas-bacnet-stack/adapters/cpp/CMakeLists.txt for what each mode does.
add_subdirectory("${CAS_STACK_DIR}/adapters/cpp" cas-bacnet-stack-adapter)

# --- LifeSafetyOperation (AE-LS-B) ------------------------------------------
# AE-LS-B executes LifeSafetyOperation (135-2024 K.2.13). The stack compiles that
# service only when STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION is defined - its
# documented public toggle (CASBACnetStackOptions.h: "Customer firmware opts in by
# defining STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION explicitly"); the default
# STACK_OPTION_TARGET_FULL preset leaves it off. In SOURCE mode the stack's .cpp
# files are compiled here, so define it on the adapter target. In STATIC / DLL
# mode the prebuilt library must have been built with it, e.g. on Windows:
#     CL=-DSTACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION tools/build-stack-static.sh <this repo>
# Without it main() stops at start-up ("Failed to enable the LifeSafetyOperation
# service") instead of advertising a service it cannot execute.
if(CAS_BACNET_STACK_LINK STREQUAL "SOURCE")
    target_compile_definitions(cas_bacnet_stack_adapter PRIVATE STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION)
else()
    message(STATUS "B-LSC: CAS_BACNET_STACK_LINK=${CAS_BACNET_STACK_LINK} - the prebuilt stack library "
                   "must be built with STACK_OPTION_DM_LSO_LIFE_SAFETY_OPERATION, or the example exits at start-up")
endif()

# --- The shared example helper ----------------------------------------------
set(COMMON_SOURCES
    "${EXAMPLES_COMMON_DIR}/CASExampleHelper.cpp"
    "${EXAMPLES_COMMON_DIR}/SimpleUDP.cpp")

# --- SIMULATION (not part of a real product) ---------------------------------
# simulation/SimulationControl.cpp is the Wire Tests' loopback-only back door to
# the simulated smoke detector (main.cpp section 1b). It is off unless
# BLSC_SIMULATION_CONTROL_PORT is set. A real product deletes it.
set(SIMULATION_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/simulation/SimulationControl.cpp")

# --- The executable ---------------------------------------------------------
add_executable(BACnetExampleBLSC
    main.cpp
    ${SIMULATION_SOURCES}
    ${COMMON_SOURCES})

target_include_directories(BACnetExampleBLSC PRIVATE
    "${EXAMPLES_COMMON_DIR}")

target_link_libraries(BACnetExampleBLSC PRIVATE CASBACnetStack::Adapter)

# --- Warnings on OUR sources only -------------------------------------------
# Apply strict warnings to the example + helper sources, but NOT to the bundled
# third-party stack (~600 files, built inside the CASBACnetStack::Adapter target)
# which would be noisy and isn't ours to fix.
if(MSVC)
    set(EXAMPLE_WARNINGS "/W4")
else()
    set(EXAMPLE_WARNINGS "-Wall;-Wextra")
endif()
set_source_files_properties(main.cpp ${SIMULATION_SOURCES} ${COMMON_SOURCES}
    PROPERTIES COMPILE_OPTIONS "${EXAMPLE_WARNINGS}")

# --- Platform sockets -------------------------------------------------------
# ws2_32  = Winsock (UDP); iphlpapi = GetAdaptersInfo (local broadcast address).
if(WIN32)
    target_link_libraries(BACnetExampleBLSC PRIVATE ws2_32 iphlpapi)
endif()
