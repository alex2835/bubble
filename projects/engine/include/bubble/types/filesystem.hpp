#pragma once
#include <filesystem>
#include <system_error>

namespace bubble
{
namespace fs = std::filesystem;

// A path as the OS spells it, only to touch the disk; inside the engine a
// file is an AssetPath. Never built from a string: PathFromUtf8.
using OsPath = std::filesystem::path;
// What the filesystem calls give back instead of throwing.
using std::error_code;
}
