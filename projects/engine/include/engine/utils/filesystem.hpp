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
}

using path = filesystem::path;
}