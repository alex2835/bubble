// The asset registry: one entry per asset, found through the catalog and
// read from what the importer made of it, refs to entries, reloads into the
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

// A catalog, the import cache in memory - by the path an imported file has -
// a "text" loader that refuses "broken", and a "number" one.
struct Project
{
    hmap<string, string> mImported;
    AssetCatalog mCatalog;
    AssetRegistry mAssets{ mCatalog, [this]( const AssetId& assetId, string_view kind ) -> expected<string, string> {
                              const auto found = mImported.find( ImportedPath( assetId, kind ).View() );
                              if ( found == mImported.end() )
                                  return unexpected( "no such file"s );
                              return found->second;
                          } };

    Project()
    {
        mAssets.RegisterLoader<TextAsset>( "text", []( string_view bytes, const AssetPath& ) {
            if ( bytes == "broken" )
                return expected<TextAsset, string>( unexpected( "cannot read it"s ) );
            return expected<TextAsset, string>( TextAsset{ string( bytes ) } );
        } );
        mAssets.RegisterLoader<NumberAsset>( "number", []( string_view bytes, const AssetPath& ) {
            return expected<NumberAsset, string>( NumberAsset{ static_cast<int>( bytes.size() ) } );
        } );
    }

    // An asset of the project, in the catalog as a scan would put it, and
    // imported as `kind` with `bytes` - again, if it was.
    AssetId Add( string_view path, string_view bytes, string_view kind = "text" )
    {
        const AssetId assetId = Catalog( path );
        mImported[ImportedPath( assetId, kind ).String()] = string( bytes );
        return assetId;
    }

    // In the catalog, never imported.
    AssetId Catalog( string_view path )
    {
        const AssetPath assetPath = *AssetPath::TryFrom( path );
        if ( const auto assetIdMaybe = mCatalog.TryIdOf( assetPath ) )
            return *assetIdMaybe;
        const AssetId assetId = AssetId::Generate();
        mCatalog.Add( assetId, assetPath );
        return assetId;
    }

    template <typename T = TextAsset>
    AssetRef<T> Load( string_view path )
    {
        return mAssets.Load<T>( *AssetPath::TryFrom( path ) );
    }
};
}

TEST_CASE( "A loaded asset is Ready, and the same path is the same entry" )
{
    Project project;
    const AssetId helloId = project.Add( "notes/hello.txt", "hello" );
    const auto hello = project.Load( "notes/hello.txt" );
    REQUIRE( hello.Ready() );
    CHECK( hello.TryGet()->mText == "hello" );
    CHECK( hello.Entry().Path().View() == "notes/hello.txt" );
    CHECK( hello.Entry().Id() == helloId );
    CHECK( project.Load( "notes/hello.txt" ) == hello );
    // By id, the same entry.
    CHECK( project.mAssets.Load<TextAsset>( helloId ) == hello );

    // TryFind never loads, by path or by id.
    CHECK( project.mAssets.TryFind<TextAsset>( *AssetPath::TryFrom( "notes/hello.txt" ) ) == hello );
    CHECK( project.mAssets.TryFind<TextAsset>( hello.Entry().Id() ) == hello );
    project.Add( "notes/other.txt", "other" );
    CHECK_FALSE( project.mAssets.TryFind<TextAsset>( *AssetPath::TryFrom( "notes/other.txt" ) ) );
    // Nor finds an asset as another kind.
    CHECK_FALSE( project.mAssets.TryFind<NumberAsset>( *AssetPath::TryFrom( "notes/hello.txt" ) ) );
}

TEST_CASE( "An asset that cannot load fails with the reason" )
{
    Project project;
    project.Add( "bad.txt", "broken" );
    project.Add( "count.num", "123", "number" );
    project.Catalog( "missing.txt" );
    const auto missing = project.Load( "missing.txt" );
    CHECK( missing.State() == AssetState::Failed );
    CHECK( missing.Entry().Error() == "no imported text: no such file" );
    CHECK_FALSE( missing.TryGet() );

    CHECK( project.Load( "bad.txt" ).Entry().Error() == "cannot read it" );
    // Imported as another kind: none of this kind.
    CHECK( project.Load( "count.num" ).Entry().Error() == "no imported text: no such file" );
    CHECK( project.Load<NumberAsset>( "count.num" ).TryGet()->mValue == 3 );

    // Not in the catalog, by path or by id: nothing to load, and nothing kept.
    const auto stray = project.Load( "stray.txt" );
    CHECK( stray.Entry().Error() == "stray.txt is not an asset of the project" );
    const AssetId unknown = AssetId::Generate();
    CHECK( project.mAssets.Load<TextAsset>( unknown ).Entry().Error() ==
           "no asset " + unknown.ToString() + " in the project" );
    CHECK_FALSE( project.mAssets.TryFind<TextAsset>( *AssetPath::TryFrom( "stray.txt" ) ) );

    // A file in memory as one kind, asked for as another.
    project.Add( "notes.txt", "x" );
    const auto text = project.Load( "notes.txt" );
    const auto asNumber = project.Load<NumberAsset>( "notes.txt" );
    CHECK( asNumber.State() == AssetState::Failed );
    CHECK( text.Ready() );
}

