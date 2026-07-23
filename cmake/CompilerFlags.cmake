# CMake Module: Compiler Flags, Security Hardening, Warnings as Errors, and Sanitizers

option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer (ASan)" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (UBSan)" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer (TSan)" OFF)
option(ENABLE_HARDENING "Enable security hardening flags" ON)

function(apply_compiler_flags TARGET_NAME)
    if(WIN32)
        target_compile_definitions(${TARGET_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /wd4324 /sdl)
        if(WARNINGS_AS_ERRORS)
            target_compile_options(${TARGET_NAME} PRIVATE /WX)
        endif()

        if(ENABLE_HARDENING)
            target_compile_options(${TARGET_NAME} PRIVATE /GS /guard:cf)
            target_link_options(${TARGET_NAME} PRIVATE /NXCOMPAT /DYNAMICBASE /guard:cf)
        endif()

    else() # GCC / Clang
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
        )

        if(WARNINGS_AS_ERRORS)
            target_compile_options(${TARGET_NAME} PRIVATE -Werror)
        endif()

        if(ENABLE_HARDENING)
            get_target_property(TARGET_TYPE ${TARGET_NAME} TYPE)
            target_compile_options(${TARGET_NAME} PRIVATE
                -fstack-protector-strong
            )
            if(TARGET_TYPE STREQUAL "EXECUTABLE")
                target_compile_options(${TARGET_NAME} PRIVATE -fPIE)
                target_link_options(${TARGET_NAME} PRIVATE -pie)
            endif()

            if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
                target_compile_definitions(${TARGET_NAME} PRIVATE _FORTIFY_SOURCE=2)
            endif()
            target_link_options(${TARGET_NAME} PRIVATE
                -Wl,-z,relro,-z,now
                -Wl,-z,noexecstack
            )
        endif()

        set(SANITIZER_FLAGS "")

        if(ENABLE_ASAN)
            list(APPEND SANITIZER_FLAGS "-fsanitize=address" "-fno-omit-frame-pointer")
        endif()

        if(ENABLE_UBSAN)
            list(APPEND SANITIZER_FLAGS "-fsanitize=undefined")
        endif()

        if(ENABLE_TSAN)
            if(ENABLE_ASAN)
                message(FATAL_ERROR "ThreadSanitizer (TSan) cannot be combined with AddressSanitizer (ASan).")
            endif()
            list(APPEND SANITIZER_FLAGS "-fsanitize=thread")
        endif()

        if(SANITIZER_FLAGS)
            target_compile_options(${TARGET_NAME} PRIVATE ${SANITIZER_FLAGS})
            target_link_options(${TARGET_NAME} PRIVATE ${SANITIZER_FLAGS})
        endif()
    endif()
endfunction()
