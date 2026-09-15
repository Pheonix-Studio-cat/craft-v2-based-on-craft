# Craft in the browser

`build.sh` compiles the same C sources the desktop build uses into WebAssembly
and WebGL 1, with touch controls in `shell.html`. Every change in `src/` is
behind `#ifdef __EMSCRIPTEN__`, so the desktop build (`cmake . && make`) is
untouched.

    ./web/build.sh          # needs emcc on PATH, writes build/web/

## What had to change, and why

| Problem | What the browser build does |
| --- | --- |
| No threads without `SharedArrayBuffer`, which GitHub Pages cannot enable (it cannot send the COOP/COEP headers) | `WORKERS` is 1 and the worker runs on the main thread, one chunk per frame; the database queue is drained on commit instead of by its own thread |
| The main loop must give the browser its turn | `-sASYNCIFY` plus one `await requestAnimationFrame` at the point where the desktop build swaps buffers, so the loop keeps its shape |
| WebGL needs GLSL ES, the desktop needs GLSL 1.20 | The `#version` line is prepended when the shader is loaded (`src/util.c`), not stored in the files |
| No pointer lock on a touch screen | The look comes from dragging on the canvas; craft's request for `GLFW_CURSOR_DISABLED` is compiled out |
| A phone is not a desktop | Chunk radius 5 instead of 10, and the HUD is drawn at device-pixel scale |
| No curl, no sockets | Offline single player only; `get_access_token()` returns 0 and the online mode is unreachable |
| A tab can be closed at any time | The sqlite file lives in an IDBFS directory (`/craft`) that is written back to IndexedDB after every commit |

## Known limits

* Single player only.
* Signs and chat need a keyboard; there is no on-screen text input yet.
* The frame rate on real phone hardware has not been measured -- the build was
  tested in headless Chromium with SwiftShader, which says nothing about speed.
