#pragma once
#include "engine/renderer/camera_third_person.hpp"
#include "editor_interfaces/ieditor_interface.hpp"

namespace bubble
{
namespace
{
enum class FilesystemNodeType
{
    Unknown,
    Folder,
    Model,
    Texture
};
} // namespace


class ProjectInterface : public IEditorInterface
{
    struct FilesystemNode
    {
        path mPath;
        FilesystemNodeType mType = FilesystemNodeType::Unknown;
        vector<FilesystemNode> mChildren;
    };

public:
    ProjectInterface( EditorState& editorState );
    ~ProjectInterface();

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

    // Render model
    ThirdPersonCamera mCamera;
    Scene mScene;
    //Ref<Shader> mShader;

    str_hash_map<Texture2D> mModelIcons;
    Ref<Texture2D> mFolderIcon;
    Ref<Texture2D> mFileIcon;
};

}
