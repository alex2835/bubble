#include "bubble/assets/material_asset.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/data/json.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
constexpr int cVersion = 1;

// "3:14: params.roughness: ..." - where in the file, then what.
unexpected<string> Fail( const Json& at, string_view where, string_view what )
{
    const string place = at.Where();
    return unexpected( place.empty() ? format( "{}: {}", where, what ) : format( "{}: {}: {}", place, where, what ) );
}

expected<AssetId, string> TryReadAsset( const Json& json, string_view where )
{
    const auto assetMaybe = json.TryFind( "asset" );
    const auto textMaybe = assetMaybe ? assetMaybe->TryAs<string>() : OptRef<const string>();
    if ( not textMaybe )
        return Fail( json, where, format( "an asset is {{ \"asset\": id }}, not {}", json.Describe() ) );
    const auto assetIdMaybe = AssetId::TryParse( *textMaybe );
    if ( not assetIdMaybe )
        return Fail( *assetMaybe, where, format( "{} is not an asset id: 32 hex digits", json.Describe() ) );
    return *assetIdMaybe;
}

expected<MaterialValue, string> TryReadValue( const Json& json, string_view where )
{
    if ( const auto numberMaybe = json.TryAs<f64>() )
        return MaterialValue{ .mNumbers = { static_cast<f32>( *numberMaybe ) }, .mTexture = {} };
    if ( const auto arrayMaybe = json.TryAs<Json::Array>();
         arrayMaybe and not arrayMaybe->empty() and arrayMaybe->size() <= 4 )
    {
        MaterialValue value;
        for ( const Json& element : *arrayMaybe )
        {
            const auto numberMaybe = element.TryAs<f64>();
            if ( not numberMaybe )
                return Fail( element, where, format( "a vector holds numbers, not {}", element.Describe() ) );
            value.mNumbers.push_back( static_cast<f32>( *numberMaybe ) );
        }
        return value;
    }
    if ( json.Is<Json::Object>() )
    {
        auto textureMaybe = TryReadAsset( json, where );
        if ( not textureMaybe )
            return unexpected( std::move( textureMaybe.error() ) );
        return MaterialValue{ .mNumbers = {}, .mTexture = *textureMaybe };
    }
    return Fail( json, where,
                 format( "a number, 1 to 4 numbers or a texture {{ \"asset\": id }}, not {}", json.Describe() ) );
}

expected<bool, string> TryReadFlag( const Json& json, string_view where )
{
    const auto flagMaybe = json.TryAs<bool>();
    if ( not flagMaybe )
        return Fail( json, where, format( "true or false, not {}", json.Describe() ) );
    return *flagMaybe;
}

Json AssetJson( const AssetId& assetId )
{
    return Json( Json::Object{ Json::Member{ "asset", Json( assetId.ToString() ) } } );
}
}

expected<MaterialAsset, string> TryReadMaterialAsset( string_view text )
{
    auto jsonMaybe = Json::TryParse( text );
    if ( not jsonMaybe )
        return unexpected( std::move( jsonMaybe.error() ) );
    const auto objectMaybe = jsonMaybe->TryAs<Json::Object>();
    if ( not objectMaybe )
        return unexpected( format( "a material is a JSON object, not {}", jsonMaybe->Describe() ) );

    MaterialAsset material;
    for ( const Json::Member& member : *objectMaybe )
    {
        const string_view key = member.mKey;
        const Json& value = member.mValue;
        if ( key == "version" )
        {
            if ( const auto versionMaybe = value.TryAs<f64>(); not versionMaybe or *versionMaybe != cVersion )
                return Fail( value, key,
                             format( "{} is not a version this reads (it reads {})", value.Describe(), cVersion ) );
        }
        else if ( key == "shader" or key == "base" )
        {
            auto assetIdMaybe = TryReadAsset( value, key );
            if ( not assetIdMaybe )
                return unexpected( std::move( assetIdMaybe.error() ) );
            ( key == "shader" ? material.mShader : material.mBase ) = *assetIdMaybe;
        }
        else if ( key == "double_sided" or key == "transparent" )
        {
            auto flagMaybe = TryReadFlag( value, key );
            if ( not flagMaybe )
                return unexpected( std::move( flagMaybe.error() ) );
            ( key == "double_sided" ? material.mDoubleSided : material.mTransparent ) = *flagMaybe;
        }
        else if ( key == "params" )
        {
            const auto paramsMaybe = value.TryAs<Json::Object>();
            if ( not paramsMaybe )
                return Fail( value, key, format( "params are an object, not {}", value.Describe() ) );
            for ( const Json::Member& param : *paramsMaybe )
            {
                auto paramMaybe = TryReadValue( param.mValue, format( "params.{}", param.mKey ) );
                if ( not paramMaybe )
                    return unexpected( std::move( paramMaybe.error() ) );
                material.mParams.emplace_back( param.mKey, std::move( *paramMaybe ) );
            }
        }
        else
            return Fail( value, key, "not a field (fields: version, shader, base, double_sided, transparent, params)" );
    }
    if ( material.mShader and material.mBase )
        return unexpected( "shader: a material with a base takes its base's shader"s );
    return material;
}

string WriteMaterialAsset( const MaterialAsset& material )
{
    Json::Object file{ Json::Member{ "version", Json( static_cast<f64>( cVersion ) ) } };
    if ( material.mShader )
        file.push_back( Json::Member{ "shader", AssetJson( material.mShader ) } );
    if ( material.mBase )
        file.push_back( Json::Member{ "base", AssetJson( material.mBase ) } );
    if ( material.mDoubleSided )
        file.push_back( Json::Member{ "double_sided", Json( *material.mDoubleSided ) } );
    if ( material.mTransparent )
        file.push_back( Json::Member{ "transparent", Json( *material.mTransparent ) } );
    if ( not material.mParams.empty() )
    {
        Json::Object params;
        for ( const auto& [name, value] : material.mParams )
        {
            if ( value.mTexture )
                params.push_back( Json::Member{ name, AssetJson( value.mTexture ) } );
            else if ( value.mNumbers.size() == 1 )
                params.push_back( Json::Member{ name, Json( static_cast<f64>( value.mNumbers.front() ) ) } );
            else
            {
                Json::Array numbers;
                for ( const f32 number : value.mNumbers )
                    numbers.push_back( Json( static_cast<f64>( number ) ) );
                params.push_back( Json::Member{ name, Json( std::move( numbers ) ) } );
            }
        }
        file.push_back( Json::Member{ "params", Json( std::move( params ) ) } );
    }
    return Json( std::move( file ) ).Write();
}

void RegisterMaterialLoader( AssetRegistry& registry )
{
    registry.RegisterLoader<MaterialAsset>( cMaterialKind, []( string_view bytes, const AssetPath& path ) {
        auto materialMaybe = TryReadMaterialAsset( bytes );
        if ( not materialMaybe )
            return expected<MaterialAsset, string>(
                unexpected( format( "{}: {}", path.View(), materialMaybe.error() ) ) );
        return materialMaybe;
    } );
}
}
