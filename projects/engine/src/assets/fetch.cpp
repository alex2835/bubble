// The registry's reads on the web: the browser's fetch, which hands the
// bytes over when they come and never blocks.
#ifdef __EMSCRIPTEN__
#include "bubble/assets/asset_registry.hpp"
#include "bubble/types/format.hpp"
#include <cstring>
#include <emscripten/fetch.h>

namespace bubble
{
namespace
{
// What a fetch carries to its callbacks through emscripten's userData: made
// when it starts, and taken back - and freed - in whichever callback ends it.
struct Fetch
{
    AssetRegistry::Read mHandOver;
};

void Succeeded( emscripten_fetch_t* fetch )
{
    const Scope<Fetch> carried( static_cast<Fetch*>( fetch->userData ) );
    string bytes( fetch->data, static_cast<size_t>( fetch->numBytes ) );
    emscripten_fetch_close( fetch );
    carried->mHandOver( std::move( bytes ) );
}

void Failed( emscripten_fetch_t* fetch )
{
    const Scope<Fetch> carried( static_cast<Fetch*>( fetch->userData ) );
    string why = format( "fetch answered {} {}", fetch->status, fetch->statusText );
    emscripten_fetch_close( fetch );
    carried->mHandOver( unexpected( std::move( why ) ) );
}
}

AssetRegistry::ReadImported AssetRegistry::FromFetch( string base )
{
    return [base = std::move( base )]( const AssetId& assetId, string_view kind, Read handOver ) {
        emscripten_fetch_attr_t attributes;
        emscripten_fetch_attr_init( &attributes );
        std::strcpy( attributes.requestMethod, "GET" );
        attributes.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attributes.onsuccess = &Succeeded;
        attributes.onerror = &Failed;
        attributes.userData = new Fetch{ std::move( handOver ) };
        const string url = format( "{}/{}", base, ImportedPath( assetId, kind ).View() );
        emscripten_fetch( &attributes, url.c_str() );
    };
}
}
#endif
