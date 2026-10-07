#include "bubble/core/asset_path.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/process.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/stream.hpp"
#include <cstdio>
#include <doctest.h>

using namespace bubble;

TEST_CASE( "Utf8Valid takes well-formed text in any script" )
{
    CHECK( Utf8Valid( "" ) );
    CHECK( Utf8Valid( "hello" ) );
    CHECK( Utf8Valid( "Привет" ) );
    CHECK( Utf8Valid( "日本語" ) );
    CHECK( Utf8Valid( "👍🏽" ) );
    CHECK( Utf8Valid( "e\u0301" ) );
}

TEST_CASE( "Utf8Valid refuses what only looks like UTF-8" )
{
    CHECK_FALSE( Utf8Valid( "\x80" ) );             // a continuation byte alone
    CHECK_FALSE( Utf8Valid( "\xC0\xAF" ) );         // overlong '/'
    CHECK_FALSE( Utf8Valid( "\xED\xA0\x80" ) );     // a UTF-16 surrogate
    CHECK_FALSE( Utf8Valid( "\xF4\x90\x80\x80" ) ); // past U+10FFFF
    CHECK_FALSE( Utf8Valid( "\xE6\x97" ) );         // cut short
    CHECK_FALSE( Utf8Valid( "ok\xFF" ) );
}

TEST_CASE( "size() counts bytes, Utf8CodePoints counts code points" )
{
    CHECK( string( "Привет" ).size() == 12 );
    CHECK( Utf8CodePoints( "Привет" ) == 6 );
    CHECK( Utf8CodePoints( "日本" ) == 2 );
    // One character to a person, two code points: graphemes are the text
    // module's job.
    CHECK( Utf8CodePoints( "👍🏽" ) == 2 );
    CHECK( Utf8CodePoints( "e\u0301" ) == 2 );
}

TEST_CASE( "Utf8Next steps over bad bytes one at a time" )
{
    const string_view text = "a\xFF"
                             "b";
    size_t pos = 0;
    CHECK( Utf8Next( text, pos ) == U'a' );
    CHECK( Utf8Next( text, pos ) == cReplacementCharacter );
    CHECK( pos == 2 );
    CHECK( Utf8Next( text, pos ) == U'b' );
    CHECK( pos == text.size() );
}

TEST_CASE( "Utf8Append and Utf8Next round-trip every encoded length" )
{
    for ( const char32_t c : { U'\x7F', U'\x80', U'\x7FF', U'\x800', U'\xFFFF', U'\x10000', U'\x10FFFF' } )
    {
        string encoded;
        Utf8Append( encoded, c );
        CHECK( Utf8Valid( encoded ) );
        size_t pos = 0;
        CHECK( Utf8Next( encoded, pos ) == c );
        CHECK( pos == encoded.size() );
    }
    string surrogate;
    Utf8Append( surrogate, char32_t( 0xD800 ) );
    size_t pos = 0;
    CHECK( Utf8Next( surrogate, pos ) == cReplacementCharacter );
}

TEST_CASE( "Utf8Truncate never cuts a character in half" )
{
    CHECK( Utf8Truncate( "Привет", 5 ) == "Пр" );
    CHECK( Utf8Truncate( "Привет", 12 ) == "Привет" );
    CHECK( Utf8Truncate( "日本", 4 ) == "日" );
    CHECK( Utf8Truncate( "日本", 2 ).empty() );
    CHECK( Utf8Truncate( "abc", 2 ) == "ab" );
}

TEST_CASE( "AssetPath tidies separators and steps" )
{
    CHECK( AssetPath::From( "models\\cube.obj" )->View() == "models/cube.obj" );
    CHECK( AssetPath::From( "./models//props/./chair.glb" )->View() == "models/props/chair.glb" );
    CHECK( AssetPath::From( "модели/куб.obj" )->View() == "модели/куб.obj" );
    CHECK( AssetPath::From( "" )->Empty() );
}

TEST_CASE( "AssetPath refuses what is not a file inside the project" )
{
    CHECK_FALSE( AssetPath::From( "/etc/passwd" ) );
    CHECK_FALSE( AssetPath::From( "\\share\\x" ) );
    CHECK_FALSE( AssetPath::From( "C:/models/cube.obj" ) );
    CHECK_FALSE( AssetPath::From( "models/../../secret" ) );
    CHECK( AssetPath::From( "a/../b" ).error().find( "'..'" ) != string::npos );
    CHECK_FALSE( AssetPath::From( "bad\xFF.png" ) );
}

TEST_CASE( "AssetPath splits into its parts" )
{
    const AssetPath path = *AssetPath::From( "models/props/chair.glb" );
    CHECK( path.Filename() == "chair.glb" );
    CHECK( path.Stem() == "chair" );
    CHECK( path.Extension() == ".glb" );
    CHECK( path.Parent().View() == "models/props" );
    CHECK( path.Parent().Parent().Parent().Empty() );
    CHECK( path.Parent().Join( "table.glb" )->View() == "models/props/table.glb" );
    CHECK( AssetPath::From( ".gitignore" )->Extension().empty() );

    hset<AssetPath> set{ path };
    CHECK( set.contains( *AssetPath::From( "models\\props\\chair.glb" ) ) );
}

TEST_CASE( "Files with names in any script are written, listed and read back" )
{
    const OsPath root = fs::temp_directory_path() / "bubble_utf8_test";
    fs::remove_all( root );
    fs::create_directories( root );

    const vector<string> names = { "уровень.level", "日本語.txt", "emoji_👍🏽.json", "plain.txt" };
    for ( const string& name : names )
    {
        ofstream file( root / PathFromUtf8( name ) );
        REQUIRE( file );
        file << name;
    }

    set<string> listed;
    for ( const auto& entry : fs::directory_iterator( root ) )
        listed.insert( PathToUtf8( entry.path().filename() ) );
    CHECK( listed == set<string>( names.begin(), names.end() ) );

    for ( const string& name : names )
    {
        ifstream file( root / PathFromUtf8( name ) );
        string content;
        getline( file, content );
        CHECK( content == name );
    }

    // What a library does: fopen with a char path. This works on Windows
    // only because of the UTF-8 manifest.
    CHECK( ProcessUsesUtf8() );
    const string utf8Path = PathToUtf8( root / PathFromUtf8( "日本語.txt" ) );
    std::FILE* file = std::fopen( utf8Path.c_str(), "rb" );
    CHECK( file != nullptr );
    if ( file )
        std::fclose( file );

    fs::remove_all( root );
}

TEST_CASE( "The log keeps UTF-8 as it was given" )
{
    vector<LogEntry> entries;
    const u64 from = LogReadSince( 0, entries );
    LogInfo( "уровень {} загружен", "日本" );
    entries.clear();
    LogReadSince( from, entries );
    REQUIRE( entries.size() == 1 );
    CHECK( entries[0].mText == "уровень 日本 загружен" );
}
