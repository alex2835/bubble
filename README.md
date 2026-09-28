# Bubble  Game Engine
C++ OpenGL Imgui

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

## Project layout
A project is a directory with a `<name>.bubble` file (resources, `global_state`,
which level a run starts in) and its levels under `levels/*.level` (a scene:
one tree of entities, see `docs/hierarchy_and_prefabs.md`). One level is open
at a time; `Level` menu in the editor creates and switches them, F5 runs the
one being edited, and a script switches with `load_level( "levels/arena.level" )`
(see `docs/scripting.md`).

## Editor scripting
The editor's verbs are operators (`scene.create_node`, `level.open`, ...) that
menus, hotkeys and Lua all call the same way. Try one in the Console window
or run a script at startup: `bubble_editor -p game.bubble -s setup.lua`.
See `docs/editor_scripting.md`.
