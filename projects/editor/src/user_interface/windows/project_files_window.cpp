#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/project_files_window.hpp"
#include "editor_application/editor_application.hpp"
#include <imgui.h>

namespace bubble
{
namespace
{
std::optional<path> tryFindModelInFolder( const path& dir )
{
    for ( const auto& item : filesystem::directory_iterator( dir ) )
    {
        const auto& extension = item.path().extension();
        if ( extension == ".obj" )
            return item;
    }
    return std::nullopt;
}

bool isScriptFile( const filesystem::directory_entry& item )
{
    const auto& extension = item.path().extension();
    return extension == ".lua";
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
        if ( tryFindModelInFolder( item ) )
            return FilesystemNodeType::Model;
        return FilesystemNodeType::Folder;
    }
    else
    {
        if ( isTextureFile( item ) )
            return FilesystemNodeType::Texture;
        else if ( isScriptFile( item ) )
            return FilesystemNodeType::Script;
    }
    return FilesystemNodeType::Unknown;
}
} // namespace



ProjectFilesWindow::ProjectFilesWindow( BubbleEditor& editorState )
    : UserInterfaceWindowBase( editorState )
{
    mFolderIcon = LoadTexture2D( "./resources/images/project_tree/folder.png" );
    mFileIcon = LoadTexture2D( "./resources/images/project_tree/file.png" );
    if ( not mProject.mName.empty() )
        FillFilesystemTree();

}


ProjectFilesWindow::~ProjectFilesWindow()
{

}

string_view ProjectFilesWindow::Name()
{
    return "Project"sv;
}

void ProjectFilesWindow::FillFilesystemNode( FilesystemNode& root )
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

void ProjectFilesWindow::FillIcons( const FilesystemNode& node )
{
    if ( node.mType == FilesystemNodeType::Model )
    {
        auto modelPath = tryFindModelInFolder( node.mPath );
        if ( modelPath )
            mProject.mLoader.LoadModel( *modelPath );
    }
    else if ( node.mType == FilesystemNodeType::Script )
        mProject.mLoader.LoadScript( node.mPath );

    for ( const auto& child : node.mChildren )
        FillIcons( child );
}

void ProjectFilesWindow::FillFilesystemTree()
{
    FilesystemNode root;
    root.mPath = mProject.mRootFile.parent_path();
    root.mType = FilesystemNodeType::Folder;
    FillFilesystemNode( root );
    FillIcons( root );
    std::swap( mFilesystemTreeRoot, root );
}

void ProjectFilesWindow::OnUpdate( DeltaTime )
{

}

void ProjectFilesWindow::DrawFilesystemTree( const FilesystemNode& node )
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

void ProjectFilesWindow::DrawSelectedFolderItems()
{
    if ( not mSelectedNode )
        return;

    int elementsInRow = 8;
    ImVec2 windowSize = ImGui::GetWindowSize();
    float elemLength = windowSize.x / ( elementsInRow * 1.1f );
    ImVec2 elemSize = ImVec2( elemLength, elemLength );

    for ( size_t i = 0; i < mSelectedNode->mChildren.size(); i++ )
    {
        const auto& child = mSelectedNode->mChildren[i];
        if ( i % elementsInRow != 0 )
            ImGui::SameLine();

        ImGui::BeginChild( child.mPath.string().c_str(), elemSize + ImVec2( 0, 50 ) );
        {
            // Handle actions
            if ( child.mType == FilesystemNodeType::Folder and
                 ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) and
                 ImGui::IsWindowHovered() )
                mSelectedNode = &child;

            // Draw
            Ref<Texture2D> texture = mFileIcon;
            if ( child.mType == FilesystemNodeType::Folder )
                texture = mFolderIcon;
            if ( child.mType == FilesystemNodeType::Texture )
                texture = mProject.mLoader.LoadTexture2D( child.mPath );

            ImGui::Image( (ImTextureID)(i64)texture->RendererID(), elemSize );
            ImGui::Text( "%s", child.mPath.stem().string().c_str() );
        }
        ImGui::EndChild();
    }
}

void ProjectFilesWindow::OnDraw( DeltaTime )
{
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 } );
    ImGui::Begin( Name().data(), &mOpen, ImGuiWindowFlags_NoCollapse );
    if ( mEditorMode == EditorMode::Editing )
    {
        ImGui::BeginChild( "Project tree", ImVec2( 250, 0 ), true );
        {
            if ( mUIGlobals.mNeedUpdateProjectWindow )
            {
                mUIGlobals.mNeedUpdateProjectWindow = false;
                FillFilesystemTree();
            }

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

}