#include "bubble/render/material_layout.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include <wgsl_reflect/wgsl_reflect.hpp>

namespace bubble
{
namespace
{
constexpr u32 cMaterialGroup = 2;

// The text of `@name( ... )` in a comment, if it has one.
opt<string_view> TryAnnotation( string_view comment, string_view name )
{
    const string opening = format( "@{}", name );
    const size_t at = comment.find( opening );
    if ( at == string_view::npos )
        return nullopt;
    const size_t open = comment.find( '(', at + opening.size() );
    const size_t close = comment.find( ')', open );
    if ( open == string_view::npos or close == string_view::npos )
        return string_view();
    return comment.substr( open + 1, close - open - 1 );
}

string_view Trim( string_view text )
{
    while ( not text.empty() and text.front() == ' ' )
        text.remove_prefix( 1 );
    while ( not text.empty() and text.back() == ' ' )
        text.remove_suffix( 1 );
    return text;
}

opt<vector<f32>> TryNumbers( string_view text )
{
    vector<f32> numbers;
    while ( true )
    {
        const size_t comma = text.find( ',' );
        const string_view piece = Trim( text.substr( 0, comma ) );
        f32 number = 0;
        const auto [end, error] = from_chars( piece.data(), piece.data() + piece.size(), number );
        if ( piece.empty() or error != errc() or end != piece.data() + piece.size() )
            return nullopt;
        numbers.push_back( number );
        if ( comma == string_view::npos )
            return numbers;
        text.remove_prefix( comma + 1 );
    }
}

expected<MaterialParam, string> TryNumberParam( const wgsl_reflect::StructMember& member, string_view where )
{
    const wgsl_reflect::Type& type = member.mType;
    const bool floats = type.mScalar == wgsl_reflect::ScalarType::F32;
    const bool isScalar = type.mKind == wgsl_reflect::TypeKind::Scalar and floats;
    const bool isVector = type.mKind == wgsl_reflect::TypeKind::Vector and floats and type.mRows >= 2;
    if ( not isScalar and not isVector )
        return unexpected( format( "{}: {} is a {}; a material's numbers are f32 and vec2f to vec4f", where,
                                   member.mName, type.mSpelling ) );
    MaterialParam param{ .mName = member.mName,
                         .mKind = MaterialParamKind::Number,
                         .mOffset = member.mOffset,
                         .mComponents = isScalar ? 1 : type.mRows,
                         .mColor = member.mTrailingComment.find( "@color" ) != string::npos,
                         .mDefault = {},
                         .mBinding = 0,
                         .mTextureDefault = TextureDefault::White };
    if ( param.mColor and param.mComponents < 3 )
        return unexpected(
            format( "{}: {} is a color, which is a vec3f or a vec4f, not a {}", where, member.mName, type.mSpelling ) );
    // A color is white until it is picked; anything else nothing.
    param.mDefault = vector<f32>( param.mComponents, param.mColor ? 1.0f : 0.0f );
    if ( const auto annotationMaybe = TryAnnotation( member.mTrailingComment, "default" ) )
    {
        const auto numbersMaybe = TryNumbers( *annotationMaybe );
        if ( not numbersMaybe or numbersMaybe->size() != param.mComponents )
            return unexpected( format( "{}: {}'s @default( {} ) is not {} number{}", where, member.mName,
                                       Trim( *annotationMaybe ), param.mComponents,
                                       param.mComponents == 1 ? "" : "s" ) );
        param.mDefault = std::move( *numbersMaybe );
    }
    return param;
}

expected<TextureDefault, string> TryTextureDefault( const wgsl_reflect::Binding& binding, string_view where )
{
    const auto annotationMaybe = TryAnnotation( binding.mTrailingComment, "default" );
    if ( not annotationMaybe )
        return TextureDefault::White;
    const string_view name = Trim( *annotationMaybe );
    if ( name == "white" )
        return TextureDefault::White;
    if ( name == "black" )
        return TextureDefault::Black;
    if ( name == "normal" )
        return TextureDefault::Normal;
    return unexpected(
        format( "{}: {}'s @default( {} ) is not a texture's (white, black, normal)", where, binding.mName, name ) );
}
}

OptRef<const MaterialParam> MaterialLayout::TryFind( string_view name ) const
{
    for ( const MaterialParam& param : mParams )
        if ( param.mName == name )
            return param;
    return nullopt;
}

expected<MaterialLayout, string> TryReflectMaterial( string_view source, string_view label )
{
    const wgsl_reflect::Module module = wgsl_reflect::Parse( source );
    if ( not module.mErrors.empty() )
        return unexpected( format( "{}: {}", label, module.mErrors.front() ) );
    if ( find( module.mFunctions.begin(), module.mFunctions.end(), "surface" ) == module.mFunctions.end() )
        return unexpected(
            format( "{}: no fn surface( in: SurfaceInput ) -> SurfaceOutput, which a material is", label ) );

    MaterialLayout layout;
    for ( const wgsl_reflect::Binding& binding : module.mBindings )
    {
        if ( binding.mGroup != cMaterialGroup )
            return unexpected( format( "{}: {} is in group {}; a material binds in group {} alone, the others are "
                                       "the engine's",
                                       label, binding.mName, binding.mGroup, cMaterialGroup ) );
        const wgsl_reflect::Type& type = binding.mType;
        if ( binding.mAddressSpace == wgsl_reflect::AddressSpace::Uniform and
             type.mKind == wgsl_reflect::TypeKind::Struct )
        {
            if ( layout.mUniformBinding )
                return unexpected(
                    format( "{}: {} is a second uniform block; a material has one", label, binding.mName ) );
            const wgsl_reflect::Struct* block = module.FindStruct( type.mName );
            if ( block == nullptr )
                return unexpected(
                    format( "{}: {} is a {}, which it does not declare", label, binding.mName, type.mName ) );
            layout.mUniformBinding = binding.mBinding;
            layout.mUniformSize = block->mSize;
            for ( const wgsl_reflect::StructMember& member : block->mMembers )
            {
                auto paramMaybe = TryNumberParam( member, label );
                if ( not paramMaybe )
                    return unexpected( std::move( paramMaybe.error() ) );
                layout.mParams.push_back( std::move( *paramMaybe ) );
            }
        }
        else if ( type.mKind == wgsl_reflect::TypeKind::Texture and type.mSpelling == "texture_2d<f32>" )
        {
            auto defaultMaybe = TryTextureDefault( binding, label );
            if ( not defaultMaybe )
                return unexpected( std::move( defaultMaybe.error() ) );
            layout.mParams.push_back( MaterialParam{ .mName = binding.mName,
                                                     .mKind = MaterialParamKind::Texture,
                                                     .mOffset = 0,
                                                     .mComponents = 0,
                                                     .mColor = false,
                                                     .mDefault = {},
                                                     .mBinding = binding.mBinding,
                                                     .mTextureDefault = *defaultMaybe } );
        }
        else if ( type.mKind == wgsl_reflect::TypeKind::Sampler and type.mSpelling == "sampler" )
            layout.mSamplers.push_back( binding.mBinding );
        else
            return unexpected( format( "{}: {} is a {}; a material binds one uniform struct, texture_2d<f32> and "
                                       "sampler",
                                       label, binding.mName, type.mSpelling ) );
    }
    return layout;
}
}
