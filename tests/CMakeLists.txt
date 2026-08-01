# =============================================================================
# tests/CMakeLists.txt
# Unit test suite using Qt Test (QTest) + CTest.
# =============================================================================

# ---------------------------------------------------------------------------
# Helper macro: declare a test executable
# Usage: mailinium_add_test(NAME TestFoo SOURCES foo_test.cpp)
# ---------------------------------------------------------------------------
macro(mailinium_add_test)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES;LINKS" ${ARGN})

    add_executable(${ARG_NAME} ${ARG_SOURCES})

    target_include_directories(${ARG_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
    )

    target_link_libraries(${ARG_NAME} PRIVATE
        Qt6::Core
        Qt6::Test
        ${ARG_LINKS}
    )

    mailinium_set_target_options(${ARG_NAME})
    mailinium_deploy_qt_executable(${ARG_NAME})

    add_test(NAME ${ARG_NAME} COMMAND ${ARG_NAME})
endmacro()

# ---------------------------------------------------------------------------
# Test subdirectories — added as modules are implemented
# ---------------------------------------------------------------------------
# add_subdirectory(core)
add_subdirectory(storage)
# add_subdirectory(protocols)
# add_subdirectory(sync)
