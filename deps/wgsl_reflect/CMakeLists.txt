# wgsl_reflect - what a WGSL module declares at its top: structs laid out
# for the uniform address space, bindings, functions. The standard library
# only; built with the same warnings as the engine, since it is our own code.
add_library( wgsl_reflect STATIC src/wgsl_reflect.cpp include/wgsl_reflect/wgsl_reflect.hpp )
target_include_directories( wgsl_reflect PUBLIC include )
bubble_target_warnings( wgsl_reflect )
