#include "bubble/scene/prefab.hpp"
#include "bubble/core/log.hpp"
#include "bubble/data/value_json.hpp"
#include "bubble/scene/scene_file.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
EntityPath RootPath()
{
    return EntityPath{ *Text::TryFrom( "/" ) };
}

// Every absolute link in `value` under `from` moved under `to`; the others
// stay as they are.
void RebaseLinks( Value& value, const EntityPath& from, const EntityPath& to )
{
    if ( const auto linkMaybe = value.TryAs<EntityPath>() )
    {
        if ( auto rebasedMaybe = TryRebase( *linkMaybe, from, to ) )
            value = Value( std::move( *rebasedMaybe ) );
    }
    else if ( const auto listMaybe = value.TryAs<Value::List>() )
    {
        for ( Value& element : *listMaybe )
            RebaseLinks( element, from, to );
    }
    else if ( const auto recordMaybe = value.TryAs<Value::Record>() )
    {
        for ( ValueField& field : *recordMaybe )
            RebaseLinks( field.mValue, from, to );
    }
}

unexpected<string> Fail( const Json& json, string_view where, string_view what )
{
    return unexpected( format( "{}: {}: {}", json.Where(), where, what ) );
}

expected<u32, string> TryReadId( const Json& json, string_view where )
{
    const auto numberMaybe = json.TryAs<f64>();
    if ( not numberMaybe or *numberMaybe < 0 or *numberMaybe != static_cast<f64>( static_cast<u32>( *numberMaybe ) ) )
        return Fail( json, where, format( "an id - a whole number from 0 - expected, got {}", json.Describe() ) );
    return static_cast<u32>( *numberMaybe );
}

// A component's value made whole: a field the file left out takes its
// default, so a prefab and an instance compare field by field.
expected<Value, string> TryWhole( const Value& value,
                                  const ComponentTypes::Type& type,
                                  const ComponentTypes& componentTypes )
{
    Reflection& reflection = componentTypes.GetReflection();
    auto madeMaybe = reflection.TryFromValue( value, type.mType );
    if ( not madeMaybe )
        return unexpected( std::move( madeMaybe.error() ) );
    return reflection.TryToValue( *madeMaybe );
}

// Reads the nodes of a prefab file into the order Prefab keeps them.
class PrefabReader
{
public:
    PrefabReader( const ComponentTypes& componentTypes, vector<Prefab::Node>& nodes, hmap<u32, size_t>& index )
        : mComponentTypes( componentTypes ),
          mNodes( nodes ),
          mIndex( index )
    {
    }

