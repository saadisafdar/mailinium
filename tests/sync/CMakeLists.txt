set(SYNC_SOURCES
    "${CMAKE_SOURCE_DIR}/src/core/Logger.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/Settings.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/SyncWorker.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/SyncManager.cpp"
    "${CMAKE_SOURCE_DIR}/src/network/ImapClient.cpp"
    "${CMAKE_SOURCE_DIR}/src/protocols/MimeParser.cpp"
    "${CMAKE_SOURCE_DIR}/src/auth/GoogleOAuth.cpp"
    "${CMAKE_SOURCE_DIR}/src/storage/Database.cpp"
    "${CMAKE_SOURCE_DIR}/src/storage/AccountStore.cpp"
    "${CMAKE_SOURCE_DIR}/src/storage/FolderStore.cpp"
    "${CMAKE_SOURCE_DIR}/src/storage/MessageStore.cpp"
)

mailinium_add_test(
    NAME SyncTest
    SOURCES SyncTest.cpp ${SYNC_SOURCES}
    LINKS Qt6::Sql Qt6::Network Qt6::Widgets
)

mailinium_add_test(
    NAME MessageOpsTest
    SOURCES MessageOpsTest.cpp ${SYNC_SOURCES}
    LINKS Qt6::Sql Qt6::Network Qt6::Widgets
)

mailinium_add_test(
    NAME RecoveryTest
    SOURCES RecoveryTest.cpp ${SYNC_SOURCES}
    LINKS Qt6::Sql Qt6::Network Qt6::Widgets
)

mailinium_add_test(
    NAME AccountAuthTest
    SOURCES AccountAuthTest.cpp ${SYNC_SOURCES}
    LINKS Qt6::Sql Qt6::Network Qt6::Widgets
)

mailinium_add_test(
    NAME PerformanceTest
    SOURCES PerformanceTest.cpp ${SYNC_SOURCES}
    LINKS Qt6::Sql Qt6::Network Qt6::Widgets
)

set(OBS_SOURCES
    ${SYNC_SOURCES}
    "${CMAKE_SOURCE_DIR}/src/core/DiagnosticsManager.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/ThemeManager.cpp"
    "${CMAKE_SOURCE_DIR}/src/ui/NotificationBanner.cpp"
    "${CMAKE_SOURCE_DIR}/src/storage/DraftStore.cpp"
)

mailinium_add_test(
    NAME ObservabilityTest
    SOURCES ObservabilityTest.cpp ${OBS_SOURCES}
    LINKS Qt6::Sql Qt6::Network Qt6::Widgets
)

mailinium_add_test(
    NAME ReleaseValidationTest
    SOURCES ReleaseValidationTest.cpp ${OBS_SOURCES}
    LINKS Qt6::Sql Qt6::Network Qt6::Widgets
)
