#include "gpu_materials.hpp"
#include "bubble/core/color.hpp"
#include "bubble/core/log.hpp"
#include "bubble/render/gpu/gpu_binding.hpp"
#include "bubble/render/gpu/gpu_model.hpp"
#include "bubble/render/gpu/gpu_pipeline.hpp"
#include "bubble/render/gpu/shaders.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
constexpr string_view cStandardLabel = "standard_material.wgsl";

u32 PipelineIndex( bool transparent, bool doubleSided, bool skinned )
{
    return ( skinned ? 4u : 0u ) + ( transparent ? 2u : 0u ) + ( doubleSided ? 1u : 0u );
}

GpuTexture OneTexel( const Gpu& gpu, array<u8, 4> rgba, string_view label )
{
    GpuTexture texture( gpu, GpuTextureOptions{ .mSize = ivec2( 1 ),
                                                .mMips = 1,
                                                .mFormat = WGPUTextureFormat_RGBA8Unorm,
                                                .mUsage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
                                                .mLabel = label } );
    texture.Write( gpu, 0, AsBytes( rgba ) );
    return texture;
}

// The value a material's chain gives a parameter: the first that sets it.
OptRef<const MaterialValue> ValueOf( const vector<OptRef<const MaterialAsset>>& chain, string_view name )
{
    for ( const OptRef<const MaterialAsset>& material : chain )
        for ( const auto& [param, value] : material->mParams )
            if ( param == name )
                return value;
    return nullopt;
}

string ParamNames( const MaterialLayout& layout )
{
    string names;
    for ( const MaterialParam& param : layout.mParams )
        names += ( names.empty() ? "" : ", " ) + param.mName;
    return names.empty() ? "none" : names;
}

// Whether every value a material sets fits its shader's parameter.
expected<void, string> TryFit( const MaterialAsset& material, const MaterialLayout& layout, string_view shader )
{
    for ( const auto& [name, value] : material.mParams )
    {
        const auto paramMaybe = layout.TryFind( name );
        if ( not paramMaybe )
            return unexpected(
                format( "params.{}: {} has no such parameter (parameters: {})", name, shader, ParamNames( layout ) ) );
        if ( paramMaybe->mKind == MaterialParamKind::Texture and not value.mTexture )
            return unexpected( format( "params.{}: a texture, {{ \"asset\": id }}, not numbers", name ) );
        if ( paramMaybe->mKind == MaterialParamKind::Number and value.mNumbers.size() != paramMaybe->mComponents )
            return unexpected( format( "params.{}: {} number{}, not {}", name, paramMaybe->mComponents,
                                       paramMaybe->mComponents == 1 ? "" : "s",
                                       value.mTexture ? "a texture"s : format( "{}", value.mNumbers.size() ) ) );
    }
    return {};
}
}

MaterialShader::MaterialShader( Gpu& gpu,
                                string_view source,
                                string_view label,
                                bool trusted,
                                WGPUBindGroupLayout frame,
                                WGPUBindGroupLayout objects )
{
    auto layoutMaybe = TryReflectMaterial( source, label );
    if ( not layoutMaybe )
    {
        mState = State::Failed;
        mError = std::move( layoutMaybe.error() );
        return;
    }
    mLayout = std::move( *layoutMaybe );
    // The material's lines first, so an error's line is its file's.
    const string wgsl = format( "{}\n{}", source, EngineShader( "lit.wgsl" ) );

    vector<WGPUBindGroupLayoutEntry> entries;
    if ( mLayout.mUniformBinding )
        entries.push_back(
            BufferEntry( *mLayout.mUniformBinding, WGPUShaderStage_Fragment, WGPUBufferBindingType_Uniform ) );
    for ( const u32 sampler : mLayout.mSamplers )
        entries.push_back( SamplerEntry( sampler, WGPUSamplerBindingType_Filtering ) );
    for ( const MaterialParam& param : mLayout.mParams )
        if ( param.mKind == MaterialParamKind::Texture )
            entries.push_back( TextureEntry( param.mBinding, WGPUTextureSampleType_Float ) );

    mCheck = gpu.Check( [&] {
        mModule = CreateShader( gpu, wgsl, label );
        mGroupLayout = CreateLayout( gpu, label, entries );
        for ( const bool skinned : { false, true } )
            for ( const bool transparent : { false, true } )
                for ( const bool doubleSided : { false, true } )
                    mPipelines[PipelineIndex( transparent, doubleSided, skinned )] = CreateRenderPipeline(
                        gpu, GpuPipelineOptions{
                                 .mLabel = string( label ),
                                 .mShader = mModule.Get(),
                                 .mVertexEntry = skinned ? "vs_skinned" : "vs_main",
                                 .mFragmentEntry = "fs_main",
                                 .mBindGroupLayouts = { frame, objects, mGroupLayout.Get() },
                                 .mVertexBuffers =
                                     skinned ? vector<GpuVertexBufferLayout>{ ModelVertexLayout(), ModelSkinLayout() }
                                             : vector<GpuVertexBufferLayout>{ ModelVertexLayout() },
                                 .mColorFormat = cHdrFormat,
                                 // What is behind a transparent
                                 // surface still shows through
                                 // the next one.
                                 .mDepth = GpuDepthOptions{ .mFormat = cDepthFormat,
                                                            .mCompare = WGPUCompareFunction_Greater,
                                                            .mWrite = not transparent,
                                                            .mBias = 0,
                                                            .mBiasSlopeScale = 0 },
                                 .mTopology = WGPUPrimitiveTopology_TriangleList,
                                 .mCullMode = doubleSided ? WGPUCullMode_None : WGPUCullMode_Back,
                                 .mBlend = transparent } );
    } );
    mState = trusted ? State::Ready : State::Waiting;
}

