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

TEST_CASE( "A byte that fits nowhere counts as one character of its own" )
{
    // One bad byte becomes one replacement character and the reader moves on,
    // so a broken text neither stalls the count nor swallows its neighbours.
    CHECK( Utf8CodePoints( "\xFF\xFF" ) == 2 );
    CHECK( Utf8CodePoints( "\xED\xA0\x80" ) == 3 );     // one surrogate is three bad bytes
    CHECK( Utf8CodePoints( "\xE0\x80\xAF" ) == 3 );     // an overlong three-byte form
    CHECK( Utf8CodePoints( "\xF4\x90\x80\x80" ) == 4 ); // past U+10FFFF: the lead plus three
    // A sequence cut short at the end of the text is bad from its lead byte on.
    CHECK( Utf8CodePoints( "\xE6\x97" ) == 2 );
    CHECK( Utf8CodePoints( "a\xE6\x97" ) == 3 );
    // NUL is a code point of its own, and a byte-order mark is a character too.
    CHECK( Utf8CodePoints( string_view( "a\0b", 3 ) ) == 3 );
    CHECK( Utf8Valid( string_view( "a\0b", 3 ) ) );
    size_t pos = 0;
    CHECK( Utf8Next( string_view( "a\0b", 3 ), pos ) == U'a' );
    CHECK( Utf8Next( string_view( "a\0b", 3 ), pos ) == char32_t( 0 ) );
    size_t bomPos = 0;
    CHECK( Utf8Next( "\xEF\xBB\xBF", bomPos ) == char32_t( 0xFEFF ) );
    CHECK( bomPos == 3 );
}

TEST_CASE( "Utf8Append turns what Unicode cannot use into the replacement character" )
{
    for ( const char32_t bad :
          { char32_t( 0xD800 ), char32_t( 0xDFFF ), char32_t( 0x110000 ), char32_t( 0x7FFFFFFF ) } )
    {
        string out;
        Utf8Append( out, bad );
        CHECK( out == "\xEF\xBF\xBD" );
        CHECK( Utf8Valid( out ) );
    }
    // The ends of each encoded length, kept whole.
    for ( const char32_t edge : { U'\x00', U'\x7F', U'\x80', U'\x7FF', U'\x800', U'\xFFFF', U'\x10FFFF' } )
    {
        string out;
        Utf8Append( out, edge );
        size_t pos = 0;
        CHECK( Utf8Next( out, pos ) == edge );
        CHECK( pos == out.size() );
    }
}

TEST_CASE( "Utf8Truncate cuts at a code point, never inside one" )
{
    // The byte at the cut is the first one dropped: when it continues the code
    // point before, that whole code point goes, even though room is left over.
    CHECK( Utf8Truncate( "\U0001F44D", 3 ).empty() );
    CHECK( Utf8Truncate( "a\U0001F44D", 2 ) == "a" );
    CHECK( Utf8Truncate( "日本", 5 ) == "日" );
    // A combining accent is a code point of its own, so the budget may end
    // right after the letter it sits on: a whole character as a person counts
    // it is Text's business, not this function's.
    const string_view accented = "e\u0301xy";
    CHECK( Utf8Truncate( accented, 3 ) == "e\u0301" );
    CHECK( Utf8Truncate( accented, 2 ) == "e" );
    CHECK( Utf8Valid( Utf8Truncate( accented, 2 ) ) );
    CHECK( Utf8Truncate( "abc", 0 ).empty() );
    CHECK( Utf8Truncate( "", 5 ).empty() );
    // What is already short comes back whole, and so does an invalid byte the
    // cut does not touch.
    const string broken = "a\xFF"
                          "bc";
    CHECK( Utf8Truncate( broken, 3 ) == broken.substr( 0, 3 ) );
    CHECK( Utf8Truncate( "日本", 6 ).size() == 6 );
}

TEST_CASE( "AssetPath tidies separators and steps" )
{
    CHECK( AssetPath::TryFrom( "models\\cube.obj" )->View() == "models/cube.obj" );
    CHECK( AssetPath::TryFrom( "./models//props/./chair.glb" )->View() == "models/props/chair.glb" );
    CHECK( AssetPath::TryFrom( "модели/куб.obj" )->View() == "модели/куб.obj" );
    CHECK( AssetPath::TryFrom( "" )->Empty() );
}

TEST_CASE( "AssetPath refuses what is not a file inside the project" )
{
    CHECK_FALSE( AssetPath::TryFrom( "/etc/passwd" ) );
    CHECK_FALSE( AssetPath::TryFrom( "\\share\\x" ) );
    CHECK_FALSE( AssetPath::TryFrom( "C:/models/cube.obj" ) );
    CHECK_FALSE( AssetPath::TryFrom( "models/../../secret" ) );
    CHECK( AssetPath::TryFrom( "a/../b" ).error().find( "'..'" ) != string::npos );
    CHECK_FALSE( AssetPath::TryFrom( "bad\xFF.png" ) );
}

