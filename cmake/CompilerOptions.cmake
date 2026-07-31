# =============================================================================
# CompilerOptions.cmake
# Configure warnings, sanitizers, and hardening for all targets.
# =============================================================================

# ---------------------------------------------------------------------------
# Per-compiler warning sets
# ---------------------------------------------------------------------------
if(MSVC)
    set(MAILINIUM_COMPILE_OPTIONS
        /W4           # High warning level (equivalent to -Wall -Wextra)
        /WX           # Treat warnings as errors
        /permissive-  # Strict standards conformance
        /Zc:__cplusplus   # Correct __cplusplus macro
        /utf-8        # Source and execution character sets are UTF-8
        /wd4251       # Disable: class needs dll-interface (Qt internals)
        /wd4127       # Disable: conditional expression is constant (Qt macros)
    )
    # Debug: runtime checks
    set(MAILINIUM_DEBUG_OPTIONS /RTC1 /sdl)
    # Release: optimise for speed, enable whole-program optimisation
    set(MAILINIUM_RELEASE_OPTIONS /O2 /GL)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set(MAILINIUM_COMPILE_OPTIONS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Werror           # Treat warnings as errors
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        list(APPEND MAILINIUM_COMPILE_OPTIONS
            -Wno-unknown-warning-option
        )
    endif()
    set(MAILINIUM_DEBUG_OPTIONS -g3 -fno-omit-frame-pointer)
    set(MAILINIUM_RELEASE_OPTIONS -O2)
endif()

# ---------------------------------------------------------------------------
# Sanitizers (debug builds only)
# Enable via: cmake -DMAILINIUM_ENABLE_SANITIZERS=ON
# ---------------------------------------------------------------------------
option(MAILINIUM_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan (debug builds)" OFF)

if(MAILINIUM_ENABLE_SANITIZERS AND NOT MSVC)
    message(STATUS "Mailinium: sanitizers enabled")
    set(SANITIZER_FLAGS -fsanitize=address,undefined -fno-sanitize-recover=all)
    add_compile_options(${SANITIZER_FLAGS})
    add_link_options(${SANITIZER_FLAGS})
endif()

# ---------------------------------------------------------------------------
# Helper function: apply standard options to a target
# Usage: mailinium_set_target_options(my_target)
# ---------------------------------------------------------------------------
function(mailinium_set_target_options target)
    target_compile_options(${target} PRIVATE
        ${MAILINIUM_COMPILE_OPTIONS}
        $<$<CONFIG:Debug>:${MAILINIUM_DEBUG_OPTIONS}>
        $<$<CONFIG:Release>:${MAILINIUM_RELEASE_OPTIONS}>
        $<$<CONFIG:RelWithDebInfo>:${MAILINIUM_RELEASE_OPTIONS}>
    )

    target_compile_definitions(${target} PRIVATE
        $<$<CONFIG:Debug>:MAILINIUM_DEBUG>
        $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
        $<$<CONFIG:RelWithDebInfo>:QT_NO_DEBUG_OUTPUT>
    )
endfunction()
