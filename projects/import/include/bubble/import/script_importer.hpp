#pragma once
#include "bubble/import/importer.hpp"

namespace bubble
{
// .luau into bytecode, kind "script": what the engine's script loader
// reads. A file that does not compile is refused with Luau's message, the
// path and line first. Next: the analyzer's strict check, and the props and
// requires read from the AST.
Importer ScriptImporter();
}
