# Mailinium Architecture

## Overview

Mailinium is a native C++/Qt desktop email client. This document is the authoritative reference for the application's architecture and design decisions.

For the full Phase 1 architecture document, see the project kickoff plan.

---

## Layer Dependency Rules

```
UI → Application → Service → Protocol → Storage → Platform
```

**No layer may call upward.** This is enforced by code review.

---

## Module Summary

| Directory | Purpose |
|---|---|
| `src/core/` | Application lifecycle, Logger, Settings |
| `src/accounts/` | Account data model and management |
| `src/protocols/` | IMAP, SMTP, OAuth protocol clients |
| `src/sync/` | Sync scheduler, workers, IMAP IDLE |
| `src/storage/` | SQLite wrapper, message/folder stores |
| `src/models/` | Qt item models for UI |
| `src/ui/` | Qt Widgets UI components |
| `src/platform/` | OS-specific: keychain, notifications |
| `src/security/` | Token vault, certificate pinning |

---

## Coding Standards

- C++20, no extensions (`CMAKE_CXX_EXTENSIONS OFF`)
- Warnings treated as errors on all compilers
- No raw `QSettings` calls outside `core/Settings.h`
- No synchronous I/O on the UI (main) thread
- No passwords or tokens written to SQLite or settings files
- No exceptions for control flow; use `std::expected<T, E>` in protocol/storage layers
- All strings: `QStringLiteral(...)` for compile-time string literals

---

## Data Directory

| Platform | Path |
|---|---|
| Windows | `%APPDATA%\Mailinium\` |
| macOS | `~/Library/Application Support/Mailinium/` |
| Linux | `~/.local/share/mailinium/` |

---

## Build

```bash
cmake -B build -DCMAKE_PREFIX_PATH=<Qt6 prefix>
cmake --build build
ctest --test-dir build
```

Enable sanitizers:
```bash
cmake -B build -DMAILINIUM_ENABLE_SANITIZERS=ON -DCMAKE_PREFIX_PATH=<Qt6 prefix>
```