MaterialShader::State MaterialShader::Poll()
{
    if ( mState == State::Failed or not mCheck )
        return mState;
    if ( const auto resultMaybe = mCheck->TryResult() )
    {
        if ( *resultMaybe )
            mState = State::Ready;
        else
        {
            mState = State::Failed;
            mError = resultMaybe->error();
        }
        mCheck.reset();
    }
    return mState;
}

WGPURenderPipeline MaterialShader::Pipeline( bool transparent, bool doubleSided, bool skinned ) const
{
    return mPipelines[PipelineIndex( transparent, doubleSided, skinned )].Get();
}

GpuMaterials::GpuMaterials( Gpu& gpu,
                            AssetRegistry& assets,
                            GpuAssets& gpuAssets,
                            WGPUBindGroupLayout frame,
                            WGPUBindGroupLayout objects )
    : mGpu( gpu ),
      mAssets( assets ),
      mGpuAssets( gpuAssets ),
      mFrameLayout( frame ),
      mObjectsLayout( objects )
{
    WGPUSamplerDescriptor sampler = WGPU_SAMPLER_DESCRIPTOR_INIT;
    sampler.label = ToGpuString( "material" );
    sampler.addressModeU = WGPUAddressMode_Repeat;
    sampler.addressModeV = WGPUAddressMode_Repeat;
    sampler.magFilter = WGPUFilterMode_Linear;
    sampler.minFilter = WGPUFilterMode_Linear;
    sampler.mipmapFilter = WGPUMipmapFilterMode_Linear;
    sampler.maxAnisotropy = 8;
    mSampler = GpuSamplerHandle( wgpuDeviceCreateSampler( gpu.Device(), &sampler ) );
    mWhite = OneTexel( gpu, { 255, 255, 255, 255 }, "white" );
    mBlack = OneTexel( gpu, { 0, 0, 0, 255 }, "black" );
    mFlatNormal = OneTexel( gpu, { 128, 128, 255, 255 }, "flat normal" );

    mStandardShader =
        CreateScope<MaterialShader>( gpu, EngineShader( cStandardLabel ), cStandardLabel, true, frame, objects );
    if ( mStandardShader->Poll() == MaterialShader::State::Failed )
        throw logic_error( format( "the standard material does not keep the contract: {}", mStandardShader->Error() ) );
    const MaterialAsset none;
    mStandard = Build( *mStandardShader, { none }, {}, false, false );
    // Black, and glowing: magenta whatever the light.
    const MaterialAsset placeholder{ .mShader = {},
                                     .mBase = {},
                                     .mDoubleSided = nullopt,
                                     .mTransparent = nullopt,
                                     .mParams = {
                                         { "base_color", MaterialValue{ .mNumbers = { 0, 0, 0, 1 }, .mTexture = {} } },
                                         { "emissive", MaterialValue{ .mNumbers = { 1, 0, 1 }, .mTexture = {} } },
                                     } };
    mPlaceholder = Build( *mStandardShader, { placeholder }, {}, false, true );
}

GpuMaterials::~GpuMaterials() = default;

void GpuMaterials::BeginFrame()
{
    ++mFrame;
    if ( mStandardShader->Poll() == MaterialShader::State::Failed )
        throw logic_error( format( "the standard material does not compile: {}", mStandardShader->Error() ) );
}

