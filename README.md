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

## Map zoom

Press `+`/`=` or `-` (keypad keys also work) to switch between 1×, 2×,
and 3× map zoom. The default is 2×. Each 16×16 tile occupies exactly
16×16, 32×32, or 48×48 framebuffer pixels, including after resizing.
The map grid, collision overlay, hover, selection outline, and painting
follow the same zoom; the atlas and selected-sprite preview are unchanged.

Finish any drag before changing zoom. Zoom is a view setting: it does not
change the map, undo history, or unsaved indicator, and resets on restart.
In small windows the map stays to the right of the atlas but may be
clipped; zoom out or enlarge the window to see the full canvas.

## Map persistence

Startup loads `assets/maps/village.json` from the source checkout.
`Ctrl+S` saves the current map to that same file, independent of the
working directory. Finish painting, save, close, and launch again to
restore both layers. Edits are not autosaved.

The window title shows `Midnight *` while the map differs from the last
successful load or save. Saving successfully or undoing back to the saved
content removes the asterisk. Selection, layer, grid, and collision-overlay
changes do not affect it. A new blank map with no file stays marked until
its first successful save. Failed saves leave the indicator unchanged.

Escape or the window close button asks you to **Save**, **Discard**, or
**Cancel** when changes are unsaved. Save closes only after a successful
write; a failed save keeps the map open. Cancel (also Enter, Escape, or
dismissing the dialog) returns to editing. Maps with no unsaved changes
close immediately.

This increment supports the existing version-1 format: a 16×12 map,
16×16 tiles, the outdoor tileset, and Ground followed by Above Ground.
Tileset image paths are relative to the map file; the editor loads the
referenced source PNG. Above Ground occupancy determines collision.
Selections, undo history, and overlay visibility are not persisted.

A missing map starts a blank canvas. An invalid or unsupported map,
unreadable map, or missing/unreadable tileset stops startup with an error
without changing the saved file. Correct or back up and move the affected
file before restarting; no invalid map is silently replaced.
