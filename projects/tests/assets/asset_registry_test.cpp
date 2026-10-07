// The asset registry: one entry per file, refs to entries, reloads into the
// same entry. Synchronous for now; the calls are the ones loading in the
// background will keep.
#include "bubble/assets/asset_registry.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/types/filesystem.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

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
    hmap<string, string> mFiles;
    AssetRegistry mAssets{ [this]( const AssetPath& path ) -> expected<string, string> {
        const auto found = mFiles.find( path.View() );
        if ( found == mFiles.end() )
            return unexpected( "no such file"s );
        return found->second;
    } };

    Project()
    {
        mAssets.RegisterImporter<TextAsset>( ".txt", "text", []( string_view bytes, const AssetPath& ) {
            if ( bytes == "broken" )
                return expected<TextAsset, string>( unexpected( "cannot read it"s ) );
            return expected<TextAsset, string>( TextAsset{ string( bytes ) } );
        } );
        mAssets.RegisterImporter<NumberAsset>( ".num", "number", []( string_view bytes, const AssetPath& ) {
            return expected<NumberAsset, string>( NumberAsset{ static_cast<int>( bytes.size() ) } );
        } );
    }

    template <typename T = TextAsset>
    AssetRef<T> Load( string_view path )
    {
        return mAssets.Load<T>( *AssetPath::From( path ) );
    }
};
}

TEST_CASE( "A loaded asset is Ready, and the same path is the same entry" )
{
    Project project;
    project.mFiles["notes/hello.txt"] = "hello";
    const auto hello = project.Load( "notes/hello.txt" );
    REQUIRE( hello.Ready() );
    CHECK( hello.Get()->mText == "hello" );
    CHECK( hello.Entry().Path().View() == "notes/hello.txt" );
    CHECK( hello.Entry().Id() == AssetId::MakeFrom( "notes/hello.txt" ) );
    CHECK( project.Load( "notes/hello.txt" ) == hello );

    // Find never loads, by path or by id.
    CHECK( project.mAssets.Find<TextAsset>( *AssetPath::From( "notes/hello.txt" ) ) == hello );
    CHECK( project.mAssets.Find<TextAsset>( hello.Entry().Id() ) == hello );
    project.mFiles["notes/other.txt"] = "other";
    CHECK_FALSE( project.mAssets.Find<TextAsset>( *AssetPath::From( "notes/other.txt" ) ) );
    // Nor finds an asset as another kind.
    CHECK_FALSE( project.mAssets.Find<NumberAsset>( *AssetPath::From( "notes/hello.txt" ) ) );
}

TEST_CASE( "An asset that cannot load fails with the reason" )
{
    Project project;
    project.mFiles["bad.txt"] = "broken";
    project.mFiles["image.png"] = "";
    project.mFiles["count.num"] = "123";
    const auto missing = project.Load( "missing.txt" );
    CHECK( missing.State() == AssetState::Failed );
    CHECK( missing.Entry().Error() == "no such file" );
    CHECK_FALSE( missing.Get() );

    CHECK( project.Load( "bad.txt" ).Entry().Error() == "cannot read it" );
    CHECK( project.Load( "image.png" ).Entry().Error() == "no importer for '.png' files" );
    CHECK( project.Load( "count.num" ).Entry().Error() == "a number file, asked for as something else" );

    // A file in memory as one kind, asked for as another.
    project.mFiles["notes.txt"] = "x";
    const auto text = project.Load( "notes.txt" );
    const auto asNumber = project.Load<NumberAsset>( "notes.txt" );
    CHECK( asNumber.State() == AssetState::Failed );
    CHECK( text.Ready() );
}

TEST_CASE( "A reload puts the new version into the same entry and tells the listeners" )
{
    Project project;
    project.mFiles["a.txt"] = "first";
    const auto a = project.Load( "a.txt" );
    vector<string> heard;
    const auto listener =
        project.mAssets.OnChanged( [&]( const AssetEntryBase& entry ) { heard.push_back( entry.Path().String() ); } );

    project.mFiles["a.txt"] = "second";
    REQUIRE( project.mAssets.Reload( *AssetPath::From( "a.txt" ) ) );
    CHECK( a.Get()->mText == "second" );
    CHECK( a.Entry().Version() == 1 );
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

TEST_CASE( "An entry lives as long as a ref to it" )
{
    Project project;
    project.mFiles["a.txt"] = "a";
    {
        const auto a = project.Load( "a.txt" );
        const auto copy = a;
        // A file asked for as the wrong kind is not listed.
        const auto wrong = project.Load<NumberAsset>( "a.txt" );
        REQUIRE( project.mAssets.Find<TextAsset>( *AssetPath::From( "a.txt" ) ) );
        CHECK( project.mAssets.Count() == 1 );
    }
    CHECK_FALSE( project.mAssets.Find<TextAsset>( *AssetPath::From( "a.txt" ) ) );
    CHECK_FALSE( project.mAssets.Find<TextAsset>( AssetId::MakeFrom( "a.txt" ) ) );
    // Its entries went with it.
    CHECK( project.mAssets.Count() == 0 );

    // Loaded again, it is a new entry.
    const auto again = project.Load( "a.txt" );
    CHECK( again.Ready() );
    CHECK( project.mAssets.Count() == 1 );
}

TEST_CASE( "An asset ref may outlive the registry" )
{
    AssetRef<TextAsset> kept;
    {
        Project project;
        project.mFiles["a.txt"] = "a";
        kept = project.Load( "a.txt" );
    }
    CHECK( kept.Get()->mText == "a" );
}

TEST_CASE( "An empty asset ref has no entry to ask" )
{
    const AssetRef<TextAsset> empty;
    CHECK_FALSE( empty );
    CHECK( empty.State() == AssetState::Failed );
    CHECK_FALSE( empty.Get() );
    CHECK_THROWS_WITH_AS( empty.Entry(), "Entry() of an empty asset ref", logic_error );
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
    const auto root = fs::temp_directory_path() / PathFromUtf8( "bubble_assets_проект" );
    fs::create_directories( root / PathFromUtf8( "заметки" ) );
    ofstream( root / PathFromUtf8( "заметки/привет.txt" ), ios::binary ) << "привет";

    AssetRegistry assets( AssetRegistry::FromDirectory( root ) );
    assets.RegisterImporter<TextAsset>( ".txt", "text", []( string_view bytes, const AssetPath& ) {
        return expected<TextAsset, string>( TextAsset{ string( bytes ) } );
    } );
    const auto text = assets.Load<TextAsset>( *AssetPath::From( "заметки/привет.txt" ) );
    REQUIRE( text.Ready() );
    CHECK( text.Get()->mText == "привет" );
    fs::remove_all( root );
}
