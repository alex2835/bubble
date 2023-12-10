# Buble
C++ OpeGL Imgui - Game engine.

# Windows build
mkdir build
cd build
cmake ..
cmake --build .

# Emscripten(Webassembly) build
mkdir build
cd build
emcmake cmake ..
cmake --build .