TEST_CASE( "AssetPath splits into its parts" )
{
    const AssetPath path = *AssetPath::TryFrom( "models/props/chair.glb" );
    CHECK( path.Filename() == "chair.glb" );
    CHECK( path.Stem() == "chair" );
    CHECK( path.Extension() == ".glb" );
    CHECK( path.Parent().View() == "models/props" );
    CHECK( path.Parent().Parent().Parent().Empty() );
    CHECK( path.Parent().TryJoin( "table.glb" )->View() == "models/props/table.glb" );
    CHECK( AssetPath::TryFrom( ".gitignore" )->Extension().empty() );

    hset<AssetPath> set{ path };
    CHECK( set.contains( *AssetPath::TryFrom( "models\\props\\chair.glb" ) ) );
}

TEST_CASE( "AssetPath drops the empty steps whatever left them behind" )
{
    CHECK( AssetPath::TryFrom( "models/" )->View() == "models" );
    CHECK( AssetPath::TryFrom( "a//b" )->View() == "a/b" );
    CHECK( AssetPath::TryFrom( "a\\b/c\\d" )->View() == "a/b/c/d" );
    CHECK( AssetPath::TryFrom( "a/./b/./" )->View() == "a/b" );
    CHECK( AssetPath::TryFrom( "././" )->Empty() );
    CHECK( AssetPath::TryFrom( "//" ).error().find( "is absolute" ) != string::npos );
    // A step written with a backslash is the same step, and ".." is refused
    // however it is spelled.
    CHECK( AssetPath::TryFrom( "a\\..\\" ).error() == "'a\\..\\' leaves the project with '..'" );
    CHECK( AssetPath::TryFrom( ".." ).error() == "'..' leaves the project with '..'" );
}

TEST_CASE( "AssetPath says what is wrong in words a person can act on" )
{
    CHECK( AssetPath::TryFrom( "/etc/passwd" ).error() ==
           "'/etc/passwd' is absolute; asset paths start at the project's root" );
    CHECK( AssetPath::TryFrom( "C:/models/cube.obj" ).error().find( "is absolute" ) != string::npos );
    // A drive left without a path is still a drive: "C:" alone, and "C:x" next to it.
    CHECK_FALSE( AssetPath::TryFrom( "C:" ) );
    CHECK_FALSE( AssetPath::TryFrom( "C:x" ) );
    CHECK( AssetPath::TryFrom( "bad\xFF.png" ).error() == "the path is not valid UTF-8" );
    // The text of the error carries the path as it was written, so the person
    // sees which of their lines is meant.
    CHECK( AssetPath::TryFrom( "models\\..\\secret" ).error() == "'models\\..\\secret' leaves the project with '..'" );
}

TEST_CASE( "AssetPath reads a filename's parts by its last dot" )
{
    const AssetPath archive = *AssetPath::TryFrom( "sounds/archive.tar.gz" );
    CHECK( archive.Extension() == ".gz" );
    CHECK( archive.Stem() == "archive.tar" );
    // A leading dot belongs to the name, and dots in a directory say nothing
    // about the file under it.
    CHECK( AssetPath::TryFrom( "sounds/.gitignore" )->Stem() == ".gitignore" );
    CHECK( AssetPath::TryFrom( "a.dir/file" )->Extension().empty() );
    CHECK( AssetPath::TryFrom( "a.dir/file" )->Stem() == "file" );
    // A name that ends in a dot keeps that dot as its extension.
    CHECK( AssetPath::TryFrom( "notes." )->Extension() == "." );
    CHECK( AssetPath::TryFrom( "notes." )->Stem() == "notes" );
    // A directory has no extension of its own; neither has the root.
    CHECK( AssetPath::TryFrom( "models/" )->Filename() == "models" );
    CHECK( AssetPath::TryFrom( "models/" )->Extension().empty() );
    CHECK( AssetPath::TryFrom( "cube.obj" )->Parent().Empty() );
    CHECK( AssetPath().Parent().Empty() );
    CHECK( AssetPath::TryFrom( "models/sub/" )->Parent().View() == "models" );
}

TEST_CASE( "TryJoin puts a path inside another and keeps the rules" )
{
    const AssetPath props = *AssetPath::TryFrom( "models/props" );
    CHECK( props.TryJoin( "table.glb" )->View() == "models/props/table.glb" );
    CHECK( props.TryJoin( "shelf\\x.glb" )->View() == "models/props/shelf/x.glb" );
    CHECK( props.TryJoin( "./x.glb" )->View() == "models/props/x.glb" );
    CHECK( props.TryJoin( "" )->View() == "models/props" );
    // The joined text is what gets the rules, so a separator the relative
    // path starts with lands in the middle and only reads as one more step:
    // it nests, it does not escape, and ".." is still refused.
    CHECK( props.TryJoin( "/etc/passwd" )->View() == "models/props/etc/passwd" );
    CHECK_FALSE( props.TryJoin( "/../../secret" ) );
    CHECK_FALSE( props.TryJoin( "../secret" ) );
    CHECK( props.TryJoin( "a//b/" )->View() == "models/props/a/b" );
    CHECK( AssetPath().TryJoin( "models/cube.obj" )->View() == "models/cube.obj" );
    CHECK_FALSE( AssetPath().TryJoin( ".." ) );
    CHECK( AssetPath().TryJoin( "" )->Empty() );
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
