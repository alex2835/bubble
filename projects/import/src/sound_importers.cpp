#include "bubble/import/sound_importers.hpp"
#include "bubble/audio/audio.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
// Longer than this streams unless the settings say otherwise.
constexpr f64 cStreamFrom = 10.0;
}

vector<Importer> SoundImporters()
{
    vector<Importer> importers;
    for ( const string_view extension : { ".wav", ".flac", ".mp3" } )
        importers.push_back( Importer{
            .mExtension = string( extension ),
            .mKind = string( cSoundKind ),
            .mVersion = 1,
            .mImport = []( string_view source, const Json& settings,
                           const AssetPath& path ) -> expected<string, string> {
                const auto infoMaybe = TryProbeSound( source );
                if ( not infoMaybe )
                    return unexpected( path.String() + ": " + infoMaybe.error() );
                bool stream = infoMaybe->mSeconds > cStreamFrom;
                if ( const auto settingMaybe = settings.TryFind( "stream" ) )
                {
                    const auto streamMaybe = settingMaybe->TryAs<bool>();
                    if ( not streamMaybe )
                        return unexpected( format( "{}.import: stream is true or false, not {}", path.String(),
                                                   settingMaybe->Describe() ) );
                    stream = *streamMaybe;
                }
                return WriteSoundAsset( source, stream );
            },
        } );
    return importers;
}
}
