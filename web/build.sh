#!/bin/sh
# Build the browser version of Craft: WebAssembly + WebGL 1.
#
# Needs the Emscripten SDK on the PATH (emcc). The result is written to
# build/web/ and is a static site: index.html, index.js, index.wasm,
# index.data plus the files the interface itself loads (ui.js, ui.css and a
# plain copy of the texture atlas, which the inventory draws its icons from).
# .github/workflows/pages.yml publishes exactly that directory.
#
#   ./web/build.sh
set -e

cd "$(dirname "$0")/.."
mkdir -p build/web

# The export list is read out of the source, so a newly added
# EMSCRIPTEN_KEEPALIVE function can never be forgotten here.
EXPORTS=$(grep -o 'EMSCRIPTEN_KEEPALIVE[^(]*craft_[a-z_0-9]*' src/*.c |
    sed 's/.*\(craft_[a-z_0-9]*\)/_\1/' | sort -u | tr '\n' ',')_main

emcc \
    src/*.c \
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
    -sEXPORTED_RUNTIME_METHODS=FS,ccall,UTF8ToString \
    -sEXPORTED_FUNCTIONS="$EXPORTS" \
    -lidbfs.js \
    --preload-file textures \
    --preload-file shaders \
    --pre-js web/pre.js \
    --shell-file web/shell.html \
    -o build/web/index.html

cp web/ui.js web/ui.css build/web/
mkdir -p build/web/addons
cp web/addons/*.json build/web/addons/
cp textures/texture.png build/web/texture.png

echo "built build/web/index.html"
