# Midnight — 2D sandbox

## Build and run

Requires a C++20 compiler, CMake 3.25+, SDL3, Vulkan with
`glslangValidator`, and libpng development packages. The first CMake
configure downloads the pinned nlohmann/json dependency; later builds
reuse it from the build directory.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
./build/midnight
```

## Map persistence

Startup loads `assets/maps/village.json` from the source checkout.
`Ctrl+S` saves the current map to that same file, independent of the
working directory. Finish painting, save, close, and launch again to
restore both layers. Edits are not autosaved.

The window title shows `Midnight *` while the map differs from the last
successful load or save. Saving successfully or undoing back to the saved
content removes the asterisk. Selection, layer, grid, and collision-overlay
changes do not affect it. A new blank map with no file stays marked until
its first successful save. Failed saves leave the indicator unchanged;
closing still discards unsaved edits without a confirmation prompt.

This increment supports the existing version-1 format: a 16×12 map,
16×16 tiles, the outdoor tileset, and Ground followed by Above Ground.
Tileset image paths are relative to the map file; the editor loads the
referenced source PNG. Above Ground occupancy determines collision.
Selections, undo history, and overlay visibility are not persisted.

A missing map starts a blank canvas. An invalid or unsupported map,
unreadable map, or missing/unreadable tileset stops startup with an error
without changing the saved file. Correct or back up and move the affected
file before restarting; no invalid map is silently replaced.
