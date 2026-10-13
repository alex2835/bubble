#include "bubble/core/asset_path.hpp"
#include "bubble/scripts/script_runtime.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "text.hpp"

// require: the path a request names, and the library behind it - run
// once per world, at its first require.
namespace bubble
{
expected<string, string> ScriptRuntime::TryResolve( string_view from, string_view request ) const
{
    string joined;
    if ( request.starts_with( "./" ) or request.starts_with( "../" ) )
    {
        const size_t slash = from.rfind( '/' );
        const string_view directory = slash == string_view::npos ? string_view() : from.substr( 0, slash );
        joined = string( directory ) + "/" + string( request );
    }
    else if ( request.starts_with( '@' ) )
    {
        const size_t slash = request.find( '/' );
        const string_view alias = request.substr( 1, slash == string_view::npos ? string_view::npos : slash - 1 );
        const auto found = mAliases.find( alias );
        if ( found == mAliases.end() )
        {
            vector<string> known;
            for ( const auto& entry : mAliases )
                known.push_back( "@" + entry.first );
            ranges::sort( known );
            return unexpected( format( "no alias @{} (aliases: {})", alias, detail::Join( known ) ) );
        }
        joined = found->second + ( slash == string_view::npos ? "" : string( request.substr( slash ) ) );
    }
    else
        return unexpected( "a path starts with ./, ../ or @alias"s );

    // "." steps go, ".." takes one back.
    vector<string_view> steps;
    const string_view all = joined;
    for ( size_t at = 0; at <= all.size(); )
    {
        const size_t end = min( all.find( '/', at ), all.size() );
        const string_view step = all.substr( at, end - at );
        at = end + 1;
        if ( step.empty() or step == "." )
            continue;
        if ( step == ".." )
        {
            if ( steps.empty() )
                return unexpected( "the path leads out of the project"s );
            steps.pop_back();
        }
        else
            steps.push_back( step );
    }
    string path;
    for ( const string_view step : steps )
        path += ( path.empty() ? "" : "/" ) + string( step );
    path += ".luau";
    if ( auto validMaybe = AssetPath::TryFrom( path ); not validMaybe )
        return unexpected( validMaybe.error() );
    return path;
}

expected<LuaValue, string> ScriptRuntime::TryRequire( const LuaTable& file, const string& request )
{
    const auto fromMaybe = mPathOf[file].TryAs<string>();
    if ( not fromMaybe )
        return unexpected( "require works in script files"s );
    auto pathMaybe = TryResolve( *fromMaybe, request );
    if ( not pathMaybe )
        return unexpected( format( "require( '{}' ): {}", request, pathMaybe.error() ) );
    auto found = mLibraries.find( *pathMaybe );
    if ( found == mLibraries.end() )
    {
        // First require of this file in the world: the registry has it
        // loaded, or the world was started without it.
        AssetRef<ScriptAsset> assetRef = mAssets.TryFind<ScriptAsset>( *AssetPath::TryFrom( *pathMaybe ) );
        if ( not assetRef.Ready() )
            return unexpected( format( "require( '{}' ): no library {} is loaded", request, *pathMaybe ) );
        found =
            mLibraries
                .emplace( *pathMaybe, Library{ .mAssetRef = std::move( assetRef ), .mResult = {}, .mEnvironment = {} } )
                .first;
    }
    // Recorded by this run of the file: a run that fails takes it away.
    PassportOf( mRequiresOf, file ).RawSet( *pathMaybe, true );
    if ( not found->second.mResult.IsNil() )
        return found->second.mResult;

    if ( const auto running = ranges::find( mRunning, *pathMaybe ); running != mRunning.end() )
    {
        string chain;
        for ( auto step = running; step != mRunning.end(); ++step )
            chain += *step + " -> ";
        return unexpected( "require cycle: " + chain + *pathMaybe );
    }

    // A copy of the handle keeps the bytecode while it runs.
    const AssetRef<ScriptAsset> assetRef = found->second.mAssetRef;
    auto ranMaybe = TryRunLibrary( *pathMaybe, assetRef );
    if ( not ranMaybe )
    {
        const ScriptError& error = ranMaybe.error();
        return unexpected( error.mTraceback.empty() ? error.mMessage : error.mMessage + "\n" + error.mTraceback );
    }
    Library& library = mLibraries.find( *pathMaybe )->second;
    library.mResult = std::move( ranMaybe->mResult );
    library.mEnvironment = std::move( ranMaybe->mEnvironment );
    return library.mResult;
}
}
