#include "bubble/scene/scene_file.hpp"
#include "bubble/data/value_json.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
// How far under `json` its deepest value lies: 0 for a plain value.
int DepthBelow( const Json& json )
{
    int deepest = 0;
    if ( const auto arrayMaybe = json.TryAs<Json::Array>() )
        for ( const Json& element : *arrayMaybe )
            deepest = max( deepest, 1 + DepthBelow( element ) );
    if ( const auto objectMaybe = json.TryAs<Json::Object>() )
        for ( const Json::Member& member : *objectMaybe )
            deepest = max( deepest, 1 + DepthBelow( member.mValue ) );
    return deepest;
}

// `depth` is where the entity's object sits in the file. What the reader
// would not take, nested past Json::cMaxDepth, is refused here rather than
// written into a file that does not load. Without `instances`, an instance
// is written as the plain entities it is in memory.
expected<Json, string> TryWriteEntity( const Scene& scene,
                                       Entity entity,
                                       const FileContext& files,
                                       int depth,
                                       bool instances )
{
    const ComponentTypes& componentTypes = files.mComponentTypes;
    const auto tooDeep = [&] {
        return unexpected(
            format( "{}: nested deeper than a file holds ({} levels)", Describe( scene, entity ), Json::cMaxDepth ) );
    };
    // What an entity holds - its name first - sits one level under it.
    if ( depth + 1 > Json::cMaxDepth )
        return tooDeep();

    Json::Object object;
    if ( entity != scene.Root() )
        object.push_back( Json::Member{ "name", Json( NameOf( scene, entity ).String() ) } );

    if ( const auto instanceMaybe = scene.TryGet<PrefabInstance>( entity ); instanceMaybe and instances )
    {
        const AssetId& prefabId = instanceMaybe->mPrefab;
        if ( not files.mPrefabs )
            return unexpected( format( "{}: an instance of {}, and no prefabs to compare it with",
                                       Describe( scene, entity ), files.mAssets.Describe( prefabId ) ) );
        auto prefabMaybe = files.mPrefabs( prefabId );
        if ( not prefabMaybe )
            return unexpected( format( "{}: {}: {}", Describe( scene, entity ), files.mAssets.Describe( prefabId ),
                                       prefabMaybe.error() ) );
        auto overridesMaybe = TryInstanceOverrides( scene, entity, **prefabMaybe, files );
        if ( not overridesMaybe )
            return unexpected( std::move( overridesMaybe.error() ) );
        object.push_back( Json::Member{ "prefab", AssetToJson( prefabId, files.mAssets ) } );
        for ( Json::Member& member : *overridesMaybe )
            object.push_back( std::move( member ) );
        Json instance( std::move( object ) );
        if ( depth + DepthBelow( instance ) > Json::cMaxDepth )
            return tooDeep();
        return instance;
    }

    Json::Object components;
    for ( const ComponentTypes::Type& type : componentTypes.All() )
    {
        const MetaAny component = type.mGet( scene, entity );
        if ( not component )
            continue;
        auto valueMaybe = componentTypes.GetReflection().TryToValue( component );
        if ( not valueMaybe )
            return unexpected( format( "{}: {}", Describe( scene, entity ), valueMaybe.error() ) );
        auto jsonMaybe = TryValueToJson( *valueMaybe, type.mValueType, type.mName.ToString(), files.mAssets );
        if ( not jsonMaybe )
            return unexpected( format( "{}: {}", Describe( scene, entity ), jsonMaybe.error() ) );
        // The component sits two levels under the entity, in "components".
        if ( depth + 2 + DepthBelow( *jsonMaybe ) > Json::cMaxDepth )
            return tooDeep();
        components.push_back( Json::Member{ type.mName.ToString(), std::move( *jsonMaybe ) } );
    }
    if ( not components.empty() )
        object.push_back( Json::Member{ "components", Json( std::move( components ) ) } );

    Json::Array children;
    for ( const Entity child : ChildrenOf( scene, entity ) )
    {
        auto childJsonMaybe = TryWriteEntity( scene, child, files, depth + 2, instances );
        if ( not childJsonMaybe )
            return childJsonMaybe;
        children.push_back( std::move( *childJsonMaybe ) );
    }
    if ( not children.empty() )
        object.push_back( Json::Member{ "children", Json( std::move( children ) ) } );
    return Json( std::move( object ) );
}

unexpected<string> Fail( const Json& json, string_view where, string_view what )
{
    return unexpected( format( "{}: {}: {}", json.Where(), where, what ) );
}

// The name of a child: `where` is its parent.
expected<Text, string> TryReadName( const Json& json, string_view where )
{
    const auto nameMaybe = json.TryFind( "name" );
    if ( not json.Is<Json::Object>() or not nameMaybe )
        return Fail( json, where, "a child with no name" );
    const auto textMaybe = nameMaybe->TryAs<string>();
    if ( not textMaybe )
        return Fail( *nameMaybe, where, format( "a name - text - expected, got {}", nameMaybe->Describe() ) );
    auto childNameMaybe = Text::TryFrom( *textMaybe );
    if ( not childNameMaybe )
        return Fail( *nameMaybe, where, childNameMaybe.error() );
    if ( childNameMaybe->Empty() or *textMaybe == "." or *textMaybe == ".." or textMaybe->find( '/' ) != string::npos )
        return Fail( *nameMaybe, where,
                     format( "\"{}\" is not a name an entity can have: not empty, no /, not . or ..", *textMaybe ) );
    return childNameMaybe;
}