    expected<u32, string> TryReadNode( const Json& json, bool root, const string& where )
    {
        const auto objectMaybe = json.TryAs<Json::Object>();
        if ( not objectMaybe )
            return Fail( json, where, format( "a node - an object - expected, got {}", json.Describe() ) );
        const auto idJsonMaybe = json.TryFind( "id" );
        if ( not idJsonMaybe )
            return Fail( json, where, "a node with no id" );
        const auto idMaybe = TryReadId( *idJsonMaybe, where );
        if ( not idMaybe )
            return unexpected( idMaybe.error() );
        if ( root and *idMaybe != 0 )
            return Fail( *idJsonMaybe, where, "the root's id is 0" );
        if ( mIndex.contains( *idMaybe ) )
            return Fail( *idJsonMaybe, where, format( "a second node with the id {}", *idMaybe ) );

        const size_t at = mNodes.size();
        mIndex[*idMaybe] = at;
        mNodes.push_back( Prefab::Node{ .mId = *idMaybe, .mName = {}, .mComponents = {}, .mChildren = {} } );
        for ( const Json::Member& member : *objectMaybe )
        {
            if ( member.mKey == "id" )
                continue;
            if ( member.mKey == "name" )
            {
                // Read by the parent.
                if ( root )
                    return Fail( member.mValue, where, "the root has no name: an instance is named by its level" );
            }
            else if ( member.mKey == "components" )
            {
                auto componentsMaybe = TryReadComponents( member.mValue, where );
                if ( not componentsMaybe )
                    return unexpected( std::move( componentsMaybe.error() ) );
                mNodes[at].mComponents = std::move( *componentsMaybe );
            }
            else if ( member.mKey == "children" )
            {
                auto childrenMaybe = TryReadChildren( member.mValue, where );
                if ( not childrenMaybe )
                    return unexpected( std::move( childrenMaybe.error() ) );
                mNodes[at].mChildren = std::move( *childrenMaybe );
            }
            else
                return Fail(
                    member.mValue, where,
                    format( "{} is not a key of a node (keys: id, name, components, children)", member.mKey ) );
        }
        return *idMaybe;
    }

private:
    expected<vector<pair<Name, Value>>, string> TryReadComponents( const Json& json, const string& where )
    {
        const auto objectMaybe = json.TryAs<Json::Object>();
        if ( not objectMaybe )
            return Fail( json, where, format( "components - an object - expected, got {}", json.Describe() ) );
        vector<pair<Name, Value>> components;
        for ( const ComponentTypes::Type& type : mComponentTypes.All() )
            for ( const Json::Member& member : *objectMaybe )
                if ( member.mKey == type.mName.ToString() )
                {
                    if ( ranges::find( components, type.mName, &pair<Name, Value>::first ) != components.end() )
                        return Fail( member.mValue, where, format( "{} given twice", member.mKey ) );
                    auto valueMaybe =
                        TryValueFromJson( member.mValue, type.mValueType, format( "{}: {}", where, member.mKey ) );
                    if ( not valueMaybe )
                        return unexpected( std::move( valueMaybe.error() ) );
                    auto wholeMaybe = TryWhole( *valueMaybe, type, mComponentTypes );
                    if ( not wholeMaybe )
                        return Fail( member.mValue, where, wholeMaybe.error() );
                    components.emplace_back( type.mName, std::move( *wholeMaybe ) );
                }
        for ( const Json::Member& member : *objectMaybe )
        {
            const auto nameMaybe = Name::TryFrom( member.mKey );
            if ( not nameMaybe or not mComponentTypes.TryFind( *nameMaybe ) )
                return Fail( member.mValue, where,
                             format( "{} is not a component (components: {})", member.mKey, mComponentTypes.Names() ) );
        }
        return components;
    }

    expected<vector<u32>, string> TryReadChildren( const Json& json, const string& where )
    {
        const auto arrayMaybe = json.TryAs<Json::Array>();
        if ( not arrayMaybe )
            return Fail( json, where, format( "children - an array - expected, got {}", json.Describe() ) );
        vector<u32> children;
        vector<string> names;
        for ( const Json& childJson : *arrayMaybe )
        {
            const auto nameJsonMaybe = childJson.TryFind( "name" );
            const auto textMaybe = nameJsonMaybe ? nameJsonMaybe->TryAs<string>() : nullopt;
            if ( not textMaybe )
                return Fail( childJson, where, "a child with no name" );
            auto nameMaybe = Text::TryFrom( *textMaybe );
            if ( not nameMaybe or nameMaybe->Empty() or *textMaybe == "." or *textMaybe == ".." or
                 textMaybe->find( '/' ) != string::npos )
                return Fail(
                    *nameJsonMaybe, where,
                    format( "\"{}\" is not a name an entity can have: not empty, no /, not . or ..", *textMaybe ) );
            if ( ranges::find( names, *textMaybe ) != names.end() )
                return Fail( *nameJsonMaybe, where, format( "a second child named \"{}\"", *textMaybe ) );
            names.push_back( *textMaybe );
            const string childWhere = where == "/" ? "/" + *textMaybe : where + "/" + *textMaybe;
            auto idMaybe = TryReadNode( childJson, false, childWhere );
            if ( not idMaybe )
                return unexpected( std::move( idMaybe.error() ) );
            mNodes[mIndex[*idMaybe]].mName = std::move( *nameMaybe );
            children.push_back( *idMaybe );
        }
        return children;
    }

    const ComponentTypes& mComponentTypes;
    vector<Prefab::Node>& mNodes;
    hmap<u32, size_t>& mIndex;
};

// The entities of the instance at `root`, by node.
hmap<u32, Entity> MadeFrom( const Scene& scene, Entity root )
{
    hmap<u32, Entity> made;
    vector<Entity> pending{ root };
    while ( not pending.empty() )
    {
        const Entity entity = pending.back();
        pending.pop_back();
        const auto linkMaybe = scene.TryGet<PrefabLink>( entity );
        if ( linkMaybe and linkMaybe->mInstance == root )
            made[linkMaybe->mNode] = entity;
        for ( const Entity child : ChildrenOf( scene, entity ) )
            pending.push_back( child );
    }
    return made;
}

