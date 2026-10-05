#pragma once
// A world's scripts without the world: project files in memory, an asset
// registry that imports .luau from them, a sealed LuaState, a ScriptRuntime
// with three callbacks, and two functions bound for observing - record( text )
// for order, destroy() for lifetimes.
#include "bubble/assets/asset_registry.hpp"
#include "bubble/scripts/script_asset.hpp"
#include "bubble/scripts/script_runtime.hpp"
#include "scripts/script_helpers.hpp"

namespace bubble::test
{
enum Callback : u32
{
    OnStart,
    OnUpdate,
    OnHit,
};

struct Scripts
{
    // The project's files, by path.
    str_hmap<string> mFiles;
    AssetRegistry mAssets{ [this]( const AssetPath& path ) -> expected<string, string> {
        const auto found = mFiles.find( path.View() );
        if ( found == mFiles.end() )
            return std::unexpected( "no such file"s );
        return found->second;
    } };
    // What the world holds loaded - as its scripts module would.
    vector<AssetHandle<ScriptAsset>> mHeld;
    LuaState mState{ PrintToLog };
    ScriptRuntime mRuntime{ mState, mAssets, { "on_start", "on_update", "on_hit" } };
    // What scripts passed to record( text ), in order.
    vector<string> mRecorded;
    // What destroy() destroys - as a World's would the entity's.
    ScriptInstanceHandle mDoomed;

    Scripts()
    {
        LuaTable globals = mState.Globals();
        globals["record"] = [this]( string text ) { mRecorded.push_back( std::move( text ) ); };
        globals["destroy"] = [this]() { mRuntime.Destroy( mDoomed ); };
        mState.Seal();
        RegisterScriptImporter( mAssets );
    }

    lua_State* L() const { return mState.L(); }

    // Writes the file and loads it into the registry, as a world loads what
    // it needs before its scripts run.
    void AddFile( string_view path, string_view source )
    {
        mFiles[string( path )] = string( source );
        auto asset = mAssets.Load<ScriptAsset>( *AssetPath::From( path ) );
        REQUIRE_MESSAGE( asset.Ready(), asset.Slot().Error() );
        mHeld.push_back( std::move( asset ) );
    }

    void AddLibrary( string_view path, string_view source ) { AddFile( path, source ); }

    // The file changed on disk: the registry imports it again and the
    // runtime hears of it. Fails only if the new version does not compile.
    expected<void, string> Change( string_view path, string_view source )
    {
        mFiles[string( path )] = string( source );
        return mAssets.Reload( *AssetPath::From( path ) );
    }

    ScriptModuleHandle Load( string_view path, string_view source )
    {
        AddFile( path, source );
        auto module = mRuntime.Load( path );
        REQUIRE_MESSAGE( module.has_value(), ( module ? "" : module.error().mMessage ) );
        return *module;
    }

    string LoadError( string_view path, string_view source )
    {
        AddFile( path, source );
        auto module = mRuntime.Load( path );
        REQUIRE_FALSE( module.has_value() );
        return module.error().mMessage;
    }

    ScriptInstanceHandle Make( ScriptModuleHandle module, string label = "/player" )
    {
        auto instance = mRuntime.Create( module, std::move( label ) );
        REQUIRE_MESSAGE( instance.has_value(), ( instance ? "" : instance.error() ) );
        return *instance;
    }

    // What a Luau expression evaluates to, run in the globals.
    LuaValue Evaluate( string_view expression )
    {
        auto chunk = mState.Load( "=test", Bytecode( "return " + string( expression ) ), mState.Globals() );
        REQUIRE_MESSAGE( chunk.has_value(), ( chunk ? "" : chunk.error() ) );
        auto value = ( *chunk )();
        REQUIRE_MESSAGE( value.has_value(), ( value ? "" : value.error().mMessage ) );
        return *value;
    }

    // Made with the overrides of the table `overrides` evaluates to.
    expected<ScriptInstanceHandle, string> MakeWith( ScriptModuleHandle module, string_view overrides,
                                                     string label = "/player" )
    {
        return mRuntime.Create( module, std::move( label ), LuaTable( Evaluate( overrides ) ) );
    }

    bool Call( ScriptInstanceHandle instance, u32 callback ) { return mRuntime.Call( instance, callback ); }

    LuaValue Field( ScriptInstanceHandle instance, const char* field ) const
    {
        return mRuntime.Self( instance ).RawGet( field );
    }

    double Number( ScriptInstanceHandle instance, const char* field ) const
    {
        return Field( instance, field ).As<f64>().value_or( 0.0 );
    }

    void Set( ScriptInstanceHandle instance, const char* field, bool value ) const
    {
        mRuntime.Self( instance ).RawSet( field, value );
    }
};
}
