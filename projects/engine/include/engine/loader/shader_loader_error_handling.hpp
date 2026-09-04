#pragma once
#include "engine/types/string.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/renderer/shader.hpp"

namespace bubble
{
// Which stage's text a log line is talking about. A compile log only ever
// concerns one stage; a link log can mention several, and NVIDIA heads each
// section with "Vertex info" / "Fragment info". Leave mActive empty when the
// stage is not known up front - quoting the wrong stage's source under an error
// is worse than quoting none.
struct StageSources
{
	string_view mActive;
	string_view mVertex;
	string_view mFragment;
	string_view mGeometry;
};

// GL's info log for a shader or a program object, without the null terminator
// GL counts in the length it reports.
string ShaderInfoLog( u32 shaderId );
string ProgramInfoLog( u32 programId );

// A shader's path is extensionless: it names a .vert/.frag pair, not a file
// anyone can open. Point at the stage that actually failed.
path StagePath( const Shader& shader, string_view extension );

// Reports a compile or link failure and throws. `what` reads as a clause -
// "the fragment stage failed to compile" - and is used both in the console
// report and in the exception. The console gets the driver log with the source
// each error refers to quoted underneath; the exception carries the shader's
// identity, because at project open nothing catches it and only what() is left.
[[noreturn]] void ThrowShaderError( const Shader& shader,
									string_view what,
									const path& file,
									string_view log,
									const StageSources& sources );
}
