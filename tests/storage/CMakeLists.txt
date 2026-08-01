# =============================================================================
# tests/storage/CMakeLists.txt
# Unit tests for the storage layer.
# Each test opens an in-memory SQLite database (:memory:) for full isolation.
# =============================================================================

set(STORAGE_SOURCES
    "${CMAKE_SOURCE_DIR}/src/core/Logger.cpp"
    "${CMAKE_SOURCE_DIR}/src/core/Settings.cpp"
    "${CMAKE_SOURCE_DIR}/src/storage/Database.cpp"
    "${CMAKE_SOURCE_DIR}/src/storage/FolderStore.cpp"
    "${CMAKE_SOURCE_DIR}/src/storage/MessageStore.cpp"
)

mailinium_add_test(
    NAME DatabaseTest
    SOURCES DatabaseTest.cpp ${STORAGE_SOURCES}
    LINKS Qt6::Sql Qt6::Widgets
)

mailinium_add_test(
    NAME FolderStoreTest
    SOURCES FolderStoreTest.cpp ${STORAGE_SOURCES}
    LINKS Qt6::Sql Qt6::Widgets
)

mailinium_add_test(
    NAME MessageStoreTest
    SOURCES MessageStoreTest.cpp ${STORAGE_SOURCES}
    LINKS Qt6::Sql Qt6::Widgets
)
