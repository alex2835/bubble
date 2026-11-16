// The script importer: a script's props and requires read from its source
// without running it, and - given bubble.d.luau - its types checked
// strictly, the way the doc's «Только типизированный код» says.
#include "bubble/import/script_importer.hpp"
#include "bubble/scripts/script_asset.hpp"
#include "bubble/scripts/script_declarations.hpp"
#include "scripts/script_world.hpp"
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

namespace
{
// A project's scripts in memory, imported with a check made from the
// script world's App: its components, services and the given actions.
struct Imports
{
    ScriptWorld mWorld;
    hmap<string, string> mSources;
    ScriptCheck mCheck;

    explicit Imports( bool checked = true, vector<string> actions = { "jump", "move_x" } )
    {
        if ( checked )
            mCheck.mDeclarations =
                WriteScriptDeclarations( mWorld.mApp, ScriptLabels{ .mActions = std::move( actions ) } );
        mCheck.mAliases = { { "lib", "scripts/lib" } };
        mCheck.mRead = [this]( const AssetPath& path ) -> expected<string, string> {
            const auto found = mSources.find( path.String() );
            if ( found == mSources.end() )
                return unexpected( "no such file"s );
            return found->second;
        };
        mCheck.mAssetOf = []( const AssetPath& path ) {
            return path.View() == "models/hero.glb" ? *AssetId::TryParse( "9c1e0f3a5b7d4e2c8a6f1b3d5e7c9a0b" )
                                                    : AssetId{};
        };
    }

    expected<ScriptAsset, string> TryImport( string_view path, string_view source )
    {
        mSources[string( path )] = string( source );
        const Importer importer = ScriptImporter( mCheck );
        auto bytesMaybe = importer.mImport( source, Json(), *AssetPath::TryFrom( path ) );
        if ( not bytesMaybe )
            return unexpected( std::move( bytesMaybe.error() ) );
        return TryReadScriptAsset( *bytesMaybe );
    }

    string Error( string_view path, string_view source )
    {
        auto importedMaybe = TryImport( path, source );
        REQUIRE_FALSE( importedMaybe );
        return importedMaybe.error();
    }
};

constexpr string_view cTyped = R"(
local P = props { speed = 4 }
local L = locals { ticks = 0 }
type Self = typeof( P ) & typeof( L ) & { entity: entity }

function on_fixed_update( self: Self, dt: number )
    self.ticks += 1
    local t = self.entity.transform
    t.position += vector.create( self.speed * dt, 0, 0 )
    if input.pressed( "jump" ) then
        debug_draw.line( t.position, t.position + vector.one, color( 1, 0, 0 ) )
    end
end
)";
}

TEST_CASE( "A script's props are read from its source - defaults, types, hints - and its requires resolved" )
{
    Imports imports( false );
    const auto assetMaybe = imports.TryImport( "scripts/door.luau", R"(
local P = props {
    speed = prop( 40, { min = 0, max = 100, tooltip = "How fast it swings" } ),
    label = "door",
    open = false,
    hinge = vector.create( 1, 2, -3 ),
    tint = color( 1, 0.5, 0 ),
    model = asset( "models/hero.glb" ),
    target = entity(),
    stops = { vector.zero, vector.one },
    marks = {} :: { number },
    wave = { count = 3, delay = 0.5 },
}
local inventory = require( "@lib/inventory" )
local tuning = require( "./tuning" )
local again = require( "@lib/inventory" )
)" );
    REQUIRE_MESSAGE( assetMaybe, assetMaybe.error() );
    const vector<ScriptProp>& props = assetMaybe->mProps;
    REQUIRE( props.size() == 10 );
    CHECK( props[0] == ScriptProp{ .mName = "speed"_n,
                                   .mType = ValueType::Of( ValueKind::Number ),
                                   .mDefault = Value( 40.0 ),
                                   .mMin = 0,
                                   .mMax = 100,
                                   .mTooltip = "How fast it swings" } );
    CHECK( props[1].mDefault == Value( MakeText( "door" ) ) );
    CHECK( props[2].mDefault == Value( false ) );
    CHECK( props[3].mDefault == Value( vec3( 1.0f, 2.0f, -3.0f ) ) );
    CHECK( props[4].mDefault == Value( Color{ .mR = 1, .mG = 0.5f, .mB = 0, .mA = 1 } ) );
    CHECK( props[5].mDefault == Value( *AssetId::TryParse( "9c1e0f3a5b7d4e2c8a6f1b3d5e7c9a0b" ) ) );
    CHECK( props[6].mDefault == Value( EntityPath{} ) );
    CHECK( props[6].mType.mKind == ValueKind::Entity );
    CHECK( props[7].mType.Describe() == ValueType::ListOf( ValueType::Of( ValueKind::Vec3 ) ).Describe() );
    CHECK( props[8].mDefault == Value( Value::List{} ) );
    CHECK( props[8].mType.Describe() == ValueType::ListOf( ValueType::Of( ValueKind::Number ) ).Describe() );
    CHECK( props[9].mType.mKind == ValueKind::Record );
    CHECK( assetMaybe->mRequires == vector<string>{ "scripts/lib/inventory.luau", "scripts/tuning.luau" } );
    CHECK_FALSE( assetMaybe->mBytecode.empty() );
}

