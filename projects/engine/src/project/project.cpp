#include "engine/pch/pch.hpp"
#include "engine/project/project.hpp"
#include "engine/serialization/loader_serialization.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/types/set.hpp"
#include "nlohmann/json.hpp"
#include <fstream>

namespace bubble
{
constexpr string_view ROOT_FILE_EXT = ".bubble"sv;

void Project::LoadDefaultResources()
{
    mLoader.LoadShader( PHONG_SHADER );
    mLoader.LoadShader( WHITE_SHADER );
    mLoader.LoadShader( ONLY_DIFFUSE_SHADER );
}

Project::Project()
    : mProjectTreeRoot( CreateRef<ProjectTreeNode>() )
{}

Project::~Project()
{
}

void Project::Create( const path& rootDir, const string& projectName )
{
    auto projectDir = rootDir / projectName;
    filesystem::create_directory( projectDir );

    mRootFile = ( projectDir / projectName ).replace_extension( ROOT_FILE_EXT );
    if ( filesystem::exists( mRootFile ) )
        throw std::runtime_error( "Project with such name already exists: " + mRootFile.string() );

    LoadDefaultResources();
    Save();
}

json Project::SaveScene()
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
        auto iter = mScene.mPools.find( componentID );
        if ( iter == mScene.mPools.end() )
            throw std::runtime_error( std::format( "No pool for component: {}", componentID ) );

        const auto& pool = iter->second;
        json& poolJson = poolsJson[ComponentManager::GetName( componentID )];

        const auto& componentToJson = ComponentManager::GetToJson( componentID );
        for ( size_t i = 0; i < pool.mEntities.size(); i++ )
        {
            auto entityStr = std::to_string( pool.mEntities[i] );
            componentToJson( poolJson[entityStr], *this, pool.GetRaw( i ) );
        }
    }
    return j;
}



void Project::LoadScene( const json& j )
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

        for ( const auto& [entityIdStr, componentJson] : poolJson.items() )
        {
            u64 entityId = std::atoi( entityIdStr.c_str() );
            componentFromJson( componentJson, *this, pool.PushEmpty( mScene.GetEntityById( entityId ) ) );
        }
    }
}


json Project::SaveProjectTreeNode( const Ref<ProjectTreeNode>& node )
{
    json j;
    j["ID"] = node->mID;
    j["Type"] = magic_enum::enum_name( node->mType );

    if ( node->mType == ProjectTreeNodeType::Level or
         node->mType == ProjectTreeNodeType::Folder )
        j["State"] = std::get<string>( node->mState );
    else 
        j["State"] = (u64)std::get<Entity>( node->mState );

    json& children = j["Children"];
    for ( const auto& child : node->mChildren )
        children.push_back( SaveProjectTreeNode( child ) );
    
    return j;
}

json Project::SaveProjectTree()
{
    json j;
    j["Counter"] = ProjectTreeNode::mIDCounter;
    j["Tree"] = SaveProjectTreeNode( mProjectTreeRoot );
    return j;
}


Ref<ProjectTreeNode> Project::LoadProjectTreeNode( const json& j, const Ref<ProjectTreeNode>& parent )
{
    auto node = CreateRef<ProjectTreeNode>();

    node->mID = j["ID"];
    auto optType = magic_enum::enum_cast<ProjectTreeNodeType>( string( j["Type"] ) );
    if ( not optType )
        throw std::runtime_error( std::format( "Failed to read project tree node type: {}", string(j["Type"]) ) );
    node->mType = *optType;

    if ( node->mType == ProjectTreeNodeType::Level or
         node->mType == ProjectTreeNodeType::Folder )
        node->mState = string( j["State"] );
    else
        node->mState = mScene.GetEntityById( j["State"] );

    const json& children = j["Children"];
    for ( const auto& child : children )
        node->mChildren.emplace_back( LoadProjectTreeNode( child, node ) );

    node->mParent = parent;
    return node;
}

void Project::LoadProjectTree( const json& j )
{
    ProjectTreeNode::mIDCounter = j["Counter"];
    mProjectTreeRoot = LoadProjectTreeNode( j["Tree"], nullptr );
}

void Project::Save()
{
    json projectJson;
    projectJson["Loader"] = mLoader;
    projectJson["Scene"] = SaveScene();
    projectJson["ProjectTree"] = SaveProjectTree();

    std::ofstream projectFile( mRootFile );
    projectFile << projectJson.dump( 1 );
    LogInfo( "Project saved: {}", mRootFile.string() );
}


void Project::Open( const path& rootFile )
{
    if ( !is_regular_file( rootFile ) || rootFile.extension() != ROOT_FILE_EXT )
        throw std::runtime_error( "Invalid project path: " + rootFile.string() );

    mRootFile = rootFile;
    mLoader.mProjectRootPath = rootFile.parent_path();

    std::ifstream stream( mRootFile );
    json projectJson = json::parse( stream );
    from_json( projectJson["Loader"], mLoader );
    LoadScene( projectJson["Scene"] );
    LoadProjectTree( projectJson["ProjectTree"] );
    LogInfo( "Project opened: {}", mRootFile.string() );
}


bool Project::IsValid()
{
    return not mRootFile.empty();
}


}
