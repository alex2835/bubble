#pragma once
#include "engine/types/string.hpp"
#include "engine/types/map.hpp"
#include "engine/types/array.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/renderer/shader.hpp"

namespace bubble
{
// shader modules search path
// One WGSL file per shader; both entry points live in it.
constexpr string_view SHADER_EXTENSION = ".wgsl"sv;
// Includable snippets. Same language, kept in their own directory.
constexpr string_view MODULE_EXTENSION = ".wgsl"sv;
constexpr string_view SHADER_MODULES_SEARCH_PATH = "./resources/shaders/modules"sv;
// A project's own module directory, relative to its root. Searched before the
// engine's, so a project can shadow an engine module with one of its own.
constexpr string_view PROJECT_SHADER_MODULES_SUBDIR = "shaders/modules"sv;


// Where `#include <module>` looks, in addition to SHADER_MODULES_SEARCH_PATH.
// Process-wide rather than a Loader member because shaders are also loaded
// through the free LoadShader - by the engine at startup and by the editor's
// hot reloader - and all of them have to resolve the same modules.
void SetProjectShaderModulesDir( const path& dir );
const path& GetProjectShaderModulesDir();

// Every directory searched for modules, project first. Missing ones are
// skipped, so this is also the list the hot reloader watches.
vector<path> ShaderModuleSearchDirs();


// A module's text plus the directory it was found in, so a relative #include
// inside a module resolves against the module and not against whoever
// included it.
struct ShaderModuleSource
{
	string mText;
	path mDir;
};
using ShaderModuleTable = map<string, ShaderModuleSource>;

// One splice of source, and which engine modules it pulled in. Returned by
// value all the way up the recursion rather than accumulated into a stream and
// a bitset threaded down by reference.
struct ProcessedSource
{
	string mText;
	ShaderModules mModules;
};


// Every module file in the search directories, keyed by stem. Read fresh on every
// shader load - caching it is what once made a module edited on disk take
// effect only after restarting the editor.
ShaderModuleTable GetShaderModules();

// The file's text with every #include spliced in: `<module>` by name from the
// table, `"path"` relative to whichever file the directive is written in.
// Throws on an unknown module, a missing file, or an include cycle.
ProcessedSource ExpandShaderIncludes( const path& shaderPath, const ShaderModuleTable& modules );
}