// The fields where `instance` differs from `base`, as "light.shadow.bias";
// a record field by field, anything else whole.
expected<void, string> TryDiff( const Value& base,
                                const Value& instance,
                                const ValueType& type,
                                const string& path,
                                Json::Object& out )
{
    const auto baseRecordMaybe = base.TryAs<Value::Record>();
    const auto instanceRecordMaybe = instance.TryAs<Value::Record>();
    if ( type.mKind == ValueKind::Record and baseRecordMaybe and instanceRecordMaybe )
    {
        for ( const ValueTypeField& field : type.mFields )
        {
            const auto inBase = ranges::find( *baseRecordMaybe, field.mName, &ValueField::mName );
            const auto inInstance = ranges::find( *instanceRecordMaybe, field.mName, &ValueField::mName );
            if ( inInstance == instanceRecordMaybe->end() )
                continue;
            const string fieldPath = path + "." + field.mName.ToString();
            if ( inBase != baseRecordMaybe->end() )
            {
                if ( auto diffedMaybe = TryDiff( inBase->mValue, inInstance->mValue, field.mType, fieldPath, out );
                     not diffedMaybe )
                    return diffedMaybe;
                continue;
            }
            auto jsonMaybe = TryValueToJson( inInstance->mValue, field.mType, fieldPath );
            if ( not jsonMaybe )
                return unexpected( std::move( jsonMaybe.error() ) );
            out.push_back( Json::Member{ fieldPath, std::move( *jsonMaybe ) } );
        }
        return {};
    }
    if ( base == instance )
        return {};
    auto jsonMaybe = TryValueToJson( instance, type, path );
    if ( not jsonMaybe )
        return unexpected( std::move( jsonMaybe.error() ) );
    out.push_back( Json::Member{ path, std::move( *jsonMaybe ) } );
    return {};
}

// What `path` reaches in a value of `type`.
expected<ValueType, string> TryTypeAt( const ValueType& type, const FieldPath& path )
{
    const ValueType* at = &type;
    for ( size_t i = 0; i < path.Steps().size(); ++i )
    {
        if ( at->mKind == ValueKind::Any )
            return *at;
        const FieldPath::Step& step = path.Steps()[i];
        if ( const auto name = get_if<Name>( &step ) )
        {
            const auto field = ranges::find( at->mFields, *name, &ValueTypeField::mName );
            if ( at->mKind != ValueKind::Record or field == at->mFields.end() )
                return unexpected( format( "{}: not a field", path.Prefix( i + 1 ) ) );
            at = &field->mType;
        }
        else
        {
            if ( at->mKind != ValueKind::List )
                return unexpected( format( "{}: not a list", path.Prefix( i + 1 ) ) );
            at = at->mElement.get();
        }
    }
    return *at;
}

expected<Json, string> TryNodeToJson( const Prefab& prefab,
                                      const Prefab::Node& node,
                                      const ComponentTypes& componentTypes )
{
    Json::Object object;
    object.push_back( Json::Member{ "id", Json( static_cast<f64>( node.mId ) ) } );
    if ( not node.mName.Empty() )
        object.push_back( Json::Member{ "name", Json( node.mName.String() ) } );
    Json::Object components;
    for ( const auto& [name, value] : node.mComponents )
    {
        auto jsonMaybe = TryValueToJson( value, componentTypes.TryFind( name )->mValueType, name.ToString() );
        if ( not jsonMaybe )
            return unexpected( format( "node {}: {}", node.mId, jsonMaybe.error() ) );
        components.push_back( Json::Member{ name.ToString(), std::move( *jsonMaybe ) } );
    }
    if ( not components.empty() )
        object.push_back( Json::Member{ "components", Json( std::move( components ) ) } );
    Json::Array children;
    for ( const u32 child : node.mChildren )
    {
        auto childMaybe = TryNodeToJson( prefab, *prefab.TryFind( child ), componentTypes );
        if ( not childMaybe )
            return childMaybe;
        children.push_back( std::move( *childMaybe ) );
    }
    if ( not children.empty() )
        object.push_back( Json::Member{ "children", Json( std::move( children ) ) } );
    return Json( std::move( object ) );
}
}

