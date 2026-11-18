#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"

namespace bubble
{
// The editor's font, Roboto Medium, as its file's bytes: built in by
// cmake/embed_file.cmake from deps/roboto.
span<const u8> EditorFont();
}