TEST_CASE( "A prop the inspector could not show refuses the file, saying where and what it may be" )
{
    Imports imports( false );
    CHECK( imports.Error( "scripts/a.luau", "props { speed = 2 * 20 }" )
               .starts_with( "scripts/a.luau:1: props: speed: a literal the inspector can show" ) );
    CHECK( imports.Error( "scripts/b.luau", "props { stops = {} }" ) ==
           "scripts/b.luau:1: props: stops: an empty list says what it holds: {} :: { vector }" );
    CHECK( imports.Error( "scripts/c.luau", "props { stops = { 1, 'two' } }" ) ==
           "scripts/c.luau:1: props: stops[2]: text in a list whose first element is a number" );
    CHECK( imports.Error( "scripts/d.luau", "local t = {}\nprops( t )" ) ==
           "scripts/d.luau:2: props takes a table literal: the inspector reads it without running the file" );
    CHECK( imports.Error( "scripts/e.luau", "props { speed = prop( 1, { fastest = 3 } ) }" ) ==
           "scripts/e.luau:1: prop's hints: min and max, numbers, and tooltip, a string" );
    CHECK( imports.Error( "scripts/f.luau", "props { speed = 1 + }" ).starts_with( "scripts/f.luau:1:" ) );
}

TEST_CASE( "A typed script passes the strict check against what the engine declares" )
{
    Imports imports;
    const auto assetMaybe = imports.TryImport( "scripts/mover.luau", cTyped );
    REQUIRE_MESSAGE( assetMaybe, assetMaybe.error() );
    CHECK( assetMaybe->mProps.size() == 1 );
}

TEST_CASE( "A type error refuses the file: a misspelt field, a component's, an action the project lacks" )
{
    Imports imports;
    const auto typo = [&]( string_view from, string_view to ) {
        string source( cTyped );
        source.replace( source.find( from ), from.size(), to );
        return imports.Error( "scripts/mover.luau", source );
    };
    CHECK( typo( "self.ticks += 1", "self.tikcs += 1" ).find( "scripts/mover.luau:7: " ) == 0 );
    CHECK(
        typo( "local t = self.entity.transform", "local t = self.entity.transfrom" ).find( "scripts/mover.luau:8: " ) ==
        0 );
    CHECK( typo( "input.pressed( \"jump\" )", "input.pressed( \"jupm\" )" ).find( "scripts/mover.luau:10: " ) == 0 );
    CHECK( typo( "color( 1, 0, 0 )", "color( 1, 0 )" ).find( "scripts/mover.luau:11: " ) == 0 );
}

TEST_CASE( "No way around the check: --!nonstrict, --!nocheck, any written, callbacks unannotated" )
{
    Imports imports;
    CHECK( imports.Error( "scripts/a.luau", "--!nonstrict\nlocal x = 1" ) ==
           "scripts/a.luau:1: --!nonstrict: every script is checked strictly" );
    CHECK( imports.Error( "scripts/b.luau", "--!nocheck\nlocal x = 1" ) ==
           "scripts/b.luau:1: --!nocheck: every script is checked strictly" );
    CHECK( imports.Error( "scripts/c.luau", "local x: any = 1" ) ==
           "scripts/c.luau:1: any: write the type the value has - scripts are typed" );
    CHECK( imports.Error( "scripts/d.luau", "function on_update( self, dt: number ) end" )
               .starts_with( "scripts/d.luau:1: on_update: annotate its parameters - on_update( self: Self, dt: "
                             "number )" ) );
}

TEST_CASE( "A library is checked with the script that requires it: its types reach across" )
{
    Imports imports;
    REQUIRE( imports.TryImport( "scripts/lib/inventory.luau", R"(
local inventory = {}
function inventory.add( items: { [string]: number }, kind: string, count: number ): number
    items[kind] = ( items[kind] or 0 ) + count
    return items[kind]
end
return inventory
)" ) );
    const string user = R"(
local inventory = require( "@lib/inventory" )
local items: { [string]: number } = {}
local total: number = inventory.add( items, "gem", 2 )
)";
    REQUIRE_MESSAGE( imports.TryImport( "scripts/user.luau", user ), "" );
    string wrong = user;
    wrong.replace( wrong.find( "\"gem\", 2" ), 8, "\"gem\", \"two\"" );
    CHECK( imports.Error( "scripts/user.luau", wrong ).starts_with( "scripts/user.luau:4: " ) );
    // A require that names nothing is the import's error when checked.
    CHECK( imports.Error( "scripts/lost.luau", "local x = require( \"@libs/none\" )" ) ==
           "scripts/lost.luau:1: require( \"@libs/none\" ): no alias @libs (aliases: @lib)" );
}