expected<Prefab, string> Prefab::TryParse( string_view text, const ComponentTypes& componentTypes )
{
    const auto jsonMaybe = Json::TryParse( text );
    if ( not jsonMaybe )
        return unexpected( jsonMaybe.error() );
    const auto fileMaybe = jsonMaybe->TryAs<Json::Object>();
    if ( not fileMaybe )
        return Fail( *jsonMaybe, "the file",
                     format( "a prefab - an object - expected, got {}", jsonMaybe->Describe() ) );
    for ( const Json::Member& member : *fileMaybe )
        if ( member.mKey != "version" and member.mKey != "next_id" and member.mKey != "root" )
            return Fail( member.mValue, "the file",
                         format( "{} is not a key of a prefab (keys: version, next_id, root)", member.mKey ) );

    const auto versionMaybe = jsonMaybe->TryFind( "version" );
    if ( not versionMaybe )
        return unexpected( format( "no version: a prefab file says \"version\": {}", cPrefabFileVersion ) );
    const auto versionNumberMaybe = versionMaybe->TryAs<f64>();
    if ( not versionNumberMaybe or *versionNumberMaybe != cPrefabFileVersion )
        return Fail(
            *versionMaybe, "version",
            format( "{} is not a version this engine reads ({})", versionMaybe->Describe(), cPrefabFileVersion ) );
    const auto nextJsonMaybe = jsonMaybe->TryFind( "next_id" );
    if ( not nextJsonMaybe )
        return unexpected( "no next_id: a prefab file says which id its next node gets"s );
    const auto nextMaybe = TryReadId( *nextJsonMaybe, "next_id" );
    if ( not nextMaybe )
        return unexpected( nextMaybe.error() );
    const auto rootMaybe = jsonMaybe->TryFind( "root" );
    if ( not rootMaybe )
        return unexpected( "no root: a prefab file has \"root\": { \"id\": 0, ... }"s );

    Prefab prefab;
    if ( auto readMaybe =
             PrefabReader( componentTypes, prefab.mNodes, prefab.mIndex ).TryReadNode( *rootMaybe, true, "/" );
         not readMaybe )
        return unexpected( std::move( readMaybe.error() ) );
    u32 highest = 0;
    for ( const Node& node : prefab.mNodes )
        highest = max( highest, node.mId );
    if ( *nextMaybe <= highest )
        return Fail( *nextJsonMaybe, "next_id",
                     format( "{} is not past every id: the highest is {}", *nextMaybe, highest ) );
    prefab.mNextId = *nextMaybe;
    return prefab;
}

expected<Prefab, string> Prefab::TryFromScene( const Scene& scene,
                                               Entity root,
                                               const ComponentTypes& componentTypes,
                                               hmap<Entity, u32>& ids,
                                               u32 nextId )
{
    const EntityPath rootPath = PathOf( scene, root );
    Prefab prefab;
    prefab.mNextId = max( nextId, 1u );
    ids[root] = 0;
    for ( const auto& [entity, id] : ids )
        prefab.mNextId = max( prefab.mNextId, id + 1 );

    // Parents before children; each node's children filled in as they come.
    vector<pair<Entity, size_t>> pending{ { root, 0 } };
    while ( not pending.empty() )
    {
        const auto [entity, parentAt] = pending.back();
        pending.pop_back();
        if ( entity != root and scene.Has<PrefabInstance>( entity ) )
            return unexpected( format( "{} is an instance of {}: a prefab inside a prefab is not there yet",
                                       Describe( scene, entity ),
                                       scene.Get<PrefabInstance>( entity ).mPrefab.View() ) );
        const auto known = ids.find( entity );
        const u32 id = known != ids.end() ? known->second : prefab.mNextId++;
        ids[entity] = id;
        Node node{
            .mId = id, .mName = entity == root ? Text() : NameOf( scene, entity ), .mComponents = {}, .mChildren = {}
        };
        for ( const ComponentTypes::Type& type : componentTypes.All() )
            if ( const MetaAny component = type.mGet( scene, entity ) )
            {
                auto valueMaybe = componentTypes.GetReflection().TryToValue( component );
                if ( not valueMaybe )
                    return unexpected( format( "{}: {}", Describe( scene, entity ), valueMaybe.error() ) );
                RebaseLinks( *valueMaybe, rootPath, RootPath() );
                node.mComponents.emplace_back( type.mName, std::move( *valueMaybe ) );
            }
        const size_t at = prefab.mNodes.size();
        prefab.mIndex[id] = at;
        prefab.mNodes.push_back( std::move( node ) );
        if ( entity != root )
            prefab.mNodes[parentAt].mChildren.push_back( id );
        const span<const Entity> children = ChildrenOf( scene, entity );
        for ( auto child = children.rbegin(); child != children.rend(); ++child )
            pending.emplace_back( *child, at );
    }
    return prefab;
}

