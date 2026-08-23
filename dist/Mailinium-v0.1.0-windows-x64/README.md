# Mailinium

A lightweight, native desktop email client built with C++ and Qt.

## Status

Phase 2 — Project initialized. Core infrastructure (Logger, Settings, Application) is in place. UI features begin in Phase 3.

## Build Requirements

- CMake 3.25+
- Qt 6.5+ (LTS recommended)
- C++20 compiler (MSVC 2022, GCC 12+, or Clang 16+)

## Quick Start

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/qt6
cmake --build build
./build/bin/Mailinium
```

See [`docs/development.md`](docs/development.md) for full build instructions and [`docs/architecture.md`](docs/architecture.md) for the system design.
