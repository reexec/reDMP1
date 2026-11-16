if(REDMP1_RECOMP_SRCS)
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
        target_compile_options(redmp1_recomp PRIVATE /W0 /MP /utf-8 /bigobj $<$<CONFIG:Release>:/Z7>)
        target_compile_definitions(redmp1_recomp PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(redmp1_recomp PRIVATE -w)
    endif()
endif()
