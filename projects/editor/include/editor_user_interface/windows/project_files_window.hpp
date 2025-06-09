#pragma once
#include "editor_user_interface/windows/window_base.hpp"

namespace bubble
{
class BubbleEditor;

enum class FilesystemNodeType
{
    Unknown,
    Folder,
    Model,
    Script,
    Texture
};

class ProjectWindow : public UserInterfaceWindowBase
{
    struct FilesystemNode
    {
        path mPath;
        FilesystemNodeType mType = FilesystemNodeType::Unknown;
        vector<FilesystemNode> mChildren;
    };

public:
    ProjectWindow( BubbleEditor& editorState );
    ~ProjectWindow();

    string_view Name();

    void FillFilesystemNode( FilesystemNode& root );
    void FillIcons( const FilesystemNode& node );
    void FillFilesystemTree();

    void OnUpdate( DeltaTime );
    void DrawFilesystemTree( const FilesystemNode& node );
    void DrawSelectedFolderItems();
    void OnDraw( DeltaTime );

private:
    FilesystemNode mFilesystemTreeRoot;
    const FilesystemNode* mSelectedNode = nullptr;

    str_hash_map<Texture2D> mModelIcons;
    Ref<Texture2D> mFolderIcon;
    Ref<Texture2D> mFileIcon;
};

}
