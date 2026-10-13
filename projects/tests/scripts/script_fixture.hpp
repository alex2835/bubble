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
    hmap<string, string> mFiles;
    AssetRegistry mAssets{ [this]( const AssetPath& path ) -> expected<string, string> {
        const auto found = mFiles.find( path.View() );
        if ( found == mFiles.end() )
            return unexpected( "no such file"s );
        return found->second;
    } };
    // What the world holds loaded - as the world's scripts module would.
    vector<AssetRef<ScriptAsset>> mHeld;
    LuaState mState{ PrintToLog };
    ScriptRuntime mRuntime{ mState, mAssets, { "on_start", "on_update", "on_hit" } };
    // What scripts passed to record( text ), in order.
    vector<string> mRecorded;
    // What destroy() destroys - as a World's would the entity's.
    ScriptInstanceHandle mDoomedHandle;

    Scripts()
    {
        LuaTable globals = mState.Globals();
        globals["record"] = [this]( string text ) { mRecorded.push_back( std::move( text ) ); };
        globals["destroy"] = [this]() { mRuntime.Destroy( mDoomedHandle ); };
        mState.Seal();
        RegisterScriptImporter( mAssets );
    }

    lua_State* L() const { return mState.L(); }

    // Writes the file and loads it into the registry, as a world loads what
    // it needs before its scripts run.
    void AddFile( string_view path, string_view source )
    {
        mFiles[string( path )] = string( source );
        auto asset = mAssets.Load<ScriptAsset>( *AssetPath::TryFrom( path ) );
        REQUIRE_MESSAGE( asset.Ready(), asset.Entry().Error() );
        mHeld.push_back( std::move( asset ) );
    }

    void AddLibrary( string_view path, string_view source ) { AddFile( path, source ); }

    // The file changed on disk: the registry imports it again and the
    // runtime hears of it. Fails only if the new version does not compile.
    expected<void, string> TryChange( string_view path, string_view source )
    {
        mFiles[string( path )] = string( source );
        return mAssets.TryReload( *AssetPath::TryFrom( path ) );
    }

    ScriptHandle Load( string_view path, string_view source )
    {
        AddFile( path, source );
        auto scriptMaybe = mRuntime.TryLoad( path );
        REQUIRE_MESSAGE( scriptMaybe.has_value(), ( scriptMaybe ? "" : scriptMaybe.error().mMessage ) );
        return *scriptMaybe;
    }

    string LoadError( string_view path, string_view source )
    {
        AddFile( path, source );
        auto scriptMaybe = mRuntime.TryLoad( path );
        REQUIRE_FALSE( scriptMaybe.has_value() );
        return scriptMaybe.error().mMessage;
    }

    ScriptInstanceHandle Make( ScriptHandle scriptHandle, string label = "/player" )
    {
        auto instanceMaybe = mRuntime.TryCreate( scriptHandle, std::move( label ) );
        REQUIRE_MESSAGE( instanceMaybe.has_value(), ( instanceMaybe ? "" : instanceMaybe.error() ) );
        return *instanceMaybe;
    }

    // What a Luau expression evaluates to, run in the globals.
    LuaValue Evaluate( string_view expression )
    {
        auto chunkMaybe = mState.TryLoad( "=test", Bytecode( "return " + string( expression ) ), mState.Globals() );
        REQUIRE_MESSAGE( chunkMaybe.has_value(), ( chunkMaybe ? "" : chunkMaybe.error() ) );
        auto valueMaybe = ( *chunkMaybe )();
        REQUIRE_MESSAGE( valueMaybe.has_value(), ( valueMaybe ? "" : valueMaybe.error().mMessage ) );
        return *valueMaybe;
    }

    // Made with the overrides of the table `overrides` evaluates to.
    expected<ScriptInstanceHandle, string> TryMakeWith( ScriptHandle scriptHandle,
                                                        string_view overrides,
                                                        string label = "/player" )
    {
        return mRuntime.TryCreate( scriptHandle, std::move( label ), LuaTable( Evaluate( overrides ) ) );
    }

    bool Call( ScriptInstanceHandle instanceHandle, u32 callback ) { return mRuntime.Call( instanceHandle, callback ); }

    LuaValue Field( ScriptInstanceHandle instanceHandle, const char* field ) const
    {
        return mRuntime.Self( instanceHandle ).RawGet( field );
    }

    double Number( ScriptInstanceHandle instanceHandle, const char* field ) const
    {
        return Field( instanceHandle, field ).TryAs<f64>().value_or( 0.0 );
    }

    void Set( ScriptInstanceHandle instanceHandle, const char* field, bool value ) const
    {
        mRuntime.Self( instanceHandle ).RawSet( field, value );
    }
};
}
