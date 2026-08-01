# =============================================================================
# Deployment.cmake
# Helper functions for deploying application and test executables
# =============================================================================

# Runs windeployqt on the given target post-build on Windows.
function(mailinium_deploy_qt_executable TARGET_NAME)
    if(WIN32)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND "$<TARGET_FILE:Qt6::windeployqt>"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:${TARGET_NAME}>"
            COMMENT "Running windeployqt for ${TARGET_NAME}..."
            VERBATIM
        )
    endif()
endfunction()