TEST_CASE( "A reload puts the new version into the same entry and tells the listeners" )
{
    Project project;
    project.Add( "a.txt", "first" );
    const auto a = project.Load( "a.txt" );
    vector<string> heard;
    const auto listener =
        project.mAssets.OnChanged( [&]( const AssetEntryBase& entry ) { heard.push_back( entry.Path().String() ); } );

    project.Add( "a.txt", "second" );
    REQUIRE( project.mAssets.TryReload( *AssetPath::TryFrom( "a.txt" ) ) );
    CHECK( a.TryGet()->mText == "second" );
    CHECK( a.Entry().Version() == 1 );
    CHECK( heard == vector<string>{ "a.txt" } );

    // A failed reload keeps the version it had and tells nobody.
    project.Add( "a.txt", "broken" );
    CHECK_FALSE( project.mAssets.TryReload( *AssetPath::TryFrom( "a.txt" ) ) );
    CHECK( a.TryGet()->mText == "second" );
    CHECK( heard.size() == 1 );

    // A file nobody holds is not reloaded.
    project.Add( "b.txt", "b" );
    CHECK( project.mAssets.TryReload( *AssetPath::TryFrom( "b.txt" ) ) );
    CHECK( heard.size() == 1 );

    project.mAssets.RemoveListener( listener );
    project.Add( "a.txt", "third" );
    REQUIRE( project.mAssets.TryReload( *AssetPath::TryFrom( "a.txt" ) ) );
    CHECK( heard.size() == 1 );
}

TEST_CASE( "An entry lives as long as a ref to it" )
{
    Project project;
    const AssetId aId = project.Add( "a.txt", "a" );
    {
        const auto a = project.Load( "a.txt" );
        const auto copy = a;
        // A file asked for as the wrong kind is not listed.
        const auto wrong = project.Load<NumberAsset>( "a.txt" );
        REQUIRE( project.mAssets.TryFind<TextAsset>( *AssetPath::TryFrom( "a.txt" ) ) );
        CHECK( project.mAssets.Count() == 1 );
    }
    CHECK_FALSE( project.mAssets.TryFind<TextAsset>( *AssetPath::TryFrom( "a.txt" ) ) );
    CHECK_FALSE( project.mAssets.TryFind<TextAsset>( aId ) );
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
        project.Add( "a.txt", "a" );
        kept = project.Load( "a.txt" );
    }
    CHECK( kept.TryGet()->mText == "a" );
}

TEST_CASE( "An empty asset ref has no entry to ask" )
{
    const AssetRef<TextAsset> empty;
    CHECK_FALSE( empty );
    CHECK( empty.State() == AssetState::Failed );
    CHECK_FALSE( empty.TryGet() );
    CHECK_THROWS_WITH_AS( empty.Entry(), "Entry() of an empty asset ref", logic_error );
}

TEST_CASE( "A new asset id is random, written as 32 hex digits and read back" )
{
    const AssetId id = AssetId::Generate();
    CHECK( id );
    CHECK( id != AssetId::Generate() );
    CHECK( id.ToString().size() == 32 );
    CHECK( AssetId::TryParse( id.ToString() ) == id );
    CHECK_FALSE( AssetId::TryParse( "not an id" ) );
    CHECK_FALSE( AssetId::TryParse( string( 32, 'g' ) ) );
}