WGPUTextureView GpuMaterials::DefaultTexture( TextureDefault kind ) const
{
    switch ( kind )
    {
        case TextureDefault::Black: return mBlack.View();
        case TextureDefault::Normal: return mFlatNormal.View();
        case TextureDefault::White: break;
    }
    return mWhite.View();
}

const GpuMaterial& GpuMaterials::For( const AssetRef<MaterialAsset>& materialRef )
{
    if ( not materialRef or materialRef.State() == AssetState::Loading )
        return mStandard;
    Entry& entry = mEntries[materialRef.Entry().Id()];
    if ( entry.mFrame != mFrame )
    {
        entry.mFrame = mFrame;
        if ( materialRef.State() == AssetState::Failed )
            Fail( entry, materialRef.Entry().Error() );
        else
            Resolve( entry, materialRef );
    }
    switch ( entry.mUse )
    {
        case Use::Own: return *entry.mBuilt;
        case Use::Placeholder: return mPlaceholder;
        case Use::Standard: break;
    }
    return mStandard;
}

void GpuMaterials::Fail( Entry& entry, const string& reason )
{
    entry.mUse = Use::Placeholder;
    if ( entry.mLogged == reason )
        return;
    LogError( "{}", reason );
    entry.mLogged = reason;
}

GpuMaterials::ShaderCopy& GpuMaterials::ShaderFor( const AssetRef<ShaderAsset>& shaderRef )
{
    const AssetEntryBase& shaderEntry = shaderRef.Entry();
    ShaderCopy& copy = mShaders[shaderEntry.Id()];
    if ( not copy.mShader or copy.mVersion != shaderEntry.Version() )
    {
        copy.mShader = CreateScope<MaterialShader>( mGpu, shaderRef.TryGet()->mSource, shaderEntry.Path().View(), false,
                                                    mFrameLayout, mObjectsLayout );
        copy.mVersion = shaderEntry.Version();
        copy.mGeneration = ++mGenerations;
    }
    return copy;
}

void GpuMaterials::Resolve( Entry& entry, const AssetRef<MaterialAsset>& materialRef )
{
    // Whatever it is waiting for keeps loading meanwhile: the refs stay. A
    // ref is let go only once the new one is held - the last ref going
    // would drop the entry, and the load would start over every frame.
    entry.mUse = Use::Standard;
    const string_view path = materialRef.Entry().Path().View();
    vector<AssetRef<MaterialAsset>> chainRefs{ materialRef };
    vector<OptRef<const MaterialAsset>> chain{ *materialRef.TryGet() };
    vector<Seen> seen{ Seen{ materialRef.Entry().Id(), materialRef.Entry().Version(), true } };
    string trail( path );
    while ( const AssetId base = chain.back()->mBase )
    {
        const AssetRef<MaterialAsset> baseRef = mAssets.Load<MaterialAsset>( base );
        chainRefs.push_back( baseRef );
        entry.mChain = chainRefs;
        trail += format( ", {}", baseRef.Entry().Path().View() );
        for ( const Seen& before : seen )
            if ( before.mId == base )
                return Fail( entry, format( "{}: its bases go round: {}", path, trail ) );
        if ( baseRef.State() == AssetState::Failed )
            return Fail( entry, format( "{}: its base {}", path, baseRef.Entry().Error() ) );
        if ( not baseRef.Ready() )
            return;
        chain.push_back( *baseRef.TryGet() );
        seen.push_back( Seen{ base, baseRef.Entry().Version(), true } );
    }
    entry.mChain = chainRefs;

    // The shader of the chain's root: the material's own, or the standard.
    OptRef<MaterialShader> shader = *mStandardShader;
    string shaderName( cStandardLabel );
    const AssetId shaderId = chain.back()->mShader;
    entry.mShader = shaderId ? mAssets.Load<ShaderAsset>( shaderId ) : AssetRef<ShaderAsset>();
    if ( shaderId )
    {
        if ( entry.mShader.State() == AssetState::Failed )
            return Fail( entry, format( "{}: its shader {}", path, entry.mShader.Entry().Error() ) );
        if ( not entry.mShader.Ready() )
            return;
        ShaderCopy& copy = ShaderFor( entry.mShader );
        shader = OptRef<MaterialShader>( *copy.mShader );
        shaderName = string( entry.mShader.Entry().Path().View() );
        seen.push_back( Seen{ shaderId, copy.mGeneration, true } );
    }
    switch ( shader->Poll() )
    {
        case MaterialShader::State::Failed: return Fail( entry, shader->Error() );
        case MaterialShader::State::Waiting: return;
        case MaterialShader::State::Ready: break;
    }
    const MaterialLayout& layout = shader->Layout();
    for ( size_t at = 0; at < chain.size(); ++at )
        if ( auto fitMaybe = TryFit( *chain[at], layout, shaderName ); not fitMaybe )
            return Fail( entry, format( "{}: {}", chainRefs[at].Entry().Path().View(), fitMaybe.error() ) );

    // A texture that has not come yet is its default meanwhile.
    vector<WGPUTextureView> textures;
    vector<AssetRef<TextureAsset>> textureRefs;
    for ( const MaterialParam& param : layout.mParams )
    {
        if ( param.mKind != MaterialParamKind::Texture )
            continue;
        const auto valueMaybe = ValueOf( chain, param.mName );
        if ( not valueMaybe )
        {
            textures.push_back( DefaultTexture( param.mTextureDefault ) );
            continue;
        }
        const AssetRef<TextureAsset> textureRef = mAssets.Load<TextureAsset>( valueMaybe->mTexture );
        textureRefs.push_back( textureRef );
        const auto textureMaybe = mGpuAssets.TryTexture( textureRef );
        if ( textureRef.State() == AssetState::Failed )
            LogError( "{}: params.{}: {}", path, param.mName, textureRef.Entry().Error() );
        textures.push_back( textureMaybe ? textureMaybe->View() : DefaultTexture( param.mTextureDefault ) );
        seen.push_back(
            Seen{ valueMaybe->mTexture, textureRef.Ready() ? textureRef.Entry().Version() : 0u, textureRef.Ready() } );
    }
    entry.mTextures = std::move( textureRefs );

    entry.mUse = Use::Own;
    if ( entry.mBuilt and entry.mSeen == seen )
        return;
    const auto flagOf = [&]( opt<bool> MaterialAsset::* flag ) {
        for ( const OptRef<const MaterialAsset>& material : chain )
            if ( const opt<bool>& value = ( *material ).*flag )
                return *value;
        return false;
    };
    entry.mBuilt = Build( *shader, chain, textures, flagOf( &MaterialAsset::mTransparent ),
                          flagOf( &MaterialAsset::mDoubleSided ) );
    entry.mSeen = std::move( seen );
    entry.mLogged.clear();
}

