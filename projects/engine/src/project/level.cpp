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
    MakeRoot( "level" );
}

void Level::Clear()
{
    // Scene first - see the header.
    mScene = Scene();
    MakeRoot( "level" );
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
    j["Root"] = (u64)mScene.Root();
    // Every entity, with the ids of its components - an entity is kept even
    // when it has none.
    auto& entityComponentsJson = j["Entity components"];
    mScene.ForEachEntity( [&]( Entity entity )
    {
        entityComponentsJson[std::to_string( entity )] = mScene.ComponentsOf( entity );
    } );

    json& poolsJson = j["Component pools"];
    for ( const ComponentTypeId componentID : ComponentManager::Ids() )
    {
        json& poolJson = poolsJson[ComponentManager::GetName( componentID )];
        const auto& componentToJson = ComponentManager::GetToJson( componentID );
        mScene.ForEachComponent( componentID, [&]( Entity entity, const void* component )
        {
            componentToJson( poolJson[std::to_string( entity )], project, component );
        } );
    }
    return j;
}


void Level::LoadScene( const json& j, Project& project )
{
    // The entities first, under the ids the file gives them: components and
    // the tree refer to one another by those.
    for ( const auto& [entityStr, componentsJson] : j["Entity components"].items() )
        mScene.CreateEntity( Entity::FromId( std::strtoull( entityStr.c_str(), nullptr, 10 ) ) );

    for ( const auto& [componentNameString, poolJson] : j["Component pools"].items() )
    {
        // Throws on a name no component has.
        const ComponentTypeId componentID = ComponentManager::GetID( componentNameString );
        const auto& componentFromJson = ComponentManager::GetFromJson( componentID );
        for ( const auto& [entityIdStr, componentJson] : poolJson.items() )
        {
            const Entity entity = Entity::FromId( std::strtoull( entityIdStr.c_str(), nullptr, 10 ) );
            componentFromJson( componentJson, project, mScene.AddComponent( entity, componentID ) );
        }
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
    const json& scene = j["Scene"];
    if ( scene.contains( "Root" ) )
        mScene.SetRoot( Entity::FromId( scene["Root"].get<u64>() ) );
    if ( not mScene.HasEntity( Root() ) )
        throw std::runtime_error( "Level has no root entity" );
    DropDanglingLinks( mScene );
    // A file edited by hand may repeat a name among siblings; the second gets
    // a number, so every path leads somewhere.
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
