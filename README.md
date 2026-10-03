# bubble

A small 3D game engine: plain-data components on EnTT, reflection-driven
editing, Luau scripting, WebGPU rendering for desktop, web and mobile.

The previous engine lives on as `bubble0.7`; this one is rebuilt from it a
stage at a time, each stage closed by tests before the next.

## Build

Presets live in `CMakePresets.json`:

The engine is built with clang on every platform.

| preset | platform |
| --- | --- |
| `windows-debug`, `windows-release` | clang-cl, from a Visual Studio developer prompt (VS component "C++ Clang Compiler for Windows") |
| `windows-profile` | release with the Tracy client; connect the [Tracy profiler 0.14.1](https://github.com/wolfpld/tracy/releases/tag/v0.14.1) |
| `linux` | clang 20 or newer with libc++ (`libc++-dev`, `libc++abi-dev`) |
| `web` | Emscripten, with `EMSDK` set |

```
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

## License

MIT, see `LICENSE`. Third-party libraries in `deps/` keep their own licenses
beside them.
