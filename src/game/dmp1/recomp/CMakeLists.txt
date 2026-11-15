if(REDMP1_RECOMP_SRCS)
    if(MSVC)
        # The generated code is always built optimized so that lifted tail calls become
        # jumps (issue #16). MSVC rejects /O2 together with /RTC1, and the Visual Studio
        # generator takes these from both the C and C++ configuration flags.
        foreach(_flag_var IN ITEMS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELWITHDEBINFO
                                   CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            string(REPLACE "/RTC1" "" ${_flag_var} "${${_flag_var}}")
            string(REPLACE "/Od" "" ${_flag_var} "${${_flag_var}}")
            string(REPLACE "/Ob0" "" ${_flag_var} "${${_flag_var}}")
        endforeach()
    endif()

    add_library(redmp1_recomp STATIC
        ${REDMP1_RECOMP_SRCS}
    )
    target_include_directories(redmp1_recomp PUBLIC
        $<BUILD_INTERFACE:${REDMP1_GENERATED_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/psprecomp/include>
    )
    target_link_libraries(redmp1_recomp PUBLIC
        redmp1_recomp_abi
    )
    if(REDMP1_TRACE_GUEST)
        target_compile_definitions(redmp1_recomp PRIVATE PSPRECOMP_TRACE)
    endif()
    if(MSVC)
        target_compile_options(redmp1_recomp PRIVATE /O2 /W0 /MP /utf-8 /bigobj)
        target_compile_definitions(redmp1_recomp PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(redmp1_recomp PRIVATE -O2 -w)
    endif()
endif()
