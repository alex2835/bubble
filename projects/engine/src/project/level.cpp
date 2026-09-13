#include "engine/pch/pch.hpp"
#include "engine/project/level.hpp"
#include "engine/project/project.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/types/set.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace bubble
{
Level::Level()
    : mTreeRoot( CreateRef<ProjectTreeNode>( mNodeIDCounter ) )
{
}

void Level::Clear()
{
    // Scene first - see the header.
    mScene = Scene();
    mNodeIDCounter = 0;
    mTreeRoot = CreateRef<ProjectTreeNode>( mNodeIDCounter );
    mName.clear();
    mFile.clear();
}

json Level::SaveScene( const Project& project ) const
{
    json j;
    j["Entity counter"] = mScene.mEntityCounter;
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
        // Feeding a sorted pool in that order makes every insert memmove the tail
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


json Level::SaveTreeNode( const Ref<ProjectTreeNode>& node ) const
{
    json j;
    j["ID"] = node->mID;
    j["Type"] = magic_enum::enum_name( node->mType );

    if ( node->mType == ProjectTreeNodeType::Root or
         node->mType == ProjectTreeNodeType::Folder )
        j["State"] = std::get<string>( node->mState );
    else
        j["State"] = (u64)std::get<Entity>( node->mState );

    json& children = j["Children"];
    for ( const auto& child : node->mChildren )
        children.push_back( SaveTreeNode( child ) );

    return j;
}

json Level::SaveTree() const
{
    json j;
    j["Counter"] = mNodeIDCounter;
    j["Tree"] = SaveTreeNode( mTreeRoot );
    return j;
}


Ref<ProjectTreeNode> Level::LoadTreeNode( const json& j, const Ref<ProjectTreeNode>& parent )
{
    auto node = CreateRef<ProjectTreeNode>( mNodeIDCounter );

    node->mID = j["ID"];
    const string typeName = j["Type"];
    auto optType = magic_enum::enum_cast<ProjectTreeNodeType>( typeName );
    // The root used to be called "Level" before a level became a file of its own.
    if ( not optType and typeName == "Level" )
        optType = ProjectTreeNodeType::Root;
    if ( not optType )
        throw std::runtime_error( std::format( "Failed to read project tree node type: {}", typeName ) );
    node->mType = *optType;

    if ( node->mType == ProjectTreeNodeType::Root or
         node->mType == ProjectTreeNodeType::Folder )
        node->mState = string( j["State"] );
    else
        node->mState = mScene.GetEntityById( j["State"] );

    const json& children = j["Children"];
    for ( const auto& child : children )
        node->mChildren.emplace_back( LoadTreeNode( child, node ) );

    node->mParent = parent;
    return node;
}

void Level::LoadTree( const json& j )
{
    mNodeIDCounter = j["Counter"];
    mTreeRoot = LoadTreeNode( j["Tree"], nullptr );
}


json Level::ToJson( const Project& project ) const
{
    json j;
    j["Scene"] = SaveScene( project );
    j["ProjectTree"] = SaveTree();
    return j;
}

void Level::FromJson( const json& j, Project& project )
{
    LoadScene( j["Scene"], project );
    LoadTree( j["ProjectTree"] );
}


void Level::Load( const path& absFile, Project& project )
{
    if ( not is_regular_file( absFile ) or absFile.extension() != LEVEL_FILE_EXT )
        throw std::runtime_error( "Invalid level path: " + absFile.string() );

    Clear();
    std::ifstream stream( absFile );
    FromJson( json::parse( stream ), project );
    mFile = absFile;
    mName = absFile.stem().string();
    // The root node is labeled after the file, so the tree says which level
    // it is showing.
    mTreeRoot->mState = mName;
    LogInfo( "Level opened: {}", absFile.string() );
}

void Level::Save( const path& absFile, const Project& project ) const
{
    std::ofstream levelFile( absFile );
    levelFile << ToJson( project ).dump( 1 );
    LogInfo( "Level saved: {}", absFile.string() );
}

}
