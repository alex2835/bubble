# Buble  Game Engine
C++ OpeGL Imgui

## Build
### Windows/MacOS
~~~
mkdir build
cd build
cmake ..
cmake --build .
~~~

### Emscripten(Webassembly)
~~~
mkdir build
cd build
emcmake cmake ..
cmake --build .
~~~
Run server in bin directory
~~~
python3 -m http.server
~~~
and follow http://localhost:8000/
