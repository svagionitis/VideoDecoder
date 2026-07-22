# Compiler flags for warnings, security hardening, and sanitizers.

if(MSVC)
    add_compile_options(
        /WX /W4
        /sdl            # Enable extra SDL checks
        /guard:cf       # Control Flow Guard
        /GS             # Buffer security checks
    )
    add_link_options(
        /DYNAMICBASE    # Use ASLR
        /NXCOMPAT       # Use DEP/NX
        /guard:cf
    )
else()
    add_compile_options(
        -Werror
        -Wall
        -Wextra
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wnull-dereference
        -fstack-protector-strong
    )

    # Enable position-independent code (PIE) for all targets
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    # Enable fortify source in non-Debug builds (requires optimization)
    add_compile_options($<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE> $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=3>)

    # Hardened linker flags
    add_link_options(-Wl,-z,relro -Wl,-z,now)
endif()

# Sanitizers option (ASan & UBSan) for testing/development
option(USE_SANITIZERS "Enable ASan and UBSan runtime checks" OFF)
if(USE_SANITIZERS)
    if(MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()
