# =============================================================================
# src/CMakeLists.txt
# Defines the Mailinium executable and organises source files by module.
# =============================================================================

# ---------------------------------------------------------------------------
# Collect sources
# Each subdirectory will eventually have its own CMakeLists that appends to
# a MAILINIUM_SOURCES variable, but for Phase 2 we list them here directly
# so the project builds immediately without placeholder implementations.
# ---------------------------------------------------------------------------

set(MAILINIUM_SOURCES
    main.cpp

    # core/
    core/Application.h
    core/Application.cpp
    core/Logger.h
    core/Logger.cpp
    core/Settings.h
    core/Settings.cpp
    core/ThemeManager.h
    core/ThemeManager.cpp
    core/SyncManager.h
    core/SyncManager.cpp
    core/SyncWorker.h
    core/SyncWorker.cpp

    # ui/
    ui/MainWindow.h
    ui/MainWindow.cpp
    ui/AccountSidebar.h
    ui/AccountSidebar.cpp
    ui/AccountSetupWizard.h
    ui/AccountSetupWizard.cpp
    ui/MessageListPanel.h
    ui/MessageListPanel.cpp
    ui/MessageViewPanel.h
    ui/MessageViewPanel.cpp
    ui/SettingsDialog.h
    ui/SettingsDialog.cpp
    
    # network/
    network/ImapClient.h
    network/ImapClient.cpp
    
    # auth/
    auth/GoogleOAuth.h
    auth/GoogleOAuth.cpp
    
    # storage/
    storage/Folder.h
    storage/Message.h
    storage/Database.h
    storage/Database.cpp
    storage/Account.h
    storage/AccountStore.h
    storage/AccountStore.cpp
    storage/FolderStore.h
    storage/FolderStore.cpp
    storage/MessageStore.h
    storage/MessageStore.cpp
)

set(MAILINIUM_RESOURCES
    ../resources/mailinium.qrc
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
if(WIN32)
    # WIN32 suppresses the console window in release builds
    add_executable(Mailinium WIN32
        ${MAILINIUM_SOURCES}
        ${MAILINIUM_RESOURCES}
    )
elseif(APPLE)
    add_executable(Mailinium MACOSX_BUNDLE
        ${MAILINIUM_SOURCES}
        ${MAILINIUM_RESOURCES}
    )
else()
    add_executable(Mailinium
        ${MAILINIUM_SOURCES}
        ${MAILINIUM_RESOURCES}
    )
endif()

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
target_include_directories(Mailinium PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# ---------------------------------------------------------------------------
# Link Qt modules
# ---------------------------------------------------------------------------
target_link_libraries(Mailinium PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)

# ---------------------------------------------------------------------------
# Apply compiler options
# ---------------------------------------------------------------------------
mailinium_set_target_options(Mailinium)

# ---------------------------------------------------------------------------
# Windows: Qt deployment helper
# Copies Qt DLLs next to the executable after build (debug only)
# ---------------------------------------------------------------------------
mailinium_deploy_qt_executable(Mailinium)