expected<string, string> Prefab::TryWrite( const ComponentTypes& componentTypes ) const
{
    auto rootMaybe = TryNodeToJson( *this, Root(), componentTypes );
    if ( not rootMaybe )
        return unexpected( std::move( rootMaybe.error() ) );
    Json::Object file;
    file.push_back( Json::Member{ "version", Json( static_cast<f64>( cPrefabFileVersion ) ) } );
    file.push_back( Json::Member{ "next_id", Json( static_cast<f64>( mNextId ) ) } );
    file.push_back( Json::Member{ "root", std::move( *rootMaybe ) } );
    return Json( std::move( file ) ).Write();
}

OptRef<const Prefab::Node> Prefab::TryFind( u32 id ) const
{
    const auto found = mIndex.find( id );
    if ( found == mIndex.end() )
        return nullopt;
    return mNodes[found->second];
}

namespace
{
// The entities of `node` and what is under it, from `entity` down; links
// from the prefab's root lead from `rootPath`. `made` hears of each.
void MakeNodes( Scene& scene,
                const Prefab& prefab,
                const Prefab::Node& node,
                Entity entity,
                const EntityPath& rootPath,
                const ComponentTypes& componentTypes,
                const function<void( Entity, u32 )>& made )
{
    made( entity, node.mId );
    for ( const auto& [componentName, value] : node.mComponents )
    {
        const auto typeMaybe = componentTypes.TryFind( componentName );
        if ( not typeMaybe )
            throw logic_error( format( "a prefab read with another set of components: {}", componentName.ToString() ) );
        auto madeMaybe = componentTypes.GetReflection().TryFromValue( ToInstance( value, rootPath ), typeMaybe->mType );
        if ( not madeMaybe )
            throw logic_error( format( "a prefab's value it read itself: {}", madeMaybe.error() ) );
        typeMaybe->mAdd( scene, entity, *madeMaybe );
    }
    for ( const u32 childId : node.mChildren )
    {
        const Prefab::Node& child = *prefab.TryFind( childId );
        MakeNodes( scene, prefab, child, CreateChild( scene, entity, child.mName ), rootPath, componentTypes, made );
    }
}
}

void Prefab::SetComponents( u32 id, vector<pair<Name, Value>> components )
{
    const auto found = mIndex.find( id );
    if ( found == mIndex.end() )
        throw logic_error( format( "Prefab::SetComponents of node {}, which it does not have", id ) );
    mNodes[found->second].mComponents = std::move( components );
}

Entity Instantiate( Scene& scene,
                    const Prefab& prefab,
                    const AssetPath& path,
                    Entity parent,
                    const Text& name,
                    const ComponentTypes& componentTypes,
                    size_t index )
{
    const Entity root = CreateChild( scene, parent, name, index );
    MakeNodes( scene, prefab, prefab.Root(), root, PathOf( scene, root ), componentTypes, [&]( Entity entity, u32 id ) {
        scene.Add<PrefabLink>( entity, PrefabLink{ .mInstance = root, .mNode = id } );
    } );
    scene.Add<PrefabInstance>( root, PrefabInstance{ path } );
    return root;
}

Scene SceneOf( const Prefab& prefab, const ComponentTypes& componentTypes, hmap<Entity, u32>& ids )
{
    Scene scene;
    MakeNodes( scene, prefab, prefab.Root(), scene.Root(), RootPath(), componentTypes,
               [&]( Entity entity, u32 id ) { ids[entity] = id; } );
    return scene;
}

Value ToInstance( Value value, const EntityPath& root )
{
    RebaseLinks( value, RootPath(), root );
    return value;
}

Value FromInstance( Value value, const EntityPath& root )
{
    RebaseLinks( value, root, RootPath() );
    return value;
}

