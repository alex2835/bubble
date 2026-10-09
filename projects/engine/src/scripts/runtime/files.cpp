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

expected<LuaValue, ScriptError> ScriptRuntime::RunFile( string_view path,
                                                        const AssetRef<ScriptAsset>& asset,
                                                        const LuaTable& env )
{
    if ( not mLua.Sealed() )
        return unexpected( ScriptError{ "scripts load once the Luau state is sealed", {} } );
    if ( not asset.Ready() )
        return unexpected( ScriptError{ format( "{} is not loaded", path ), {} } );

    auto chunk = mLua.Load( path, asset.Get()->mBytecode, env );
    if ( not chunk )
        return unexpected( ScriptError{ std::move( chunk.error() ), {} } );
    mRunning.emplace_back( path );
    auto ran = ( *chunk )();
    mRunning.pop_back();
    // From here on a new global is a mistake.
    if ( ran )
        env.SetMetatable( mStrictEnvironment );
    return ran;
}

LuaTable ScriptRuntime::PassportOf( LuaTable& passport, const LuaTable& env )
{
    if ( auto kept = passport[env].As<LuaTable>() )
        return *kept;
    // For props and locals an empty table kept also makes a later
    // declaration an error.
    LuaTable none = mLua.NewTable();
    passport[env] = none;
    return none;
}

expected<ScriptRuntime::ScriptFile, ScriptError> ScriptRuntime::RunScript( string_view path,
                                                                           const AssetRef<ScriptAsset>& asset )
{
    const auto fail = [&]( string message ) -> expected<ScriptFile, ScriptError> {
        return unexpected( ScriptError{ format( "{}: {}", path, message ), {} } );
    };
    ScriptFile file;
    file.mEnvironment = NewFileEnvironment( path );
    const LuaTable env = file.mEnvironment;
    if ( auto ran = RunFile( path, asset, env ); not ran )
        return unexpected( std::move( ran.error() ) );

    file.mProps = PassportOf( mPropsOf, env );
    if ( auto checked = CheckProps( file.mProps ); not checked )
        return fail( checked.error() );
    file.mLocals = PassportOf( mLocalsOf, env );
    if ( auto checked = CheckLocals( file.mLocals, file.mProps ); not checked )
        return fail( checked.error() );

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
        const auto name = key.As<string>();
        if ( name and name->starts_with( "on_" ) and value.Is( LuaKind::Function ) and
             ranges::find( mCallbacks, *name ) == mCallbacks.end() )
            LogWarning( "{}: {} is not a callback the engine calls ({})", path, *name, detail::Join( mCallbacks ) );
    }
    return file;
}

expected<ScriptRuntime::LibraryFile, ScriptError> ScriptRuntime::RunLibrary( string_view path,
                                                                             const AssetRef<ScriptAsset>& asset )
{
    const auto fail = [&]( string message ) -> expected<LibraryFile, ScriptError> {
        return unexpected( ScriptError{ format( "{}: {}", path, message ), {} } );
    };
    LibraryFile file;
    file.mEnvironment = NewFileEnvironment( path );
    const LuaTable env = file.mEnvironment;
    // Marked before it runs, so props and locals refuse it at their line.
    mLibraryOf[env] = true;
    auto ran = RunFile( path, asset, env );
    if ( not ran )
        return unexpected( std::move( ran.error() ) );

    if ( ran->IsNil() )
        return fail( "a library returns what it shares - end it with return" );
    for ( const string& name : mCallbacks )
        if ( not env.RawGet( name ).IsNil() )
            return fail( format( "{} is a callback of entity scripts; a library only returns what it shares", name ) );
    file.mResult = std::move( *ran );
    return file;
}

expected<void, string> ScriptRuntime::CheckProps( const LuaTable& props )
{
    for ( const auto& [key, value] : props.Pairs() )
    {
        const auto name = key.As<string>();
        if ( not name )
            return unexpected( "props are named, " + key.Describe() + " is not a name" );
        if ( *name == "entity" )
            return unexpected( "a prop cannot be called 'entity': self.entity is the engine's"s );
        if ( auto encoded = mLua.Encode( value ); not encoded )
            return unexpected( format( "prop {}: {}", *name, encoded.error() ) );
    }
    return {};
}

// Locals are not saved, so any value will do; what matters is that self
// has one meaning for each name.
expected<void, string> ScriptRuntime::CheckLocals( const LuaTable& locals, const LuaTable& props )
{
    for ( const auto& [key, value] : locals.Pairs() )
    {
        const auto name = key.As<string>();
        if ( not name )
            return unexpected( "locals are named, " + key.Describe() + " is not a name" );
        if ( *name == "entity" )
            return unexpected( "a local cannot be called 'entity': self.entity is the engine's"s );
        if ( not props.RawGet( key ).IsNil() )
            return unexpected( format( "{} is both a prop and a local; self has one of each name", *name ) );
    }
    return {};
}
}
