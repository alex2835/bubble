#pragma once 
#include "engine/utils/types.hpp"
#include <filesystem>
#include <fstream>

namespace bubble
{
namespace filesystem
{
using namespace std::filesystem;

string readFile( const path& file );
}

using path = filesystem::path;
}