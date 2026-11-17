# Third-party notices

bubble includes the following libraries under `deps/`. Each keeps its full
license text beside its sources. A game built with bubble ships this file
next to the executable.

wgpu-native is the one library not kept under `deps/`: its prebuilt release
is downloaded at configure time (`deps/CMakeLists.txt`, pinned by version and
SHA-256) and ships as `wgpu_native.dll` / `libwgpu_native.so`.

| library | version | license | file |
| --- | --- | --- | --- |
| [assimp](https://github.com/assimp/assimp) | 6.0.6 | BSD-3-Clause | `deps/assimp/LICENSE`, and its bundled libraries' own under `deps/assimp/contrib` (editor and cook only) |
| [doctest](https://github.com/doctest/doctest) | 2.4.12 | MIT | `deps/doctest/LICENSE.txt` (tests only) |
| [enkiTS](https://github.com/dougbinks/enkiTS) | 1.12 | Zlib | `deps/enkiTS/License.txt` (not on the web) |
| [EnTT](https://github.com/skypjack/entt) | 3.16.0 | MIT | `deps/entt/LICENSE` |
| [GLM](https://github.com/g-truc/glm) | 1.0.3 | MIT | `deps/glm/copying.txt` |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | 5.7.0 | MIT | `deps/jolt/LICENSE` |
| [Luau](https://github.com/luau-lang/luau) | 0.741 | MIT | `deps/luau/LICENSE.txt`, `deps/luau/lua_LICENSE.txt` |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | 1.3 | MIT | `deps/meshoptimizer/LICENSE.md` |
| [miniaudio](https://github.com/mackron/miniaudio) | 0.11.26 | public domain (Unlicense) or MIT-0 | `deps/miniaudio/LICENSE` |
| [ozz-animation](https://github.com/guillaumeblanc/ozz-animation) | 0.17.0 | MIT | `deps/ozz/LICENSE.md` (its runtime in the game; the offline builders in the editor and cook only) |
| [SDL](https://github.com/libsdl-org/SDL) | 3.4.20 | Zlib | `deps/SDL/LICENSE.txt`; its hidapi under BSD-3-Clause (`deps/SDL/src/hidapi/LICENSE-bsd.txt`), its yuv2rgb's `deps/SDL/src/video/yuv2rgb/LICENSE` |
| [stb_image](https://github.com/nothings/stb) | 2.32 | MIT or public domain | at the end of `deps/stb/stb_image.h` (editor and cook only) |
| [Tracy](https://github.com/wolfpld/tracy) | 0.14.1 | BSD-3-Clause | `deps/tracy/LICENSE` (profiling builds only) |
| [wgpu-native](https://github.com/gfx-rs/wgpu-native) | v29.0.1.1 | MIT or Apache-2.0 | `LICENSE.MIT` and `LICENSE.APACHE` in its repository (desktop only; the web uses the browser's WebGPU) |
| [utf8proc](https://github.com/JuliaStrings/utf8proc) | 2.12.0 | MIT | `deps/utf8proc/LICENSE.md` |