// Fills a scene from the tree of a file, an entity at a time.
class SceneReader
{
public:
    SceneReader( Scene& scene, const FileContext& files )
        : mScene( scene ),
          mFiles( files ),
          mComponentTypes( files.mComponentTypes )
    {
    }

    expected<void, string> TryReadEntity( const Json& json, Entity entity )
    {
        const string where( PathOf( mScene, entity ).mPath.View() );
        const auto objectMaybe = json.TryAs<Json::Object>();
        if ( not objectMaybe )
            return Fail( json, where, format( "an entity - an object - expected, got {}", json.Describe() ) );

        for ( const Json::Member& member : *objectMaybe )
        {
            if ( member.mKey == "name" )
            {
                // Read by the parent, which made this entity under it.
                if ( entity == mScene.Root() )
                    return Fail( member.mValue, where, "the root has no name" );
            }
            else if ( member.mKey == "components" )
            {
                if ( auto readMaybe = TryReadComponents( member.mValue, entity, where ); not readMaybe )
                    return readMaybe;
            }
            else if ( member.mKey == "children" )
            {
                if ( auto readMaybe = TryReadChildren( member.mValue, entity, where ); not readMaybe )
                    return readMaybe;
            }
            else
                return Fail( member.mValue, where,
                             format( "{} is not a key of an entity (keys: name, components, children)", member.mKey ) );
        }
        return {};
    }

private:
    expected<void, string> TryReadComponents( const Json& json, Entity entity, const string& where )
    {
        const auto objectMaybe = json.TryAs<Json::Object>();
        if ( not objectMaybe )
            return Fail( json, where, format( "components - an object - expected, got {}", json.Describe() ) );
        for ( const Json::Member& member : *objectMaybe )
        {
            const auto nameMaybe = Name::TryFrom( member.mKey );
            const auto typeMaybe = nameMaybe ? mComponentTypes.TryFind( *nameMaybe ) : nullopt;
            if ( not typeMaybe )
                return Fail( member.mValue, where,
                             format( "{} is not a component (components: {})", member.mKey, mComponentTypes.Names() ) );
            if ( typeMaybe->mGet( mScene, entity ) )
                return Fail( member.mValue, where, format( "{} given twice", member.mKey ) );

            auto valueMaybe = TryValueFromJson( member.mValue, typeMaybe->mValueType,
                                                format( "{}: {}", where, member.mKey ), mFiles.mAssets );
            if ( not valueMaybe )
                return unexpected( std::move( valueMaybe.error() ) );
            auto madeMaybe = mComponentTypes.GetReflection().TryFromValue( *valueMaybe, typeMaybe->mType );
            if ( not madeMaybe )
                return Fail( member.mValue, where, madeMaybe.error() );
            typeMaybe->mAdd( mScene, entity, *madeMaybe );
        }
        return {};
    }

    expected<void, string> TryReadChildren( const Json& json, Entity entity, const string& where )
    {
        const auto arrayMaybe = json.TryAs<Json::Array>();
        if ( not arrayMaybe )
            return Fail( json, where, format( "children - an array - expected, got {}", json.Describe() ) );
        for ( const Json& childJson : *arrayMaybe )
        {
            auto childNameMaybe = TryReadName( childJson, where );
            if ( not childNameMaybe )
                return unexpected( std::move( childNameMaybe.error() ) );
            // The name as it is, or the file is wrong: CreateChild would
            // change it, and a path to it would lead nowhere.
            if ( TryResolve( mScene, EntityPath{ *childNameMaybe }, entity ) )
                return Fail( *childJson.TryFind( "name" ), where,
                             format( "a second child named \"{}\"", childNameMaybe->View() ) );

            if ( const auto prefabMaybe = childJson.TryFind( "prefab" ) )
            {
                if ( auto readMaybe = TryReadInstance( childJson, *prefabMaybe, entity, *childNameMaybe, where );
                     not readMaybe )
                    return readMaybe;
                continue;
            }
            const Entity child = CreateChild( mScene, entity, *childNameMaybe );
            if ( auto readMaybe = TryReadEntity( childJson, child ); not readMaybe )
                return readMaybe;
        }
        return {};
    }

