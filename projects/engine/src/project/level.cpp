#include "engine/pch/pch.hpp"
#include "engine/project/level.hpp"
#include "engine/project/project.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/project/prefab.hpp"
#include "engine/types/set.hpp"
#include "engine/scene/components/folder_component.hpp"
#include "engine/scene/components/hierarchy_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace bubble
{
Level::Level()
{
    MakeRoot( "Level" );
}

void Level::Clear()
{
    // Scene first - see the header.
    mScene = Scene();
    MakeRoot( "Level" );
    mName.clear();
    mFile.clear();
}

Entity Level::MakeRoot( const string& name )
{
    const Entity root = mScene.CreateEntity();
    mScene.AddComponent<TagComponent>( root, name );
    mScene.AddComponent<TransformComponent>( root );
    mScene.AddComponent<FolderComponent>( root );
    mScene.AddComponent<HierarchyComponent>( root );
    mScene.SetRoot( root );
    return root;
}

void Level::SetRootName( const string& name )
{
    if ( mScene.HasEntity( Root() ) and mScene.HasComponent<TagComponent>( Root() ) )
        mScene.GetComponent<TagComponent>( Root() ).mName = name;
}

json Level::SaveScene( const Project& project ) const
{
    json j;
    j["Entity counter"] = mScene.mEntityCounter;
    j["Root"] = (u64)mScene.Root();
    // Entity components
    auto& entityComponentsJson = j["Entity components"];
    for ( const auto& [entity, componentTypeIds] : mScene.mEntitiesComponentTypeIds )
        entityComponentsJson[std::to_string( entity )] = componentTypeIds;

    json& poolsJson = j["Component pools"];
    for ( const auto& componentID : mScene.mComponents )
    {
        const auto iter = mScene.mPools.find( componentID );
        if ( iter == mScene.mPools.end() )
            throw std::runtime_error( std::format( "No pool for component: {}", componentID ) );

        const auto& pool = iter->second;
        json& poolJson = poolsJson[ComponentManager::GetName( componentID )];

        const auto& componentToJson = ComponentManager::GetToJson( componentID );
        const auto& poolEntities = pool.Entities();
        for ( size_t i = 0; i < poolEntities.size(); i++ )
        {
            const auto entityStr = std::to_string( poolEntities[i] );
            componentToJson( poolJson[entityStr], project, pool.GetRaw( i ) );
        }
    }
    return j;
}


void Level::LoadScene( const json& j, Project& project )
{
    mScene.mEntityCounter = j["Entity counter"];
    // Entity components
    const json& entityComponentsJson = j["Entity components"];
    for ( const auto& [entityStr, componentsJson] : entityComponentsJson.items() )
    {
        set<ComponentTypeId> components;
        for ( ComponentTypeId component : componentsJson )
            components.insert( component );

        u64 entityId = std::atoi( entityStr.c_str() );
        Entity entity = *(Entity*)&entityId;
        mScene.mEntitiesComponentTypeIds[entity] = components;
    }

    for ( const auto& [componentNameString, poolJson] : j["Component pools"].items() )
    {
        int componentID = ComponentManager::GetID( componentNameString );
        auto componentsIter = mScene.mComponents.find( componentID );
        if ( componentsIter == mScene.mComponents.end() )
            throw std::runtime_error( std::format( "Scene from_json failed. No such component: {}", componentID ) );

        auto poolsIter = mScene.mPools.find( componentID );
        if ( poolsIter == mScene.mPools.end() )
            throw std::runtime_error( std::format( "scene from_json failed. No such pool {}", componentID ) );

        Pool& pool = poolsIter->second;
        const auto& componentFromJson = ComponentManager::GetFromJson( componentID );

        // json object keys are strings, so items() yields "1", "10", "100", "2"...
        // Feeding a sorted pool in that order makes every insert shift the tail
        // (O(n^2) load). Sort numerically first so each push appends instead.
        std::vector<std::pair<u64, const json*>> ordered;
        ordered.reserve( poolJson.size() );
        for ( const auto& [entityIdStr, componentJson] : poolJson.items() )
            ordered.emplace_back( std::strtoull( entityIdStr.c_str(), nullptr, 10 ), &componentJson );

        std::sort( ordered.begin(), ordered.end(),
                   []( const auto& a, const auto& b ) { return a.first < b.first; } );

        for ( const auto& [entityId, componentJson] : ordered )
            componentFromJson( *componentJson, project, pool.PushEmpty( mScene.GetEntityById( entityId ) ) );
    }
}


void Level::MigrateTree( const json& tree )
{
    // The links the file may have had (a Hierarchy pool from before the tree
    // moved into the scene) are rebuilt from the tree, which was the truth.
    mScene.ForEach<HierarchyComponent>( []( Entity, HierarchyComponent& h )
    {
        h.mParent = INVALID_ENTITY;
        h.mChildren.clear();
    } );

    std::function<void( const json&, Entity )> walk = [&]( const json& node, Entity parent )
    {
        const string type = node.value( "Type", string() );
        Entity entity = INVALID_ENTITY;
        if ( type == "Root" or type == "Level" )
            entity = MakeRoot( node["State"].get<string>() );
        else if ( type == "Folder" )
        {
            entity = CreateChildEntity( mScene, parent );
            mScene.AddComponent<TagComponent>( entity, node["State"].get<string>() );
            mScene.AddComponent<TransformComponent>( entity );
            mScene.AddComponent<FolderComponent>( entity );
        }
        else
        {
            entity = mScene.GetEntityById( node["State"].get<u64>() );
            if ( not mScene.HasEntity( entity ) )
                return;
            AttachChild( mScene, entity, parent );
        }
        if ( const auto children = node.find( "Children" ); children != node.end() and children->is_array() )
            for ( const auto& child : *children )
                walk( child, entity );
    };
    walk( tree["Tree"], INVALID_ENTITY );
}

void Level::AdoptStrays()
{
    const Entity root = Root();
    if ( not mScene.HasEntity( root ) )
        return;
    vector<Entity> strays;
    vector<Entity> empty;
    mScene.ForEachEntity( [&]( Entity entity )
    {
        if ( entity == root )
            return;
        const Entity parent = ParentOf( mScene, entity );
        if ( parent != INVALID_ENTITY and mScene.HasEntity( parent ) )
            return;
        // Old files kept entities with nothing on them that no tree node
        // named; there is nothing to keep.
        if ( mScene.EntityComponentTypeIds( entity ).empty() )
            empty.push_back( entity );
        else
            strays.push_back( entity );
    } );
    mScene.RemoveEntities( empty );
    for ( const Entity entity : strays )
    {
        DetachFromParent( mScene, entity );
        AttachChild( mScene, entity, root );
    }
}


json Level::ToJson( const Project& project ) const
{
    json j;
    j["Scene"] = SaveScene( project );
    return j;
}

void Level::FromJson( const json& j, Project& project )
{
    // Loaded into an empty scene: the default root goes, the file brings one.
    mScene = Scene();
    LoadScene( j["Scene"], project );
    if ( j.contains( "ProjectTree" ) )
        MigrateTree( j["ProjectTree"] );
    else
        mScene.SetRoot( mScene.GetEntityById( j["Scene"].value( "Root", u64( 0 ) ) ) );
    if ( not mScene.HasEntity( Root() ) )
        MakeRoot( "Level" );
    AdoptStrays();
    // Files from before names were unique among siblings may repeat one; the
    // second gets a number, so every path leads somewhere.
    MakeNamesUnique( mScene, Root() );
    UpdateWorldTransforms( mScene );
}


void Level::Load( const path& absFile, Project& project )
{
    // A prefab is a level file under another extension.
    if ( not is_regular_file( absFile ) or ( absFile.extension() != LEVEL_FILE_EXT and absFile.extension() != PREFAB_FILE_EXT ) )
        throw std::runtime_error( "Invalid level path: " + absFile.string() );

    Clear();
    std::ifstream stream( absFile );
    FromJson( json::parse( stream ), project );
    mFile = absFile;
    mName = absFile.stem().string();
    // The root is labeled after the file, so the tree says which level it is
    // showing.
    SetRootName( mName );
    LogInfo( "{} opened: {}", absFile.extension() == PREFAB_FILE_EXT ? "Prefab" : "Level", absFile.string() );
}

void Level::Save( const path& absFile, const Project& project ) const
{
    std::ofstream levelFile( absFile );
    levelFile << ToJson( project ).dump( 1 );
    LogInfo( "{} saved: {}", absFile.extension() == PREFAB_FILE_EXT ? "Prefab" : "Level", absFile.string() );
}

}
