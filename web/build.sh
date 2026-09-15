#!/bin/sh
# Build the browser version of Craft: WebAssembly + WebGL 1.
#
# Needs the Emscripten SDK on the PATH (emcc). The result is written to
# build/web/ and is a static site: index.html, index.js, index.wasm and
# index.data. .github/workflows/pages.yml publishes exactly that directory.
#
#   ./web/build.sh
set -e

cd "$(dirname "$0")/.."
mkdir -p build/web

emcc \
    src/auth.c src/client.c src/cube.c src/db.c src/item.c src/main.c \
    src/map.c src/matrix.c src/ring.c src/sign.c src/util.c src/world.c \
    deps/lodepng/lodepng.c \
    deps/noise/noise.c \
    deps/sqlite/sqlite3.c \
    deps/tinycthread/tinycthread.c \
    -Ideps/lodepng -Ideps/noise -Ideps/sqlite -Ideps/tinycthread \
    -std=gnu99 -O3 \
    -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION \
    -sUSE_GLFW=3 \
    -sMIN_WEBGL_VERSION=1 -sMAX_WEBGL_VERSION=1 \
    -sASYNCIFY -sASYNCIFY_STACK_SIZE=32768 \
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 \
    -sSTACK_SIZE=5242880 \
    -sEXPORTED_RUNTIME_METHODS=FS,ccall \
    -sEXPORTED_FUNCTIONS=_main,_craft_touch_move,_craft_touch_look,_craft_touch_jump,_craft_toggle_fly,_craft_is_flying,_craft_break_block,_craft_place_block,_craft_pick_block,_craft_cycle_item,_craft_resize \
    -lidbfs.js \
    --preload-file textures \
    --preload-file shaders \
    --pre-js web/pre.js \
    --shell-file web/shell.html \
    -o build/web/index.html

echo "built build/web/index.html"
