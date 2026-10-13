#include "bubble/core/log.hpp"
#include "bubble/scripts/script_runtime.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "text.hpp"

// Running a file: its environment, its top run once, and what a script or
// a library leaves - the file's passport.
namespace bubble
{
LuaTable ScriptRuntime::NewFileEnvironment( string_view path )
{
    // The file's own globals, reading through to the engine's; what props,
    // locals and require need to know of the file, by its environment.
    LuaTable env = mLua.NewTable();
    env.SetMetatable( mOpenEnvironment );
    mPathOf[env] = path;
    return env;
}

expected<LuaValue, ScriptError> ScriptRuntime::TryRunFile( string_view path,
                                                           const AssetRef<ScriptAsset>& assetRef,
                                                           const LuaTable& env )
{
    if ( not mLua.Sealed() )
        return unexpected( ScriptError{ "scripts load once the Luau state is sealed", {} } );
    if ( not assetRef.Ready() )
        return unexpected( ScriptError{ format( "{} is not loaded", path ), {} } );

    auto chunkMaybe = mLua.TryLoad( path, assetRef.TryGet()->mBytecode, env );
    if ( not chunkMaybe )
        return unexpected( ScriptError{ std::move( chunkMaybe.error() ), {} } );
    mRunning.emplace_back( path );
    expected<LuaValue, ScriptError> ranMaybe;
    {
        // The top of a file runs once for the world and belongs to no
        // instance, even when an instance's require runs it: on and start
        // there would tie a world's file to whoever came first.
        const CurrentScope scope( *this, {} );
        ranMaybe = ( *chunkMaybe )();
    }
    mRunning.pop_back();
    // From here on a new global is a mistake.
    if ( ranMaybe )
        env.SetMetatable( mStrictEnvironment );
    return ranMaybe;
}

LuaTable ScriptRuntime::PassportOf( LuaTable& passport, const LuaTable& env )
{
    if ( auto keptMaybe = passport[env].TryAs<LuaTable>() )
        return *keptMaybe;
    // For props and locals an empty table kept also makes a later
    // declaration an error.
    LuaTable none = mLua.NewTable();
    passport[env] = none;
    return none;
}

expected<ScriptRuntime::ScriptFile, ScriptError> ScriptRuntime::TryRunScript( string_view path,
                                                                              const AssetRef<ScriptAsset>& assetRef )
{
    const auto fail = [&]( string message ) -> expected<ScriptFile, ScriptError> {
        return unexpected( ScriptError{ format( "{}: {}", path, message ), {} } );
    };
    ScriptFile file;
    file.mEnvironment = NewFileEnvironment( path );
    const LuaTable env = file.mEnvironment;
    if ( auto ranMaybe = TryRunFile( path, assetRef, env ); not ranMaybe )
        return unexpected( std::move( ranMaybe.error() ) );

    file.mProps = PassportOf( mPropsOf, env );
    if ( auto checkedMaybe = TryCheckProps( file.mProps ); not checkedMaybe )
        return fail( checkedMaybe.error() );
    file.mLocals = PassportOf( mLocalsOf, env );
    if ( auto checkedMaybe = TryCheckLocals( file.mLocals, file.mProps ); not checkedMaybe )
        return fail( checkedMaybe.error() );

    for ( const string& name : mCallbacks )
    {
        const LuaValue callback = env.RawGet( name );
        if ( not callback.IsNil() and not callback.Is( LuaKind::Function ) )
            return fail( format( "{} is {}, not a function", name, detail::KindName( callback.Kind() ) ) );
        file.mCallbacks.emplace_back( callback );
    }

    // on_updte would never run; say so instead of staying silent.
    for ( const auto& [key, value] : env.Pairs() )
    {
        const auto nameMaybe = key.TryAs<string>();
        if ( nameMaybe and nameMaybe->starts_with( "on_" ) and value.Is( LuaKind::Function ) and
             ranges::find( mCallbacks, *nameMaybe ) == mCallbacks.end() )
            LogWarning( "{}: {} is not a callback the engine calls ({})", path, *nameMaybe,
                        detail::Join( mCallbacks ) );
    }
    return file;
}

expected<ScriptRuntime::LibraryFile, ScriptError> ScriptRuntime::TryRunLibrary( string_view path,
                                                                                const AssetRef<ScriptAsset>& assetRef )
{
    const auto fail = [&]( string message ) -> expected<LibraryFile, ScriptError> {
        return unexpected( ScriptError{ format( "{}: {}", path, message ), {} } );
    };
    LibraryFile file;
    file.mEnvironment = NewFileEnvironment( path );
    const LuaTable env = file.mEnvironment;
    // Marked before it runs, so props and locals refuse it at their line.
    mLibraryOf[env] = true;
    auto ranMaybe = TryRunFile( path, assetRef, env );
    if ( not ranMaybe )
        return unexpected( std::move( ranMaybe.error() ) );

    if ( ranMaybe->IsNil() )
        return fail( "a library returns what it shares - end it with return" );
    for ( const string& name : mCallbacks )
        if ( not env.RawGet( name ).IsNil() )
            return fail( format( "{} is a callback of entity scripts; a library only returns what it shares", name ) );
    file.mResult = std::move( *ranMaybe );
    return file;
}

expected<void, string> ScriptRuntime::TryCheckProps( const LuaTable& props )
{
    for ( const auto& [key, value] : props.Pairs() )
    {
        const auto nameMaybe = key.TryAs<string>();
        if ( not nameMaybe )
            return unexpected( "props are named, " + key.Describe() + " is not a name" );
        if ( *nameMaybe == "entity" )
            return unexpected( "a prop cannot be called 'entity': self.entity is the engine's"s );
        if ( auto encodedMaybe = mLua.TryEncode( value ); not encodedMaybe )
            return unexpected( format( "prop {}: {}", *nameMaybe, encodedMaybe.error() ) );
    }
    return {};
}

// Locals are not saved, so any value will do; what matters is that self
// has one meaning for each name.
expected<void, string> ScriptRuntime::TryCheckLocals( const LuaTable& locals, const LuaTable& props )
{
    for ( const auto& [key, value] : locals.Pairs() )
    {
        const auto nameMaybe = key.TryAs<string>();
        if ( not nameMaybe )
            return unexpected( "locals are named, " + key.Describe() + " is not a name" );
        if ( *nameMaybe == "entity" )
            return unexpected( "a local cannot be called 'entity': self.entity is the engine's"s );
        if ( not props.RawGet( key ).IsNil() )
            return unexpected( format( "{} is both a prop and a local; self has one of each name", *nameMaybe ) );
    }
    return {};
}
}