TEST_CASE( "An asset id is read only as 32 lowercase hex digits" )
{
    // An id of all zeros is the same id as "no asset", and a file may hold it.
    CHECK( AssetId{}.ToString() == string( 32, '0' ) );
    const auto zeroMaybe = AssetId::TryParse( string( 32, '0' ) );
    REQUIRE( zeroMaybe );
    CHECK( *zeroMaybe == AssetId{} );
    CHECK_FALSE( *zeroMaybe );
    // Only lowercase hex, and only 32 of it: a GUID written the usual way, with
    // capitals, hyphens or spaces, is not an id.
    CHECK_FALSE( AssetId::TryParse( string( 31, '0' ) ) );
    CHECK_FALSE( AssetId::TryParse( string( 33, '0' ) ) );
    CHECK_FALSE( AssetId::TryParse( string( 32, '0' ) + " " ) );
    CHECK( AssetId::TryParse( string( 16, '0' ) + "abcdef0123456789" ) );
    CHECK_FALSE( AssetId::TryParse( "0123456789ABCDEF0123456789ABCDEF" ) );
    CHECK_FALSE( AssetId::TryParse( "0123456789abcdef0123456789abcdeg" ) );
}

TEST_CASE( "A file that would not load is kept in memory and says the same reason again" )
{
    Project project;
    const AssetId laterId = project.Catalog( "later.txt" );
    const auto missing = project.Load( "later.txt" );
    CHECK( missing.State() == AssetState::Failed );
    // A failed entry is an entry: it is listed, counted, and found by both keys.
    CHECK( project.mAssets.Count() == 1 );
    CHECK( project.mAssets.TryFind<TextAsset>( *AssetPath::TryFrom( "later.txt" ) ) == missing );
    CHECK( project.mAssets.TryFind<TextAsset>( laterId ) == missing );

    // The file appears. Loading is not looked at again: the entry that failed is
    // still the one in memory, and it keeps its old reason.
    project.Add( "later.txt", "here" );
    const auto again = project.Load( "later.txt" );
    CHECK( again == missing );
    CHECK_FALSE( again.Ready() );
    CHECK( again.Entry().Error() == "no imported text: no such file" );
    // A reload is what notices: the same entry takes the file and becomes Ready.
    REQUIRE( project.mAssets.TryReload( *AssetPath::TryFrom( "later.txt" ) ) );
    CHECK( missing.Ready() );
    CHECK( missing.TryGet()->mText == "here" );
    CHECK( missing.Entry().Version() == 1 );
    CHECK( missing.Entry().Error().empty() );
    // And when the last ref goes, the count goes with it.
    project.Catalog( "gone.txt" );
    {
        const auto once = project.Load( "gone.txt" );
        CHECK( project.mAssets.Count() == 2 );
    }
    CHECK( project.mAssets.Count() == 1 );
    CHECK_FALSE( project.mAssets.TryFind<TextAsset>( *AssetPath::TryFrom( "gone.txt" ) ) );
}

TEST_CASE( "A file already in memory asked for as another kind says which one it is" )
{
    Project project;
    project.Add( "a.txt", "first" );
    const auto text = project.Load( "a.txt" );
    const auto asNumber = project.Load<NumberAsset>( "a.txt" );
    CHECK( asNumber.State() == AssetState::Failed );
    CHECK( asNumber.Entry().Error() == "a.txt is loaded as another kind of asset" );
    CHECK( asNumber.Entry().Id() == text.Entry().Id() );
    // The ref of the kind a person mistyped is not listed: the entry that is
    // there stays the one and only, and a reload looks at it.
    CHECK( project.mAssets.Count() == 1 );
    CHECK_FALSE( project.mAssets.TryFind<NumberAsset>( *AssetPath::TryFrom( "a.txt" ) ) );
    CHECK( project.mAssets.TryFind<TextAsset>( *AssetPath::TryFrom( "a.txt" ) ) == text );
    // A file with nothing in it loads as empty text.
    project.Add( "empty.txt", "" );
    CHECK( project.Load( "empty.txt" ).TryGet()->mText == "" );
}

TEST_CASE( "Two spellings of one path are two assets the registry does not connect" )
{
    Project project;
    project.Add( "notes/hello.txt", "one" );
    project.Add( "Notes/Hello.txt", "one" );
    const auto lower = project.Load( "notes/hello.txt" );
    const auto mixed = project.Load( "Notes/Hello.txt" );
    // On Windows and on a web fetch these are the same file read twice: two
    // entries, two ids, two imports, and a reload of one leaves the other alone.
    CHECK( lower.Ready() );
    CHECK( mixed.Ready() );
    CHECK( lower != mixed );
    CHECK( lower.Entry().Id() != mixed.Entry().Id() );
    CHECK( project.mAssets.Count() == 2 );
    vector<string> heard;
    project.mAssets.OnChanged( [&]( const AssetEntryBase& entry ) { heard.push_back( entry.Path().String() ); } );
    project.Add( "notes/hello.txt", "two" );
    REQUIRE( project.mAssets.TryReload( *AssetPath::TryFrom( "notes/hello.txt" ) ) );
    CHECK( heard == vector<string>{ "notes/hello.txt" } );
    CHECK( mixed.TryGet()->mText == "one" );
    // Found by id is the entry of that spelling, not the other one.
    CHECK( project.mAssets.TryFind<TextAsset>( mixed.Entry().Id() ) == mixed );
}

