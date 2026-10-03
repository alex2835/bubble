#pragma once
#include "bubble/types/string.hpp"
#include <filesystem>
#include <functional>

namespace bubble
{
// A file of the project, named from the project's root: "models/cube.obj".
// Always UTF-8, forward slashes, no "." or ".." steps, never absolute - the
// same on every platform and in every file it is written to. It becomes an
// OS path only to touch the disk.
class AssetPath
{
public:
    AssetPath() = default;

    // Accepts backslashes and redundant "." steps and slashes, and tidies
    // them; refuses what does not name a file inside the project.
    static expected<AssetPath, string> From( string_view text );

    string_view View() const { return mPath; }
    const string& String() const { return mPath; }
    bool Empty() const { return mPath.empty(); }

    // "cube.obj", "cube", ".obj" (empty when there is none).
    string_view Filename() const;
    string_view Stem() const;
    string_view Extension() const;
    // "models" for "models/cube.obj"; empty at the root.
    AssetPath Parent() const;
    // `relative` inside this one, as From reads it.
    expected<AssetPath, string> Join( string_view relative ) const;

    std::filesystem::path ToOsPath( const std::filesystem::path& projectRoot ) const;

    friend bool operator==( const AssetPath&, const AssetPath& ) = default;
    friend auto operator<=>( const AssetPath&, const AssetPath& ) = default;

private:
    explicit AssetPath( string path ) : mPath( std::move( path ) ) {}
    string mPath;
};
}

template <>
struct std::hash<bubble::AssetPath>
{
    size_t operator()( const bubble::AssetPath& path ) const { return std::hash<std::string_view>{}( path.View() ); }
};
