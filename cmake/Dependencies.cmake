# =============================================================================
# Dependencies.cmake
# Locate and configure all third-party dependencies.
# =============================================================================

# ---------------------------------------------------------------------------
# Qt
# ---------------------------------------------------------------------------
# Require Qt 6. The consumer must have Qt6 on CMAKE_PREFIX_PATH, e.g.:
#   cmake -DCMAKE_PREFIX_PATH=C:/Qt/6.7.0/msvc2022_64 ..
#
# Required components:
#   Core       — QObject, QString, QThread, QSettings, QSql, etc.
#   Gui        — QPainter, QFont, QIcon, etc.
#   Widgets    — QMainWindow, QListView, QTreeView, etc.
#   Network    — QTcpSocket, QSslSocket, QNetworkAccessManager (OAuth)
#   Sql        — QSqlDatabase (SQLite driver)
#   Test       — QTest (used only in tests/)
#
# Optional (added later):
#   WebEngineWidgets — QWebEngineView for sandboxed HTML email rendering
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Sql
    Test
)

find_package(Qt6 OPTIONAL_COMPONENTS
    WebEngineWidgets
)

if(Qt6WebEngineWidgets_FOUND)
    message(STATUS "Mailinium: Qt WebEngineWidgets found — enabling QWebEngineView HTML renderer")
    add_compile_definitions(MAILINIUM_HAS_WEBENGINE)
else()
    message(STATUS "Mailinium: Qt WebEngineWidgets NOT found — falling back to QTextBrowser HTML renderer")
endif()

message(STATUS "Mailinium: Qt version ${Qt6_VERSION}")

# Enforce minimum Qt version
if(Qt6_VERSION VERSION_LESS "6.5.0")
    message(FATAL_ERROR "Mailinium requires Qt 6.5 or later (found ${Qt6_VERSION})")
endif()

# ---------------------------------------------------------------------------
# SQLite
# Qt's Sql module ships with a SQLite plugin; we rely on that rather than
# linking a separate libsqlite3, keeping the dependency count low.
# If FTS5 or advanced SQLite features require a newer SQLite than Qt bundles,
# we can link our own libsqlite3 here in the future.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Platform-specific dependencies (stubs — filled in when platform/ is built)
# ---------------------------------------------------------------------------
# Windows: Crypt32, Credui (Credential Manager) — linked in platform/ CMake
# macOS:   Security.framework                    — linked in platform/ CMake
# Linux:   libsecret-1                           — linked in platform/ CMake
