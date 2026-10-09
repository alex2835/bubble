#include "bubble/core/asset_path.hpp"
#include "bubble/scripts/script_runtime.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "text.hpp"

// require: the path a request names, and the library behind it - run
// once per world, at its first require.
namespace bubble
{
expected<string, string> ScriptRuntime::Resolve( string_view from, string_view request ) const
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
    if ( auto valid = AssetPath::From( path ); not valid )
        return unexpected( valid.error() );
    return path;
}

expected<LuaValue, string> ScriptRuntime::Require( const LuaTable& file, const string& request )
{
    const auto from = mPathOf[file].As<string>();
    if ( not from )
        return unexpected( "require works in script files"s );
    auto path = Resolve( *from, request );
    if ( not path )
        return unexpected( format( "require( '{}' ): {}", request, path.error() ) );
    auto found = mLibraries.find( *path );
    if ( found == mLibraries.end() )
    {
        // First require of this file in the world: the registry has it
        // loaded, or the world was started without it.
        AssetRef<ScriptAsset> assetRef = mAssets.Find<ScriptAsset>( *AssetPath::From( *path ) );
        if ( not assetRef.Ready() )
            return unexpected( format( "require( '{}' ): no library {} is loaded", request, *path ) );
        found = mLibraries
                    .emplace( *path, Library{ .mAssetRef = std::move( assetRef ), .mResult = {}, .mEnvironment = {} } )
                    .first;
    }
    // Recorded by this run of the file: a run that fails takes it away.
    PassportOf( mRequiresOf, file ).RawSet( *path, true );
    if ( not found->second.mResult.IsNil() )
        return found->second.mResult;

    if ( const auto running = ranges::find( mRunning, *path ); running != mRunning.end() )
    {
        string chain;
        for ( auto step = running; step != mRunning.end(); ++step )
            chain += *step + " -> ";
        return unexpected( "require cycle: " + chain + *path );
    }

    // A copy of the handle keeps the bytecode while it runs.
    const AssetRef<ScriptAsset> assetRef = found->second.mAssetRef;
    auto ran = RunLibrary( *path, assetRef );
    if ( not ran )
    {
        const ScriptError& error = ran.error();
        return unexpected( error.mTraceback.empty() ? error.mMessage : error.mMessage + "\n" + error.mTraceback );
    }
    Library& library = mLibraries.find( *path )->second;
    library.mResult = std::move( ran->mResult );
    library.mEnvironment = std::move( ran->mEnvironment );
    return library.mResult;
}
}
