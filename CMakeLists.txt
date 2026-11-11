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
set(REPSP_BUILD_PROBES ON CACHE BOOL "Build repsp verification probes" FORCE)

add_subdirectory("${REDMP1_REPSP_DIR}" repsp)

# 2. Host tool: allegrexrecomp (from third_party/psprecomp without psprecomp runtime target)
set(ALLEGREXRECOMP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/psprecomp/tools/allegrexrecomp")
add_executable(allegrexrecomp
    ${ALLEGREXRECOMP_SRC_DIR}/decode.c
    ${ALLEGREXRECOMP_SRC_DIR}/container.c
    ${ALLEGREXRECOMP_SRC_DIR}/analyze.c
    ${ALLEGREXRECOMP_SRC_DIR}/emit.c
    ${ALLEGREXRECOMP_SRC_DIR}/keys.c
    ${ALLEGREXRECOMP_SRC_DIR}/crypto/aes.c
    ${ALLEGREXRECOMP_SRC_DIR}/crypto/cmac.c
    ${ALLEGREXRECOMP_SRC_DIR}/crypto/sha1.c
    ${ALLEGREXRECOMP_SRC_DIR}/crypto/kirk.c
    ${ALLEGREXRECOMP_SRC_DIR}/main.c
)
target_include_directories(allegrexrecomp PRIVATE ${ALLEGREXRECOMP_SRC_DIR})
if(MSVC)
    target_compile_options(allegrexrecomp PRIVATE /W3 /MP /utf-8)
    target_compile_definitions(allegrexrecomp PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(allegrexrecomp PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_link_libraries(allegrexrecomp PRIVATE m)
endif()

# 3. Layer target: redmp1_game (title layer: profile, variant identification)
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
target_link_libraries(redmp1 PRIVATE
    redmp1_game
    repsp::runtime
    repsp::platform
)
repsp_apply_host_flags(redmp1)

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

# Smoke 4: Directory with invalid/unregistered SFO -> non-zero exit code
set(UNREGISTERED_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fixtures/unregistered_game")
file(MAKE_DIRECTORY "${UNREGISTERED_FIXTURE_DIR}/PSP_GAME")
file(WRITE "${UNREGISTERED_FIXTURE_DIR}/PSP_GAME/PARAM.SFO" "INVALID_SFO_DATA")
add_test(NAME app_smoke_unregistered COMMAND $<TARGET_FILE:redmp1> "${UNREGISTERED_FIXTURE_DIR}")
set_tests_properties(app_smoke_unregistered PROPERTIES WILL_FAIL TRUE)

# Smoke 5: Registered roms (if present in local tree)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/roms/PSP_GAME/PARAM.SFO")
    add_test(NAME app_smoke_registered_roms COMMAND $<TARGET_FILE:redmp1> "${CMAKE_CURRENT_SOURCE_DIR}/roms" --variants "${CMAKE_CURRENT_SOURCE_DIR}/src/game/dmp1/recomp/variants.json")
endif()
