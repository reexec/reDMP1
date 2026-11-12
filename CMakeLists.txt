cmake_minimum_required(VERSION 3.25)

# Read project version from the VERSION file
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" REDMP1_VERSION_STRING LIMIT_COUNT 1)
string(STRIP "${REDMP1_VERSION_STRING}" REDMP1_VERSION_STRING)

project(redmp1
    VERSION ${REDMP1_VERSION_STRING}
    DESCRIPTION "Native port of DJMAX Portable"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. repsp integration (shared runtime)
set(REDMP1_REPSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/repsp" CACHE PATH "Path to repsp directory")
set(REPSP_BUILD_PROBES ON CACHE BOOL "Build repsp verification probes")

add_subdirectory("${REDMP1_REPSP_DIR}" repsp)

# 2. Host tool: allegrexrecomp (patched in binary dir, leaving submodule untouched)
set(ALLEGREXRECOMP_ORIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/psprecomp/tools/allegrexrecomp")
set(ALLEGREXRECOMP_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/tools/allegrexrecomp")

file(REMOVE_RECURSE "${ALLEGREXRECOMP_BUILD_DIR}")
file(COPY "${ALLEGREXRECOMP_ORIG_DIR}/" DESTINATION "${ALLEGREXRECOMP_BUILD_DIR}")

find_package(Git REQUIRED)
set(ALLEGREXRECOMP_PATCHES
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/psprecomp/0001-relocate-prx-at-base.patch"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/psprecomp/0002-seed-harvest-segment-bases.patch"
)
file(RELATIVE_PATH ALLEGREXRECOMP_REL_DIR "${CMAKE_CURRENT_SOURCE_DIR}" "${ALLEGREXRECOMP_BUILD_DIR}")

foreach(patch IN LISTS ALLEGREXRECOMP_PATCHES)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --directory=${ALLEGREXRECOMP_REL_DIR} -p3 --whitespace=nowarn "${patch}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE patch_res
        OUTPUT_VARIABLE patch_out
        ERROR_VARIABLE patch_err
    )
    if(NOT patch_res EQUAL 0)
        message(FATAL_ERROR "Failed to apply patch ${patch}:\n${patch_err}\n${patch_out}")
    endif()
endforeach()

file(GLOB ALLEGREXRECOMP_SRCS CONFIGURE_DEPENDS
    "${ALLEGREXRECOMP_BUILD_DIR}/*.c"
    "${ALLEGREXRECOMP_BUILD_DIR}/crypto/*.c"
)
add_executable(allegrexrecomp ${ALLEGREXRECOMP_SRCS})
target_include_directories(allegrexrecomp PRIVATE "${ALLEGREXRECOMP_BUILD_DIR}")
if(MSVC)
    target_compile_options(allegrexrecomp PRIVATE /W3 /MP /utf-8)
    target_compile_definitions(allegrexrecomp PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(allegrexrecomp PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_link_libraries(allegrexrecomp PRIVATE m)
endif()

# 3. psprecomp ABI adapter target
add_library(redmp1_recomp_abi STATIC
    src/recomp_abi/cpu.c
    src/recomp_abi/mem.c
    src/recomp_abi/dispatch.c
    src/recomp_abi/trap.c
    src/recomp_abi/vfpu.c
)
target_include_directories(redmp1_recomp_abi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/recomp_abi>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/psprecomp/include>
)
target_link_libraries(redmp1_recomp_abi PUBLIC
    repsp::recomp
    repsp::module
    repsp::runtime
)
if(MSVC)
    target_compile_options(redmp1_recomp_abi PRIVATE /W3 /MP /utf-8)
    target_compile_definitions(redmp1_recomp_abi PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(redmp1_recomp_abi PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_link_libraries(redmp1_recomp_abi PRIVATE m)
endif()

# 4. Layer target: redmp1_game (title layer: profile, variant identification)
add_library(redmp1_game STATIC
    src/game/dmp1/dmp1_profile.cpp
)
target_include_directories(redmp1_game PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(redmp1_game PUBLIC
    repsp::module
    repsp::runtime
)
repsp_apply_host_flags(redmp1_game)

# 4. App target: redmp1 (CLI executable: argument parsing, orchestration)
add_executable(redmp1
    src/app/main.cpp
)
target_compile_definitions(redmp1 PRIVATE REDMP1_VERSION="${REDMP1_VERSION_STRING}")
target_link_libraries(redmp1 PRIVATE
    redmp1_game
    repsp::runtime
    repsp::platform
)
repsp_apply_host_flags(redmp1)

# Relocation verification tool (cross-check repsp vs allegrexrecomp)
add_executable(reloc_check
    tests/reloc_check.cpp
)
target_link_libraries(reloc_check PRIVATE
    repsp::module
)
repsp_apply_host_flags(reloc_check)

# recomp_abi_probe (CI test, runs without game data)
add_executable(recomp_abi_probe
    tests/recomp_abi_probe.cpp
)
target_link_libraries(recomp_abi_probe PRIVATE
    redmp1_recomp_abi
    repsp::runtime
    repsp::platform
)
repsp_apply_host_flags(recomp_abi_probe)
add_test(NAME recomp_abi_probe COMMAND $<TARGET_FILE:recomp_abi_probe>)

# 5. CTest and Smoke Tests
enable_testing()

# Smoke 1: No arguments -> non-zero exit code
add_test(NAME app_smoke_no_args COMMAND $<TARGET_FILE:redmp1>)
set_tests_properties(app_smoke_no_args PROPERTIES WILL_FAIL TRUE)

# Smoke 2: Non-existent directory -> non-zero exit code
add_test(NAME app_smoke_nonexistent_dir COMMAND $<TARGET_FILE:redmp1> "${CMAKE_CURRENT_BINARY_DIR}/nonexistent_dir_for_test")
set_tests_properties(app_smoke_nonexistent_dir PROPERTIES WILL_FAIL TRUE)

# Smoke 3: Directory missing PARAM.SFO -> non-zero exit code
add_test(NAME app_smoke_missing_sfo COMMAND $<TARGET_FILE:redmp1> "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_tests_properties(app_smoke_missing_sfo PROPERTIES WILL_FAIL TRUE)

# Smoke 4: Directory with corrupt/invalid SFO -> exits with InvalidParamSfo (3)
set(INVALID_SFO_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fixtures/invalid_sfo_game")
file(MAKE_DIRECTORY "${INVALID_SFO_FIXTURE_DIR}/PSP_GAME")
file(WRITE "${INVALID_SFO_FIXTURE_DIR}/PSP_GAME/PARAM.SFO" "INVALID_SFO_DATA")
add_test(NAME app_smoke_invalid_sfo
    COMMAND ${CMAKE_COMMAND}
        "-DCMD=$<TARGET_FILE:redmp1>"
        "-DARGS=${INVALID_SFO_FIXTURE_DIR}"
        "-DEXPECTED_CODE=3"
        "-DEXPECTED_REGEX=Failed to parse PARAM.SFO"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_exit_code.cmake"
)

# Smoke 5: Directory with valid SFO of unregistered DISC_ID -> exits with UnregisteredVariant (5)
set(UNREGISTERED_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fixtures/unregistered_game")
file(MAKE_DIRECTORY "${UNREGISTERED_FIXTURE_DIR}/PSP_GAME/SYSDIR")
file(WRITE "${UNREGISTERED_FIXTURE_DIR}/PSP_GAME/SYSDIR/BOOT.BIN" "DUMMY_EXECUTABLE_CONTENT_FOR_SMOKE_TEST")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_synthetic_sfo.py"
            "${UNREGISTERED_FIXTURE_DIR}/PSP_GAME/PARAM.SFO"
            --disc-id "TEST00000"
            --disc-version "1.00"
            --title "Synthetic Test Game"
    RESULT_VARIABLE gen_res
)
if(NOT gen_res EQUAL 0)
    message(WARNING "Failed to generate synthetic SFO for smoke test")
endif()
add_test(NAME app_smoke_unregistered
    COMMAND ${CMAKE_COMMAND}
        "-DCMD=$<TARGET_FILE:redmp1>"
        "-DARGS=${UNREGISTERED_FIXTURE_DIR}"
        "-DEXPECTED_CODE=5"
        "-DEXPECTED_REGEX=Unregistered variant"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_exit_code.cmake"
)

# Smoke 6: Registered roms (if present in local tree)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/roms/PSP_GAME/PARAM.SFO")
    add_test(NAME app_smoke_registered_roms COMMAND $<TARGET_FILE:redmp1> "${CMAKE_CURRENT_SOURCE_DIR}/roms" --variants "${CMAKE_CURRENT_SOURCE_DIR}/src/game/dmp1/recomp/variants.json")
endif()

# Relocation cross-check (runs only when roms exist)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/roms/PSP_GAME/SYSDIR/BOOT.BIN")
    add_test(NAME reloc_check_relocate
        COMMAND $<TARGET_FILE:allegrexrecomp> relocate
                "${CMAKE_CURRENT_SOURCE_DIR}/roms/PSP_GAME/SYSDIR/BOOT.BIN"
                0x08804000
                "${CMAKE_CURRENT_BINARY_DIR}/boot_reloc_allegrex.bin"
    )
    add_test(NAME reloc_check
        COMMAND $<TARGET_FILE:reloc_check>
                "${CMAKE_CURRENT_SOURCE_DIR}/roms/PSP_GAME/SYSDIR/BOOT.BIN"
                "${CMAKE_CURRENT_BINARY_DIR}/boot_reloc_allegrex.bin"
                0x08804000
    )
    set_tests_properties(reloc_check_relocate PROPERTIES FIXTURES_SETUP reloc_fixture)
    set_tests_properties(reloc_check PROPERTIES FIXTURES_REQUIRED reloc_fixture)
endif()
