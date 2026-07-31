# Development Guide

## Requirements

- CMake 3.25+
- Qt 6.5+ (LTS recommended: 6.7)
- C++20-capable compiler:
  - Windows: MSVC 2022 (v17.x+)
  - macOS: Xcode 15+ (AppleClang 15+)
  - Linux: GCC 12+ or Clang 16+

## Build

```bash
# Configure (replace path with your Qt 6 installation)
cmake -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.7.0/msvc2022_64

# Build
cmake --build build --config Release

# Run tests
ctest --test-dir build -C Release
```

## Debug Build with Sanitizers

```bash
cmake -B build-san \
  -DCMAKE_BUILD_TYPE=Debug \
  -DMAILINIUM_ENABLE_SANITIZERS=ON \
  -DCMAKE_PREFIX_PATH=/path/to/qt6

cmake --build build-san
```

## Project Layout

See `docs/architecture.md` for the full module table and coding standards.

## Log Files

Logs are written to `<data-dir>/logs/mailinium_N.log`.

- Debug builds: also echoed to stderr
- Max 3 files × 5 MB, rotating

## IDE Setup

The build generates `compile_commands.json` at `build/compile_commands.json`.
Point clangd or your IDE to this file for accurate code completion and navigation.
