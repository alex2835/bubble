#pragma once 
#include "engine/types/string.hpp"
#include <filesystem>
#include <fstream>

namespace bubble
{
namespace filesystem
{
using namespace std::filesystem;

string readFile( const path& file );
std::ifstream openStream( const path& file );

// Directory the running executable lives in. Everything the editor persists is
// resolved against this instead of the working directory, which depends on how
// the editor was launched.
const path& executableDir();

// Resolves a relative path against executableDir(), absolute paths pass through.
path resolveNearExecutable( const path& relative );
}

using path = filesystem::path;
}