TEST_CASE( "A script asset goes to bytes and comes back the same; bytes of another kind are refused" )
{
    const ScriptAsset asset{
        .mBytecode = string( "\0bytecode\xff", 10 ),
        .mProps = { ScriptProp{
            .mName = Name::Declare( "wave" ),
            .mType = ValueType::RecordOf( {
                ValueTypeField{ Name::Declare( "count" ), ValueType::Of( ValueKind::Number ) },
                ValueTypeField{ Name::Declare( "kinds" ), ValueType::ListOf( ValueType::Of( ValueKind::Name ) ) },
            } ),
            .mDefault = Value( Value::Record{
                ValueField{ Name::Declare( "count" ), Value( 3.0 ) },
                ValueField{ Name::Declare( "kinds" ), Value( Value::List{ Value( Name::Declare( "imp" ) ) } ) },
            } ),
            .mMin = 1,
            .mMax = 9,
            .mTooltip = "the next wave" } },
        .mRequires = { "scripts/lib/a.luau" }
    };
    const auto backMaybe = TryReadScriptAsset( WriteScriptAsset( asset ) );
    REQUIRE_MESSAGE( backMaybe, backMaybe.error() );
    CHECK( backMaybe->mBytecode == asset.mBytecode );
    CHECK( backMaybe->mProps == asset.mProps );
    CHECK( backMaybe->mRequires == asset.mRequires );
    CHECK( TryReadScriptAsset( "BTEX...." ).error() == "not a script the importer wrote" );
    string cut = WriteScriptAsset( asset );
    cut.pop_back();
    CHECK( TryReadScriptAsset( cut ).error() == "the imported script is damaged" );
}

TEST_CASE( "A .luaurc's aliases are read as directories from the project's root" )
{
    const auto aliasesMaybe =
        TryReadLuaurc( R"({ "languageMode": "strict", "aliases": { "lib": "./scripts/lib/" } })" );
    REQUIRE_MESSAGE( aliasesMaybe, aliasesMaybe.error() );
    CHECK( aliasesMaybe->at( "lib" ) == "scripts/lib" );
    CHECK( TryReadLuaurc( "{}" )->empty() );
    CHECK( TryReadLuaurc( R"({ "aliases": { "lib": 3 } })" ).error() ==
           ".luaurc: 1:23: alias lib: a directory, as a string" );
}

TEST_CASE( "A project's check takes its .luaurc, its files and its assets" )
{
    ScriptWorld w;
    hmap<string, string> files{
        { ".luaurc", R"({ "aliases": { "tools": "scripts/tools" } })" },
        { "scripts/tools/math2.luau", "return { twice = function( x: number ): number return 2 * x end }" },
    };
    const ProjectFiles project = ProjectFiles::InMemory( files );
    AssetCatalog catalog;
    const AssetId hero = AssetId::Generate();
    catalog.Add( hero, *AssetPath::TryFrom( "models/hero.glb" ) );
    const auto checkMaybe = TryProjectScriptCheck( w.mApp, project, catalog );
    REQUIRE_MESSAGE( checkMaybe, checkMaybe.error() );
    const Importer importer = ScriptImporter( *checkMaybe );
    const auto bytesMaybe = importer.mImport( R"(
local math2 = require( "@tools/math2" )
local P = props { model = asset( "models/hero.glb" ), size = math2.twice( 2 ) }
)",
                                              Json(), *AssetPath::TryFrom( "scripts/user.luau" ) );
    // A default must be a literal, even one the types allow.
    REQUIRE_FALSE( bytesMaybe );
    CHECK( bytesMaybe.error().starts_with( "scripts/user.luau:3: props: size:" ) );
    const auto goodMaybe = importer.mImport( "local math2 = require( \"@tools/math2\" )\n"
                                             "local P = props { model = asset( \"models/hero.glb\" ) }\n"
                                             "local n: number = math2.twice( 2 )",
                                             Json(), *AssetPath::TryFrom( "scripts/user.luau" ) );
    REQUIRE_MESSAGE( goodMaybe, goodMaybe.error() );
    const auto assetMaybe = TryReadScriptAsset( *goodMaybe );
    REQUIRE( assetMaybe );
    CHECK( assetMaybe->mProps.front().mDefault == Value( hero ) );
    CHECK( assetMaybe->mRequires == vector<string>{ "scripts/tools/math2.luau" } );

    files[".luaurc"] = "{ aliases }";
    CHECK( TryProjectScriptCheck( w.mApp, project, catalog ).error().starts_with( ".luaurc: 1:3:" ) );
}
