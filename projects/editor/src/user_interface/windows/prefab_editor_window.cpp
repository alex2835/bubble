#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/prefab_editor_window.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/project/prefab.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/scene/components/model_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include <nlohmann/json.hpp>
#include <imgui.h>

namespace bubble
{
namespace
{
constexpr uvec2 cViewportSize{ 800, 640 };

// Every .prefab under the project, relative to its root, sorted.
vector<path> ProjectPrefabs( const Project& project )
{
    vector<path> prefabs;
    std::error_code ec;
    for ( auto it = filesystem::recursive_directory_iterator( project.RootDir(), ec );
          it != filesystem::recursive_directory_iterator(); it.increment( ec ) )
    {
        if ( ec )
            break;
        if ( it->is_regular_file() and it->path().extension() == PREFAB_FILE_EXT )
            prefabs.push_back( filesystem::relative( it->path(), project.RootDir() ) );
    }
    std::ranges::sort( prefabs );
    return prefabs;
}
}

PrefabEditorWindow::PrefabEditorWindow( BubbleEditor& editor )
    : UserInterfaceWindowBase( editor ),
      mCamera( editor.mWindow.GetWindowInput(), vec3( 0, 4, 14 ) ),
      mColor( Texture2DSpecification::CreateRGBA8( cViewportSize ), Texture2DSpecification::CreateDepth( cViewportSize ) ),
      mIds( Texture2DSpecification::CreateObjectId( cViewportSize ), Texture2DSpecification::CreateDepth( cViewportSize ) )
{
    // A prefab is looked at close up.
    mCamera.mDefaultSpeed = 8.0f;
    mCamera.mBoostSpeed = 30.0f;
    mCamera.mPitch = glm::radians( -15.0f );
    mCamera.EulerAnglesToVectors();

    mTree = CreateScope<ProjectTreeWindow>( editor, Document() );
    mViewport = CreateScope<ProjectViewportWindow>( editor, Document() );
}

PrefabEditorWindow::~PrefabEditorWindow() = default;

string_view PrefabEditorWindow::Name()
{
    return "Prefab Editor"sv;
}

EditorDocument PrefabEditorWindow::Document()
{
    return EditorDocument{ mDoc, mDocHistory, mDocSelection, mDocClipboard, mCamera, mColor, mIds, mDocPicker,
                           mDocPendingRect, mDocHovered, mDocViewManipulating, /*editableWhileRunning*/ true };
}

std::optional<OperatorContext> PrefabEditorWindow::FocusedDocument()
{
    if ( not mFocused or not HasDocument() or not mUIGlobals.mShow.mPrefabEditor )
        return std::nullopt;
    return OperatorContext{ mProject, mDoc, mDocHistory, mDocSelection, mDocClipboard };
}

void PrefabEditorWindow::Open( const path& relFile )
{
    // The commands in the history hold nodes counted by the document being
    // replaced; they go first.
    mDocSelection.Clear();
    mDocHistory.Clear();
    mDocClipboard.Clear();
    mDocPicker.Cancel();
    mFile.clear();
    try
    {
        mDoc.Load( mProject.RootDir() / relFile, mProject );
    }
    catch ( const std::exception& e )
    {
        LogError( "Prefab {}: {}", relFile.generic_string(), e.what() );
        mDoc.Clear();
        return;
    }
    mFile = relFile;
    mSavedVersion = mDocHistory.Version();
    mUIGlobals.mShow.mPrefabEditor = true;
    FrameContent();
}

// The camera onto everything the prefab draws, from a little above and in
// front, far enough back to see all of it.
void PrefabEditorWindow::FrameContent()
{
    vec3 lo( std::numeric_limits<f32>::max() ), hi( -std::numeric_limits<f32>::max() );
    mDoc.mScene.ForEach<ModelComponent, TransformComponent>( [&]( Entity, const ModelComponent& model, const TransformComponent& t )
    {
        if ( not model.mModel )
            return;
        const vec3 a = model.mModel->mBBox.getMin(), b = model.mModel->mBBox.getMax();
        for ( int i = 0; i < 8; i++ )
        {
            const vec3 corner( i & 1 ? b.x : a.x, i & 2 ? b.y : a.y, i & 4 ? b.z : a.z );
            const vec3 p = vec3( t.WorldMatrix() * vec4( corner, 1.0f ) );
            lo = glm::min( lo, p );
            hi = glm::max( hi, p );
        }
    } );
    const bool empty = lo.x > hi.x;
    const vec3 center = empty ? vec3( 0 ) : ( lo + hi ) * 0.5f;
    const f32 radius = empty ? 3.0f : std::max( glm::length( hi - lo ) * 0.5f, 0.5f );

    const vec3 back = normalize( vec3( 0.0f, 0.45f, 1.0f ) );
    mCamera.mPosition = center + back * radius * 2.2f;
    const vec3 look = -back;
    mCamera.mYaw = std::atan2( look.z, look.x );
    mCamera.mPitch = std::asin( look.y );
    mCamera.EulerAnglesToVectors();
    // Moving at a pace that suits what is on screen.
    mCamera.mDefaultSpeed = std::max( radius * 1.5f, 2.0f );
    mCamera.mBoostSpeed = mCamera.mDefaultSpeed * 4.0f;
}

void PrefabEditorWindow::New( const path& relFile )
{
    path file = relFile;
    if ( file.extension() != PREFAB_FILE_EXT )
        file += PREFAB_FILE_EXT;
    const path abs = mProject.RootDir() / file;
    if ( filesystem::exists( abs ) )
    {
        LogError( "Prefab {} exists already", file.generic_string() );
        return;
    }
    Level empty;
    empty.SetRootName( abs.stem().string() );
    filesystem::create_directories( abs.parent_path() );
    empty.Save( abs, mProject );
    Open( file );
}

bool PrefabEditorWindow::Save()
{
    if ( not HasDocument() )
        return false;
    mDoc.Save( mProject.RootDir() / mFile, mProject );
    mSavedVersion = mDocHistory.Version();
    mUIGlobals.mNeedUpdateProjectFilesWindow = true;

    // The level's instances follow the file. While the game runs the level
    // is not edited; they catch up on the next save, or from the menu.
    if ( mEditorMode == EditorMode::Editing and mProject.IsValid() )
    {
        OperatorContext level = Operators();
        try
        {
            InvokeOperator( "prefab.update_instances", level, { { "file", mFile.generic_string() } } );
        }
        catch ( const std::exception& e )
        {
            LogError( "Updating the instances of {}: {}", mFile.generic_string(), e.what() );
        }
    }
    return true;
}

void PrefabEditorWindow::Close()
{
    mDocSelection.Clear();
    mDocHistory.Clear();
    mDocClipboard.Clear();
    mDocPicker.Cancel();
    mDoc.Clear();
    mFile.clear();
    mSavedVersion = mDocHistory.Version();
}

void PrefabEditorWindow::OnUpdate( DeltaTime dt )
{
    if ( not HasDocument() or not mUIGlobals.mShow.mPrefabEditor )
        return;
    mViewport->OnUpdate( dt );
    mDocSelection.Prune( mDoc.mScene );
}

void PrefabEditorWindow::Render( Engine& engine, DeltaTime dt )
{
    if ( not HasDocument() or not mUIGlobals.mShow.mPrefabEditor )
        return;
    Scene& scene = mDoc.mScene;
    UpdateWorldTransforms( scene );

    if ( not mDocViewManipulating )
        mCamera.OnUpdate( dt );

    // The engine draws through its one camera; it is lent to the prefab for
    // these passes and handed back.
    const Camera levelCamera = engine.mCamera;
    engine.mCamera = (Camera)mCamera;
    engine.PropagateCameraTransforms( scene );
    engine.PropagateLightTransforms( scene );
    engine.UpdateAnimations( scene, dt.Seconds() );
    engine.DrawScene( mColor, scene, /*previewLight*/ true );
    engine.DrawEditorBillboards( mColor, scene );
    if ( mDocPicker.WantsIdPass() )
    {
        engine.DrawEntityIds( mIds, scene );
        mDocPicker.CaptureFrom( mIds );
    }
    engine.DrawCameraFrustums( mColor, scene );
    if ( mUIGlobals.mDrawBoundingBoxes )
        engine.DrawBoundingBoxes( mColor, scene );
    if ( mUIGlobals.mDrawPhysicsShapes )
        engine.DrawPhysicsShapes( mColor, scene );
    if ( mUIGlobals.mDrawSkeletons )
        engine.DrawSkeletons( mColor, scene );
    engine.mCamera = levelCamera;
}

void PrefabEditorWindow::DrawToolbar()
{
    const f32 scale = mWindow.GetUIScale();
    ImGui::SetNextItemWidth( 260.0f * scale );
    const string current = HasDocument() ? mFile.generic_string() : string( "open a prefab..." );
    if ( ImGui::BeginCombo( "##prefab", current.c_str() ) )
    {
        for ( const path& prefab : ProjectPrefabs( mProject ) )
            if ( ImGui::Selectable( prefab.generic_string().c_str(), prefab == mFile ) and prefab != mFile )
                Open( prefab );
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled( not HasDocument() );
    if ( ImGui::Button( "Save" ) )
        Save();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SetNextItemWidth( 160.0f * scale );
    ImGui::InputTextWithHint( "##new", "prefabs/name", mNewName );
    ImGui::SameLine();
    ImGui::BeginDisabled( mNewName.empty() );
    if ( ImGui::Button( "New" ) )
    {
        New( mNewName );
        mNewName.clear();
    }
    ImGui::EndDisabled();

    if ( HasDocument() )
    {
        ImGui::SameLine();
        ImGui::TextDisabled( "Ctrl+S saves and updates the level's instances. Drag a .prefab in to nest one." );
    }
}

void PrefabEditorWindow::OnDraw( DeltaTime )
{
    if ( not mUIGlobals.mShow.mPrefabEditor )
    {
        mFocused = false;
        mDocHovered = false;
        return;
    }
    ImGui::SetNextWindowSize( ImVec2( 1200, 700 ), ImGuiCond_FirstUseEver );
    const string title = std::format( "{}{}###PrefabEditor", HasDocument() ? mFile.filename().string() : string( Name() ),
                                      IsDirty() ? " *" : "" );
    if ( not ImGui::Begin( title.c_str(), &mUIGlobals.mShow.mPrefabEditor ) )
    {
        mFocused = false;
        mDocHovered = false;
        ImGui::End();
        return;
    }
    mFocused = ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows );

    DrawToolbar();
    ImGui::Separator();
    if ( not HasDocument() )
    {
        ImGui::TextDisabled( "Open a prefab above, make a new one, or save a node of the level as one\n"
                             "(right click it in Entities)." );
        mDocHovered = false;
        ImGui::End();
        return;
    }

    const f32 side = 330.0f * mWindow.GetUIScale();
    ImGui::BeginChild( "prefab tree", ImVec2( side, 0 ), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX );
    mTree->DrawContent();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 } );
    ImGui::BeginChild( "prefab viewport", ImVec2( 0, 0 ), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
    mViewport->DrawContent();
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::End();
}

}