GpuMaterial GpuMaterials::Build( const MaterialShader& shader,
                                 const vector<OptRef<const MaterialAsset>>& chain,
                                 const vector<WGPUTextureView>& textures,
                                 bool transparent,
                                 bool doubleSided )
{
    const MaterialLayout& layout = shader.Layout();
    GpuMaterial material{ .mPipeline = shader.Pipeline( transparent, doubleSided, false ),
                          .mSkinnedPipeline = shader.Pipeline( transparent, doubleSided, true ),
                          .mTransparent = transparent,
                          .mDoubleSided = doubleSided,
                          .mUniforms = {},
                          .mGroup = {} };
    vector<WGPUBindGroupEntry> entries;
    if ( layout.mUniformBinding )
    {
        vector<f32> block( layout.mUniformSize / sizeof( f32 ), 0.0f );
        for ( const MaterialParam& param : layout.mParams )
        {
            if ( param.mKind != MaterialParamKind::Number )
                continue;
            const auto valueMaybe = ValueOf( chain, param.mName );
            const vector<f32>& numbers = valueMaybe ? valueMaybe->mNumbers : param.mDefault;
            for ( u32 component = 0; component < param.mComponents; ++component )
            {
                // A color is picked in sRGB; its opacity is no color.
                const bool linear = param.mColor and component < 3;
                block[param.mOffset / sizeof( f32 ) + component] =
                    linear ? SrgbToLinear( numbers[component] ) : numbers[component];
            }
        }
        material.mUniforms = GpuBuffer( mGpu, WGPUBufferUsage_Uniform, AsBytes( block ), "material" );
        entries.push_back( BufferBinding( *layout.mUniformBinding, material.mUniforms ) );
    }
    for ( const u32 sampler : layout.mSamplers )
        entries.push_back( SamplerBinding( sampler, mSampler.Get() ) );
    size_t texture = 0;
    for ( const MaterialParam& param : layout.mParams )
        if ( param.mKind == MaterialParamKind::Texture )
            entries.push_back( TextureBinding( param.mBinding, texture < textures.size()
                                                                   ? textures[texture++]
                                                                   : DefaultTexture( param.mTextureDefault ) ) );
    material.mGroup = CreateGroup( mGpu, "material", shader.GroupLayout(), entries );
    return material;
}
}