TEST_CASE( "A listener added while the listeners run starts hearing the reload after" )
{
    Project project;
    project.Add( "a.txt", "a" );
    // The ref is kept: with nothing holding the entry, there is nothing to
    // reload and no listener has anything to hear.
    const auto kept = project.Load( "a.txt" );
    CHECK( kept.Ready() );
    vector<string> heard;
    AssetListenerHandle second;
    project.mAssets.OnChanged( [&]( const AssetEntryBase& ) {
        heard.push_back( "first" );
        if ( not second )
            second = project.mAssets.OnChanged( [&]( const AssetEntryBase& ) { heard.push_back( "second" ); } );
    } );

    // The listeners were listed before any of them ran, so the one the first
    // added speaks only to the next reload.
    project.Add( "a.txt", "b" );
    REQUIRE( project.mAssets.TryReload( *AssetPath::TryFrom( "a.txt" ) ) );
    CHECK( heard == vector<string>{ "first" } );
    REQUIRE( project.mAssets.TryReload( *AssetPath::TryFrom( "a.txt" ) ) );
    CHECK( heard == vector<string>{ "first", "first", "second" } );

    // A listener may take itself off from inside itself: it was already copied.
    AssetListenerHandle handle;
    handle = project.mAssets.OnChanged( [&]( const AssetEntryBase& ) {
        heard.push_back( "once" );
        project.mAssets.RemoveListener( handle );
    } );
    REQUIRE( project.mAssets.TryReload( *AssetPath::TryFrom( "a.txt" ) ) );
    CHECK( heard.back() == "once" );
    CHECK( heard == vector<string>{ "first", "first", "second", "first", "second", "once" } );
    REQUIRE( project.mAssets.TryReload( *AssetPath::TryFrom( "a.txt" ) ) );
    CHECK( heard.back() == "second" );
    CHECK( heard.size() == 8 );
    // A reload nobody listens to is still a reload, and the version grows.
    const auto entry = project.mAssets.TryFind<TextAsset>( *AssetPath::TryFrom( "a.txt" ) );
    CHECK( entry.Entry().Version() == 4 );
}

TEST_CASE( "The registry reads a project's import cache, names in any script included" )
{
    const auto root = fs::temp_directory_path() / PathFromUtf8( "bubble_assets_проект" );
    AssetCatalog catalog;
    const AssetId id = AssetId::Generate();
    catalog.Add( id, *AssetPath::TryFrom( "заметки/привет.txt" ) );
    CHECK( ImportedPath( id, "text" ).View() == ".bubble/imported/" + id.ToString() + ".text" );
    const OsPath imported = ImportedPath( id, "text" ).ToOsPath( root );
    fs::create_directories( imported.parent_path() );
    ofstream( imported, ios::binary ) << "привет";

    AssetRegistry assets( catalog, AssetRegistry::FromDirectory( root ) );
    assets.RegisterLoader<TextAsset>( "text", []( string_view bytes, const AssetPath& ) {
        return expected<TextAsset, string>( TextAsset{ string( bytes ) } );
    } );
    const auto text = assets.Load<TextAsset>( *AssetPath::TryFrom( "заметки/привет.txt" ) );
    REQUIRE( text.Ready() );
    CHECK( text.TryGet()->mText == "привет" );
    fs::remove_all( root );
}

TEST_CASE( "A type has one loader, and a type with none is a bug" )
{
    Project project;
    CHECK_THROWS_AS( project.mAssets.RegisterLoader<TextAsset>(
                         "text", []( string_view, const AssetPath& ) { return expected<TextAsset, string>(); } ),
                     logic_error );
    struct Unloaded
    {
    };
    project.Add( "a.txt", "a" );
    CHECK_THROWS_AS( project.Load<Unloaded>( "a.txt" ), logic_error );
}