expected<Json::Object, string> TryInstanceOverrides( const Scene& scene,
                                                     Entity root,
                                                     const Prefab& prefab,
                                                     const ComponentTypes& componentTypes )
{
    if ( not scene.Has<PrefabInstance>( root ) )
        throw logic_error( format( "TryInstanceOverrides of {}, which is no instance", Describe( scene, root ) ) );
    const EntityPath rootPath = PathOf( scene, root );
    const hmap<u32, Entity> made = MadeFrom( scene, root );
    Reflection& reflection = componentTypes.GetReflection();

    Json::Object set;
    Json::Object addComponents;
    Json::Object addChildren;
    Json::Array remove;
    hset<u32> gone;
    hmap<u32, u32> parentOf;
    for ( const Prefab::Node& node : prefab.Nodes() )
        for ( const u32 child : node.mChildren )
            parentOf[child] = node.mId;

    for ( const Prefab::Node& node : prefab.Nodes() )
    {
        const string id = format( "{}", node.mId );
        // Under a node that went, it went with it.
        if ( const auto parent = parentOf.find( node.mId );
             parent != parentOf.end() and gone.contains( parent->second ) )
        {
            gone.insert( node.mId );
            continue;
        }
        const auto found = made.find( node.mId );
        if ( found == made.end() )
        {
            gone.insert( node.mId );
            remove.push_back( Json( id ) );
            continue;
        }
        const Entity entity = found->second;
        Json::Object nodeSet;
        Json::Object nodeAdded;
        for ( const ComponentTypes::Type& type : componentTypes.All() )
        {
            const auto inBase = ranges::find( node.mComponents, type.mName, &pair<Name, Value>::first );
            const MetaAny component = type.mGet( scene, entity );
            if ( not component )
            {
                if ( inBase != node.mComponents.end() )
                    remove.push_back( Json( format( "{}:{}", id, type.mName.ToString() ) ) );
                continue;
            }
            auto valueMaybe = reflection.TryToValue( component );
            if ( not valueMaybe )
                return unexpected( format( "{}: {}", Describe( scene, entity ), valueMaybe.error() ) );
            if ( inBase == node.mComponents.end() )
            {
                auto jsonMaybe = TryValueToJson( *valueMaybe, type.mValueType, type.mName.ToString() );
                if ( not jsonMaybe )
                    return unexpected( format( "{}: {}", Describe( scene, entity ), jsonMaybe.error() ) );
                nodeAdded.push_back( Json::Member{ type.mName.ToString(), std::move( *jsonMaybe ) } );
                continue;
            }
            Value base = inBase->second;
            RebaseLinks( base, RootPath(), rootPath );
            if ( auto diffedMaybe = TryDiff( base, *valueMaybe, type.mValueType, type.mName.ToString(), nodeSet );
                 not diffedMaybe )
                return unexpected( format( "{}: {}", Describe( scene, entity ), diffedMaybe.error() ) );
        }
        if ( not nodeSet.empty() )
            set.push_back( Json::Member{ id, Json( std::move( nodeSet ) ) } );
        if ( not nodeAdded.empty() )
            addComponents.push_back( Json::Member{ id, Json( std::move( nodeAdded ) ) } );

        Json::Array children;
        for ( const Entity child : ChildrenOf( scene, entity ) )
        {
            const auto linkMaybe = scene.TryGet<PrefabLink>( child );
            if ( linkMaybe and linkMaybe->mInstance == root )
                continue;
            auto childMaybe = TryEntityToJson( scene, child, componentTypes );
            if ( not childMaybe )
                return unexpected( std::move( childMaybe.error() ) );
            children.push_back( std::move( *childMaybe ) );
        }
        if ( not children.empty() )
            addChildren.push_back( Json::Member{ id, Json( std::move( children ) ) } );
    }

    Json::Object overrides;
    if ( not set.empty() )
        overrides.push_back( Json::Member{ "set", Json( std::move( set ) ) } );
    if ( not addComponents.empty() )
        overrides.push_back( Json::Member{ "add_components", Json( std::move( addComponents ) ) } );
    if ( not addChildren.empty() )
        overrides.push_back( Json::Member{ "add_children", Json( std::move( addChildren ) ) } );
    if ( not remove.empty() )
        overrides.push_back( Json::Member{ "remove", Json( std::move( remove ) ) } );
    return overrides;
}