    // The prefab made under `parent`, and what the file says differs.
    expected<void, string> TryReadInstance( const Json& json,
                                            const Json& prefabJson,
                                            Entity parent,
                                            const Text& name,
                                            const string& where )
    {
        const auto prefabIdMaybe = TryAssetFromJson( prefabJson, where + ": prefab", mFiles.mAssets );
        if ( not prefabIdMaybe )
            return unexpected( prefabIdMaybe.error() );
        const string prefabName = mFiles.mAssets.Describe( *prefabIdMaybe );
        if ( not mFiles.mPrefabs )
            return Fail( prefabJson, where, format( "an instance of {}, and no prefabs to make it from", prefabName ) );
        const auto prefabMaybe = mFiles.mPrefabs( *prefabIdMaybe );
        if ( not prefabMaybe )
            return Fail( prefabJson, where, format( "{}: {}", prefabName, prefabMaybe.error() ) );
        for ( const Json::Member& member : *json.TryAs<Json::Object>() )
            if ( member.mKey != "name" and member.mKey != "prefab" and member.mKey != "set" and
                 member.mKey != "add_components" and member.mKey != "add_children" and member.mKey != "remove" )
                return Fail( member.mValue, where,
                             format( "{} is not a key of an instance (keys: name, prefab, set, add_components, "
                                     "add_children, remove)",
                                     member.mKey ) );
        const Entity root = Instantiate( mScene, **prefabMaybe, *prefabIdMaybe, parent, name, mComponentTypes );
        return TryApplyOverrides( mScene, root, *json.TryAs<Json::Object>(), mFiles );
    }

    Scene& mScene;
    const FileContext& mFiles;
    const ComponentTypes& mComponentTypes;
};
}

expected<string, string> TrySaveScene( const Scene& scene, const FileContext& files, const AssetId& assetId )
{
    // The file is at 0, its root at 1.
    auto rootMaybe = TryWriteEntity( scene, scene.Root(), files, 1, true );
    if ( not rootMaybe )
        return unexpected( std::move( rootMaybe.error() ) );
    Json::Object file;
    WriteFileHead( file, cSceneFileVersion, assetId );
    file.push_back( Json::Member{ "root", std::move( *rootMaybe ) } );
    return Json( std::move( file ) ).Write();
}

expected<Scene, string> TryLoadScene( string_view text, const FileContext& files )
{
    const auto jsonMaybe = Json::TryParse( text );
    if ( not jsonMaybe )
        return unexpected( jsonMaybe.error() );
    if ( not jsonMaybe->Is<Json::Object>() )
        return Fail( *jsonMaybe, "the file",
                     format( "a scene - an object - expected, got {}", jsonMaybe->Describe() ) );

    const auto versionMaybe = jsonMaybe->TryFind( "version" );
    if ( not versionMaybe )
        return unexpected( format( "no version: a scene file says \"version\": {}", cSceneFileVersion ) );
    const auto numberMaybe = versionMaybe->TryAs<f64>();
    if ( not numberMaybe or *numberMaybe != static_cast<f64>( static_cast<int>( *numberMaybe ) ) )
        return Fail( *versionMaybe, "version", format( "a whole number expected, got {}", versionMaybe->Describe() ) );
    if ( static_cast<int>( *numberMaybe ) != cSceneFileVersion )
        return Fail( *versionMaybe, "version",
                     format( "{} is not a version this engine reads ({})", *numberMaybe, cSceneFileVersion ) );

    for ( const Json::Member& member : *jsonMaybe->TryAs<Json::Object>() )
        if ( member.mKey != "version" and member.mKey != "asset" and member.mKey != "root" )
            return Fail( member.mValue, "the file",
                         format( "{} is not a key of a scene (keys: version, asset, root)", member.mKey ) );
    if ( auto ownIdMaybe = TryReadFileId( text ); not ownIdMaybe )
        return unexpected( std::move( ownIdMaybe.error() ) );
    const auto rootMaybe = jsonMaybe->TryFind( "root" );
    if ( not rootMaybe )
        return unexpected( "no root: a scene file has \"root\": { ... }"s );

    Scene scene;
    if ( auto readMaybe = SceneReader( scene, files ).TryReadEntity( *rootMaybe, scene.Root() ); not readMaybe )
        return unexpected( std::move( readMaybe.error() ) );
    return scene;
}

expected<Json, string> TryEntityToJson( const Scene& scene, Entity entity, const FileContext& files )
{
    if ( not InTree( scene, entity ) or entity == scene.Root() )
        throw logic_error(
            format( "TryEntityToJson of {}: an entity under the root is written", Describe( scene, entity ) ) );
    return TryWriteEntity( scene, entity, files, 0, false );
}

expected<Entity, string> TryEntityFromJson( Scene& scene,
                                            const Json& json,
                                            Entity parent,
                                            const FileContext& files,
                                            size_t index )
{
    const string where( PathOf( scene, parent ).mPath.View() );
    auto nameMaybe = TryReadName( json, where );
    if ( not nameMaybe )
        return unexpected( std::move( nameMaybe.error() ) );
    const Entity entity = CreateChild( scene, parent, *nameMaybe, index );
    if ( auto readMaybe = SceneReader( scene, files ).TryReadEntity( json, entity ); not readMaybe )
    {
        scene.Destroy( entity );
        return unexpected( std::move( readMaybe.error() ) );
    }
    return entity;
}
}
