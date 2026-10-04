// The asset registry: one slot per file, handles to slots, reloads into the
// same slot. Synchronous for now; the calls are the ones loading in the
// background will keep.
#include "bubble/assets/asset_registry.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include <doctest.h>
#include <filesystem>
#include <fstream>

using namespace bubble;

namespace
{
struct TextAsset
{
    string mText;
};

struct NumberAsset
{
    int mValue = 0;
};

// Files in memory, a .txt importer that refuses "broken", a .num one.
struct Project
{
    str_hmap<string> mFiles;
    AssetRegistry mAssets{ [this]( const AssetPath& path ) -> expected<string, string> {
        const auto found = mFiles.find( path.View() );
        if ( found == mFiles.end() )
            return std::unexpected( "no such file"s );
        return found->second;
    } };

    Project()
    {
        mAssets.RegisterImporter<TextAsset>( ".txt", "text", []( string_view bytes, const AssetPath& ) {
            if ( bytes == "broken" )
                return expected<TextAsset, string>( std::unexpected( "cannot read it"s ) );
            return expected<TextAsset, string>( TextAsset{ string( bytes ) } );
        } );
        mAssets.RegisterImporter<NumberAsset>( ".num", "number", []( string_view bytes, const AssetPath& ) {
            return expected<NumberAsset, string>( NumberAsset{ static_cast<int>( bytes.size() ) } );
        } );
    }

    template <typename T = TextAsset>
    AssetHandle<T> Load( string_view path )
    {
        return mAssets.Load<T>( *AssetPath::From( path ) );
    }
};
}

TEST_CASE( "A loaded asset is Ready, and the same path is the same slot" )
{
    Project project;
    project.mFiles["notes/hello.txt"] = "hello";
    const auto hello = project.Load( "notes/hello.txt" );
    REQUIRE( hello.Ready() );
    CHECK( hello.Get()->mText == "hello" );
    CHECK( hello.Slot().Path().View() == "notes/hello.txt" );
    CHECK( hello.Slot().Id() == AssetId::MakeFrom( "notes/hello.txt" ) );
    CHECK( project.Load( "notes/hello.txt" ) == hello );

    // Find never loads, by path or by id.
    CHECK( project.mAssets.Find<TextAsset>( *AssetPath::From( "notes/hello.txt" ) ) == hello );
    CHECK( project.mAssets.Find<TextAsset>( hello.Slot().Id() ) == hello );
    project.mFiles["notes/other.txt"] = "other";
    CHECK_FALSE( project.mAssets.Find<TextAsset>( *AssetPath::From( "notes/other.txt" ) ) );
    // Nor finds an asset as another kind.
    CHECK_FALSE( project.mAssets.Find<NumberAsset>( *AssetPath::From( "notes/hello.txt" ) ) );
}

TEST_CASE( "An asset that cannot load fails with the reason, and says so in the log" )
{
    Project project;
    project.mFiles["bad.txt"] = "broken";
    project.mFiles["image.png"] = "";
    project.mFiles["count.num"] = "123";
    const auto missing = project.Load( "missing.txt" );
    CHECK( missing.State() == AssetState::Failed );
    CHECK( missing.Slot().Error() == "no such file" );
    CHECK_FALSE( missing.Get() );

    CHECK( project.Load( "bad.txt" ).Slot().Error() == "cannot read it" );
    CHECK( project.Load( "image.png" ).Slot().Error() == "no importer for '.png' files" );
    CHECK( project.Load( "count.num" ).Slot().Error() == "a number file, asked for as something else" );

    // A file in memory as one kind, asked for as another.
    project.mFiles["notes.txt"] = "x";
    const auto text = project.Load( "notes.txt" );
    const auto asNumber = project.Load<NumberAsset>( "notes.txt" );
    CHECK( asNumber.State() == AssetState::Failed );
    CHECK( text.Ready() );
}

TEST_CASE( "A reload puts the new version into the same slot and tells the listeners" )
{
    Project project;
    project.mFiles["a.txt"] = "first";
    const auto a = project.Load( "a.txt" );
    vector<string> heard;
    const auto listener =
        project.mAssets.OnChanged( [&]( const AssetSlotBase& slot ) { heard.push_back( slot.Path().String() ); } );

    project.mFiles["a.txt"] = "second";
    REQUIRE( project.mAssets.Reload( *AssetPath::From( "a.txt" ) ) );
    CHECK( a.Get()->mText == "second" );
    CHECK( a.Slot().Version() == 1 );
    CHECK( heard == vector<string>{ "a.txt" } );

    // A failed reload keeps the version it had and tells nobody.
    project.mFiles["a.txt"] = "broken";
    CHECK_FALSE( project.mAssets.Reload( *AssetPath::From( "a.txt" ) ) );
    CHECK( a.Get()->mText == "second" );
    CHECK( heard.size() == 1 );

    // A file nobody holds is not reloaded.
    project.mFiles["b.txt"] = "b";
    CHECK( project.mAssets.Reload( *AssetPath::From( "b.txt" ) ) );
    CHECK( heard.size() == 1 );

    project.mAssets.RemoveListener( listener );
    project.mFiles["a.txt"] = "third";
    REQUIRE( project.mAssets.Reload( *AssetPath::From( "a.txt" ) ) );
    CHECK( heard.size() == 1 );
}

TEST_CASE( "A slot lives as long as a handle to it" )
{
    Project project;
    project.mFiles["a.txt"] = "a";
    {
        const auto a = project.Load( "a.txt" );
        const auto copy = a;
        REQUIRE( project.mAssets.Find<TextAsset>( *AssetPath::From( "a.txt" ) ) );
    }
    CHECK_FALSE( project.mAssets.Find<TextAsset>( *AssetPath::From( "a.txt" ) ) );
}

TEST_CASE( "An asset id is written as 32 hex digits and read back" )
{
    const AssetId id = AssetId::MakeFrom( "models/hero.glb" );
    CHECK( id );
    CHECK( id == AssetId::MakeFrom( "models/hero.glb" ) );
    CHECK( id != AssetId::MakeFrom( "models/hero2.glb" ) );
    CHECK( id.ToString().size() == 32 );
    CHECK( AssetId::Parse( id.ToString() ) == id );
    CHECK_FALSE( AssetId::Parse( "not an id" ) );
    CHECK_FALSE( AssetId::Parse( string( 32, 'g' ) ) );
}

TEST_CASE( "The registry reads a project directory, names in any script included" )
{
    const auto root = std::filesystem::temp_directory_path() / PathFromUtf8( "bubble_assets_проект" );
    std::filesystem::create_directories( root / PathFromUtf8( "заметки" ) );
    std::ofstream( root / PathFromUtf8( "заметки/привет.txt" ), std::ios::binary ) << "привет";

    AssetRegistry assets( AssetRegistry::FromDirectory( root ) );
    assets.RegisterImporter<TextAsset>( ".txt", "text", []( string_view bytes, const AssetPath& ) {
        return expected<TextAsset, string>( TextAsset{ string( bytes ) } );
    } );
    const auto text = assets.Load<TextAsset>( *AssetPath::From( "заметки/привет.txt" ) );
    REQUIRE( text.Ready() );
    CHECK( text.Get()->mText == "привет" );
    std::filesystem::remove_all( root );
}