expected<void, string> TryApplyOverrides( Scene& scene,
                                          Entity root,
                                          const Json::Object& overrides,
                                          const ComponentTypes& componentTypes )
{
    if ( not scene.Has<PrefabInstance>( root ) )
        throw logic_error( format( "TryApplyOverrides on {}, which is no instance", Describe( scene, root ) ) );
    const string where( PathOf( scene, root ).mPath.View() );
    const AssetPath& prefabPath = scene.Get<PrefabInstance>( root ).mPrefab;
    const hmap<u32, Entity> made = MadeFrom( scene, root );
    Reflection& reflection = componentTypes.GetReflection();
    const auto find = [&]( string_view key ) -> OptRef<const Json> {
        for ( const Json::Member& member : overrides )
            if ( member.mKey == key )
                return member.mValue;
        return nullopt;
    };
    // The entity of a node the prefab has still, or nothing - and a warning.
    const auto node = [&]( const Json& json, string_view id ) -> expected<opt<Entity>, string> {
        const auto numberMaybe = TryParse<u32>( id );
        if ( not numberMaybe )
            return Fail( json, where, format( "\"{}\" is not a node's id", id ) );
        const auto found = made.find( *numberMaybe );
        if ( found == made.end() or not scene.Alive( found->second ) )
        {
            LogWarning( "{}: {} has no node {} any more; what the level says of it is dropped", where,
                        prefabPath.View(), id );
            return opt<Entity>();
        }
        return opt<Entity>( found->second );
    };
    const auto missing = [&]( string_view id, string_view component ) {
        LogWarning( "{}: node {} of {} has no {} any more; what the level says of it is dropped", where, id,
                    prefabPath.View(), component );
    };

    if ( const auto removeMaybe = find( "remove" ) )
    {
        const auto arrayMaybe = removeMaybe->TryAs<Json::Array>();
        if ( not arrayMaybe )
            return Fail( *removeMaybe, where,
                         format( "remove - an array - expected, got {}", removeMaybe->Describe() ) );
        for ( const Json& item : *arrayMaybe )
        {
            const auto textMaybe = item.TryAs<string>();
            if ( not textMaybe )
                return Fail( item, where, format( "a node, or a node:component, expected, got {}", item.Describe() ) );
            const size_t colon = textMaybe->find( ':' );
            const string id = textMaybe->substr( 0, colon );
            auto entityMaybe = node( item, id );
            if ( not entityMaybe )
                return unexpected( std::move( entityMaybe.error() ) );
            if ( not *entityMaybe )
                continue;
            if ( colon == string::npos )
            {
                scene.Destroy( **entityMaybe );
                continue;
            }
            const string componentName = textMaybe->substr( colon + 1 );
            const auto nameMaybe = Name::TryFrom( componentName );
            const auto typeMaybe = nameMaybe ? componentTypes.TryFind( *nameMaybe ) : nullopt;
            if ( not typeMaybe )
                return Fail(
                    item, where,
                    format( "{} is not a component (components: {})", componentName, componentTypes.Names() ) );
            if ( not typeMaybe->mGet( scene, **entityMaybe ) )
                missing( id, componentName );
            typeMaybe->mRemove( scene, **entityMaybe );
        }
    }

    if ( const auto setMaybe = find( "set" ) )
    {
        const auto nodesMaybe = setMaybe->TryAs<Json::Object>();
        if ( not nodesMaybe )
            return Fail( *setMaybe, where, format( "set - an object - expected, got {}", setMaybe->Describe() ) );
        for ( const Json::Member& nodeMember : *nodesMaybe )
        {
            auto entityMaybe = node( nodeMember.mValue, nodeMember.mKey );
            if ( not entityMaybe )
                return unexpected( std::move( entityMaybe.error() ) );
            if ( not *entityMaybe )
                continue;
            const auto fieldsMaybe = nodeMember.mValue.TryAs<Json::Object>();
            if ( not fieldsMaybe )
                return Fail( nodeMember.mValue, where,
                             format( "a node's fields - an object - expected, got {}", nodeMember.mValue.Describe() ) );
            for ( const Json::Member& field : *fieldsMaybe )
            {
                const string fieldWhere = format( "{}: node {}: {}", where, nodeMember.mKey, field.mKey );
                const size_t dot = field.mKey.find( '.' );
                const string componentName = field.mKey.substr( 0, dot );
                const auto nameMaybe = Name::TryFrom( componentName );
                const auto typeMaybe = nameMaybe ? componentTypes.TryFind( *nameMaybe ) : nullopt;
                if ( not typeMaybe )
                    return Fail(
                        field.mValue, where,
                        format( "{} is not a component (components: {})", componentName, componentTypes.Names() ) );
                if ( dot == string::npos )
                    return Fail( field.mValue, where, format( "{}: a field of the component expected", field.mKey ) );
                const auto pathMaybe = FieldPath::TryParse( string_view( field.mKey ).substr( dot + 1 ) );
                if ( not pathMaybe )
                    return Fail( field.mValue, fieldWhere, pathMaybe.error() );
                const auto typeAtMaybe = TryTypeAt( typeMaybe->mValueType, *pathMaybe );
                if ( not typeAtMaybe )
                    return Fail( field.mValue, fieldWhere, typeAtMaybe.error() );
                auto valueMaybe = TryValueFromJson( field.mValue, *typeAtMaybe, fieldWhere );
                if ( not valueMaybe )
                    return unexpected( std::move( valueMaybe.error() ) );
                MetaAny component = typeMaybe->mGetWritable( scene, **entityMaybe );
                if ( not component )
                {
                    missing( nodeMember.mKey, componentName );
                    continue;
                }
                if ( auto setFieldMaybe = reflection.TrySetField( component, *pathMaybe, MetaAny( *valueMaybe ) );
                     not setFieldMaybe )
                    return Fail( field.mValue, where, setFieldMaybe.error() );
            }
        }
    }

    if ( const auto addMaybe = find( "add_components" ) )
    {
        const auto nodesMaybe = addMaybe->TryAs<Json::Object>();
        if ( not nodesMaybe )
            return Fail( *addMaybe, where,
                         format( "add_components - an object - expected, got {}", addMaybe->Describe() ) );
        for ( const Json::Member& nodeMember : *nodesMaybe )
        {
            auto entityMaybe = node( nodeMember.mValue, nodeMember.mKey );
            if ( not entityMaybe )
                return unexpected( std::move( entityMaybe.error() ) );
            if ( not *entityMaybe )
                continue;
            const auto componentsMaybe = nodeMember.mValue.TryAs<Json::Object>();
            if ( not componentsMaybe )
                return Fail(
                    nodeMember.mValue, where,
                    format( "a node's components - an object - expected, got {}", nodeMember.mValue.Describe() ) );
            for ( const Json::Member& member : *componentsMaybe )
            {
                const auto nameMaybe = Name::TryFrom( member.mKey );
                const auto typeMaybe = nameMaybe ? componentTypes.TryFind( *nameMaybe ) : nullopt;
                if ( not typeMaybe )
                    return Fail(
                        member.mValue, where,
                        format( "{} is not a component (components: {})", member.mKey, componentTypes.Names() ) );
                auto valueMaybe = TryValueFromJson( member.mValue, typeMaybe->mValueType,
                                                    format( "{}: node {}: {}", where, nodeMember.mKey, member.mKey ) );
                if ( not valueMaybe )
                    return unexpected( std::move( valueMaybe.error() ) );
                auto madeMaybe = reflection.TryFromValue( *valueMaybe, typeMaybe->mType );
                if ( not madeMaybe )
                    return Fail( member.mValue, where, madeMaybe.error() );
                typeMaybe->mAdd( scene, **entityMaybe, *madeMaybe );
            }
        }
    }

    if ( const auto childrenMaybe = find( "add_children" ) )
    {
        const auto nodesMaybe = childrenMaybe->TryAs<Json::Object>();
        if ( not nodesMaybe )
            return Fail( *childrenMaybe, where,
                         format( "add_children - an object - expected, got {}", childrenMaybe->Describe() ) );
        for ( const Json::Member& nodeMember : *nodesMaybe )
        {
            auto entityMaybe = node( nodeMember.mValue, nodeMember.mKey );
            if ( not entityMaybe )
                return unexpected( std::move( entityMaybe.error() ) );
            if ( not *entityMaybe )
                continue;
            const auto arrayMaybe = nodeMember.mValue.TryAs<Json::Array>();
            if ( not arrayMaybe )
                return Fail(
                    nodeMember.mValue, where,
                    format( "a node's children - an array - expected, got {}", nodeMember.mValue.Describe() ) );
            for ( const Json& childJson : *arrayMaybe )
                if ( auto childMaybe = TryEntityFromJson( scene, childJson, **entityMaybe, componentTypes );
                     not childMaybe )
                    return unexpected( std::move( childMaybe.error() ) );
        }
    }
    return {};
}
}
