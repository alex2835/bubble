#include "bubble/editor/play_session.hpp"
#include "bubble/editor/document_world.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/scene/scene_snapshot.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/ui/ui.hpp"

namespace bubble
{
namespace
{
// The source's scene copied whole - entity numbers, the tree, every type's
// components in their order - into a document of its own, active.
Document& OpenCopy( EditorProject& project, const Document& source )
{
    const ComponentTypes& types = project.GetApp().GetComponentTypes();
    Scene scene;
    SceneSnapshot( source.GetScene(), types ).Restore( scene, types );
    Document& played = project.GetWorkspace().Open( std::move( scene ) );
    played.SetPlayed();
    return played;
}

string TitleOf( const Document& source )
{
    const auto pathMaybe = source.TryPath();
    if ( not pathMaybe )
        return "untitled";
    return string( pathMaybe->View().substr( pathMaybe->View().rfind( '/' ) + 1 ) );
}

// A position in the window made the game's: from its picture's corner.
vec2 InPicture( vec2 position, vec2 corner )
{
    return position - corner;
}
}

PlaySession::PlaySession( EditorProject& project, Document& source )
    : mProject( project ),
      mTitle( TitleOf( source ) ),
      mPlayed( OpenCopy( project, source ) ),
      mInput( project.GetSettings().mActions )
{
    // The level it is, for what a save slot names.
    AssetId levelId;
    if ( const auto pathMaybe = source.TryPath() )
        if ( const auto idMaybe = project.GetWorkspace().TryIdOf( *pathMaybe ) )
            levelId = *idMaybe;
    mPlayed.SetView(
        CreateScope<DocumentWorld>( project.GetApp(), mPlayed, WorldOptions{ .mLevel = levelId, .mEditing = false } ) );
    GetWorld().SetInput( WorldInput{ .mLatchStep = [this] { return mInput.LatchStep(); },
                                     .mLatchFrame = [this] { return mInput.LatchFrame(); } } );
}

PlaySession::~PlaySession()
{
    // The workspace makes the document active before it active again: the
    // one played from, opened just before the copy.
    Workspace& workspace = mProject.GetWorkspace();
    if ( mPlayed.GetHistory().InGroup() )
        mPlayed.GetHistory().EndGroup();
    workspace.Close( mPlayed );
}

DocumentWorld& PlaySession::View()
{
    return static_cast<DocumentWorld&>( *mPlayed.TryView() );
}

World& PlaySession::GetWorld()
{
    return View().Get();
}

void PlaySession::Frame( f64 elapsed )
{
    World& world = GetWorld();
    world.Get<UiWorld>().SetScreen( ivec2( static_cast<i32>( mSize.x ), static_cast<i32>( mSize.y ) ) );
    world.Frame( elapsed );
}

void PlaySession::SetScreen( vec2 corner, vec2 size )
{
    mCorner = corner;
    mSize = glm::max( size, vec2( 1.0f ) );
}

void PlaySession::Handle( const InputEvent& event )
{
    // The mouse's positions as the game's own window would have them: from
    // the picture's corner. The editor is the desktop's: no touches.
    InputEvent shown = event;
    if ( auto* move = get_if<MouseMove>( &shown ) )
        move->mPosition = InPicture( move->mPosition, mCorner );
    else if ( auto* down = get_if<MouseDown>( &shown ) )
        down->mPosition = InPicture( down->mPosition, mCorner );
    else if ( auto* up = get_if<MouseUp>( &shown ) )
        up->mPosition = InPicture( up->mPosition, mCorner );

    if ( const auto* key = get_if<KeyDown>( &shown ); key and not ranges::contains( mKeysDown, key->mKey ) )
        mKeysDown.push_back( key->mKey );
    else if ( const auto* keyUp = get_if<KeyUp>( &shown ) )
        erase( mKeysDown, keyUp->mKey );
    else if ( const auto* button = get_if<MouseDown>( &shown );
              button and not ranges::contains( mButtonsDown, button->mButton ) )
        mButtonsDown.push_back( button->mButton );
    else if ( const auto* buttonUp = get_if<MouseUp>( &shown ) )
        erase( mButtonsDown, buttonUp->mButton );

    if ( GetWorld().Get<UiWorld>().Take( shown ) )
        return;
    mInput.Handle( shown );
}

void PlaySession::Release()
{
    for ( const Key key : vector<Key>( mKeysDown ) )
        Handle( KeyUp{ .mKey = key } );
    for ( const MouseButton button : vector<MouseButton>( mButtonsDown ) )
        Handle( MouseUp{ .mButton = button, .mPosition = mCorner } );
    mKeysDown.clear();
    mButtonsDown.clear();
}
}
