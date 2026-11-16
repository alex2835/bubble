#include "bubble/import/material_importers.hpp"
#include "bubble/assets/material_asset.hpp"
#include "bubble/assets/shader_asset.hpp"
#include "bubble/render/material_layout.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
Importer ShaderImporter()
{
    return Importer{
        .mExtension = ".wgsl",
        .mKind = string( cShaderKind ),
        .mVersion = 1,
        .mImport = []( string_view source, const Json&, const AssetPath& path ) -> expected<string, string> {
            if ( auto layoutMaybe = TryReflectMaterial( source, path.View() ); not layoutMaybe )
                return unexpected( std::move( layoutMaybe.error() ) );
            return string( source );
        },
    };
}

Importer MaterialImporter()
{
    return Importer{
        .mExtension = ".mat",
        .mKind = string( cMaterialKind ),
        .mVersion = 1,
        .mImport = []( string_view source, const Json&, const AssetPath& path ) -> expected<string, string> {
            auto materialMaybe = TryReadMaterialAsset( source );
            if ( not materialMaybe )
            {
                // "materials/red.mat:3:14: ..." when it says where.
                const string& error = materialMaybe.error();
                const bool placed = not error.empty() and error.front() >= '0' and error.front() <= '9';
                return unexpected( placed ? format( "{}:{}", path.View(), error )
                                          : format( "{}: {}", path.View(), error ) );
            }
            return WriteMaterialAsset( *materialMaybe );
        },
    };
}
}
