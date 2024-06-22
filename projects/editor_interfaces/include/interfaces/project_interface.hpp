#pragma once
#include "ieditor_interface.hpp"
#include "editor_application.hpp"

namespace bubble
{
enum class FilesystemNodeType
{
    Unknown,
    Folder,
    Model,
    Texture
};

bool isModelDir( const filesystem::directory_entry& dir )
{
    for ( const auto& item : filesystem::directory_iterator( dir ) )
    {
        const auto& extension = item.path().extension();
        if ( extension == ".obj" or 
             extension == ".blend" )
            return true;
    }
    return false;
}

bool isTextureFile( const filesystem::directory_entry& item )
{
    const auto& extension = item.path().extension();
    if ( extension == ".png" or
         extension == ".jpg" )
        return true;
    return false;
}

FilesystemNodeType DetectItemType( const filesystem::directory_entry& item )
{
    if ( item.is_directory() )
    {
        if ( isModelDir( item ) )
            return FilesystemNodeType::Model;
        return FilesystemNodeType::Folder;
    }
    else
    {
        if ( isTextureFile( item ) )
            return FilesystemNodeType::Texture;
    }
    return FilesystemNodeType::Unknown;
}





class ProjectInterface : public IEditorInterface
{
public:
    struct FilesystemNode
    {
        path mPath;
        FilesystemNodeType mType = FilesystemNodeType::Unknown;
        vector<FilesystemNode> mChildren;
    };

    void FillFilesystemNode( FilesystemNode& root )
    {
        for ( const auto& item : filesystem::directory_iterator( root.mPath ) )
        {
            FilesystemNode node;
            node.mPath = item.path();
            node.mType = DetectItemType( item );
            if ( node.mType == FilesystemNodeType::Folder )
                FillFilesystemNode( node );
            root.mChildren.push_back( std::move( node ) );
        }
    }

    void FillFilesystemTree()
    {
        FilesystemNode root;
        root.mPath = mProject.mRootFile.parent_path();
        root.mType = FilesystemNodeType::Folder;
        FillFilesystemNode( root );
        std::swap( mFilesystemTreeRoot, root );
    }

    

    using IEditorInterface::IEditorInterface;
    ~ProjectInterface() override = default;

    string_view Name() override
    {
        return "Project"sv;
    }

    void OnInit() override
    {
        if ( not mProject.mName.empty() )
            FillFilesystemTree();
    }

    void OnUpdate( DeltaTime ) override
    {
    }

    void DrawFilesystemTree( const FilesystemNode& node )
    {
        if ( node.mType != FilesystemNodeType::Folder )
            return;

        auto nodeImGuiFlag = ImGuiTreeNodeFlags_None;
        if ( mSelectedNode == &node )
            nodeImGuiFlag = ImGuiTreeNodeFlags_Selected;

        if ( ImGui::TreeNodeEx( node.mPath.filename().string().c_str(), nodeImGuiFlag ) )
        {
            if ( ImGui::IsItemToggledOpen() || ImGui::IsItemClicked() )
                mSelectedNode = &node;

            for ( const auto& childNode : node.mChildren )
                DrawFilesystemTree( childNode );
            ImGui::TreePop();
        }
    }

    void DrawSelectedFolderItems()
    {
        if ( not mSelectedNode )
            return;

        for ( const auto& child : mSelectedNode->mChildren )
            ImGui::Text( child.mPath.string().c_str() );
    }

    void OnDraw( DeltaTime ) override
    {
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 } );
        ImGui::Begin( Name().data(), &mOpen, ImGuiWindowFlags_NoCollapse );
        {
            ImGui::BeginChild( "Project tree", ImVec2( 250, 0 ), true );
            {
                if ( ImGui::Button( "Update", ImVec2{ 50, 20 } ) )
                    FillFilesystemTree();

                DrawFilesystemTree( mFilesystemTreeRoot );
            }
            ImGui::EndChild();


            ImGui::SameLine();
            ImGui::BeginChild( "Selected folder items" );
            {
                DrawSelectedFolderItems();
            }
            ImGui::EndChild();
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

private:
    FilesystemNode mFilesystemTreeRoot;
    const FilesystemNode* mSelectedNode = nullptr;
};

}
