#include "bubble/scene/scene_file.hpp"
#include "bubble/data/value_json.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
expected<Json, string> TryEntityToJson( const Scene& scene, Entity entity, const ComponentTypes& componentTypes )
{
    Json::Object object;
    if ( entity != scene.Root() )
        object.push_back( Json::Member{ "name", Json( NameOf( scene, entity ).String() ) } );

    Json::Object components;
    for ( const ComponentTypes::Type& type : componentTypes.All() )
    {
        const MetaAny component = type.mGet( scene, entity );
        if ( not component )
            continue;
        auto valueMaybe = componentTypes.GetReflection().TryToValue( component );
        if ( not valueMaybe )
            return unexpected( format( "{}: {}", Describe( scene, entity ), valueMaybe.error() ) );
        components.push_back( Json::Member{ type.mName.ToString(), ValueToJson( *valueMaybe, type.mValueType ) } );
    }
    if ( not components.empty() )
        object.push_back( Json::Member{ "components", Json( std::move( components ) ) } );

    Json::Array children;
    for ( const Entity child : ChildrenOf( scene, entity ) )
    {
        auto childJsonMaybe = TryEntityToJson( scene, child, componentTypes );
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

// Fills a scene from the tree of a file, an entity at a time.
class SceneReader
{
public:
    SceneReader( Scene& scene, const ComponentTypes& componentTypes )
        : mScene( scene ),
          mComponentTypes( componentTypes )
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

            auto valueMaybe =
                TryValueFromJson( member.mValue, typeMaybe->mValueType, format( "{}: {}", where, member.mKey ) );
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
            const auto nameMaybe = childJson.TryFind( "name" );
            if ( not childJson.Is<Json::Object>() or not nameMaybe )
                return Fail( childJson, where, "a child with no name" );
            const auto textMaybe = nameMaybe->TryAs<string>();
            if ( not textMaybe )
                return Fail( *nameMaybe, where, format( "a name - text - expected, got {}", nameMaybe->Describe() ) );
            auto childNameMaybe = Text::TryFrom( *textMaybe );
            if ( not childNameMaybe )
                return Fail( *nameMaybe, where, childNameMaybe.error() );

            // The name as it is, or the file is wrong: CreateChild would
            // change it, and a path to it would lead nowhere.
            if ( childNameMaybe->Empty() or *textMaybe == "." or *textMaybe == ".." or
                 textMaybe->find( '/' ) != string::npos )
                return Fail(
                    *nameMaybe, where,
                    format( "\"{}\" is not a name an entity can have: not empty, no /, not . or ..", *textMaybe ) );
            if ( TryResolve( mScene, EntityPath{ *childNameMaybe }, entity ) )
                return Fail( *nameMaybe, where, format( "a second child named \"{}\"", *textMaybe ) );

            const Entity child = CreateChild( mScene, entity, *childNameMaybe );
            if ( auto readMaybe = TryReadEntity( childJson, child ); not readMaybe )
                return readMaybe;
        }
        return {};
    }

    Scene& mScene;
    const ComponentTypes& mComponentTypes;
};
}

expected<string, string> TrySaveScene( const Scene& scene, const ComponentTypes& componentTypes )
{
    auto rootMaybe = TryEntityToJson( scene, scene.Root(), componentTypes );
    if ( not rootMaybe )
        return unexpected( std::move( rootMaybe.error() ) );
    Json::Object file;
    file.push_back( Json::Member{ "version", Json( static_cast<f64>( cSceneFileVersion ) ) } );
    file.push_back( Json::Member{ "root", std::move( *rootMaybe ) } );
    return Json( std::move( file ) ).Write();
}

expected<Scene, string> TryLoadScene( string_view text, const ComponentTypes& componentTypes )
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
        if ( member.mKey != "version" and member.mKey != "root" )
            return Fail( member.mValue, "the file",
                         format( "{} is not a key of a scene (keys: version, root)", member.mKey ) );
    const auto rootMaybe = jsonMaybe->TryFind( "root" );
    if ( not rootMaybe )
        return unexpected( "no root: a scene file has \"root\": { ... }"s );

    Scene scene;
    if ( auto readMaybe = SceneReader( scene, componentTypes ).TryReadEntity( *rootMaybe, scene.Root() );
         not readMaybe )
        return unexpected( std::move( readMaybe.error() ) );
    return scene;
}
}
