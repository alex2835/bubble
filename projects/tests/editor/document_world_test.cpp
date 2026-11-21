// A document's world as the editor shows it, with no GPU: the camera placed
// around what a document without a camera shows once its models have come,
// and a prefab with no light of its own lit by the editor's sun.
#include "bubble/audio/audio.hpp"
#include "bubble/editor/document_world.hpp"
#include "bubble/editor/editor_project.hpp"
#include <doctest.h>

using namespace bubble;

#if defined( BUBBLE_IMPORT_MODELS ) and not defined( __EMSCRIPTEN__ )
namespace
{
constexpr string_view cProject = R"({ "version": 1, "name": "views", "actions": {} })";

// A unit cube.
constexpr string_view cCube = R"(v -0.5 -0.5 -0.5
v 0.5 -0.5 -0.5
v 0.5 0.5 -0.5
v -0.5 0.5 -0.5
v -0.5 -0.5 0.5
v 0.5 -0.5 0.5
v 0.5 0.5 0.5
v -0.5 0.5 0.5
f 1 4 3 2
f 5 6 7 8
f 1 5 8 4
f 2 3 7 6
f 1 2 6 5
f 4 8 7 3
)";
constexpr string_view cCubeImport = R"({ "version": 1, "asset": "c0be0000c0be0000c0be0000c0be0000", "settings": {} })";

constexpr string_view cCrate = R"({
  "version": 1,
  "next_id": 1,
  "root": {
    "id": 0,
    "components": {
      "transform": { "position": [ 5, 0, 0 ] },
      "model": { "asset": { "asset": "c0be0000c0be0000c0be0000c0be0000", "path": "cube.obj" } }
    }
  }
})";

constexpr string_view cLamp = R"({
  "version": 1,
  "next_id": 2,
  "root": {
    "id": 0,
    "components": { "transform": {}, "point_light": { "intensity": 4, "range": 6 } },
    "children": [
      { "id": 1, "name": "eye", "components": { "transform": { "position": [ 0, 3, 9 ] }, "camera": {} } }
    ]
  }
})";

constexpr string_view cRoom = R"({
  "version": 1,
  "root": {
    "children": [
      {
        "name": "cube",
        "components": {
          "transform": { "position": [ 0, 0, -3 ] },
          "model": { "asset": { "asset": "c0be0000c0be0000c0be0000c0be0000", "path": "cube.obj" } }
        }
      }
    ]
  }
})";

struct Views
{
    hmap<string, string> mFiles{ { "project.bubble", string( cProject ) },     { "cube.obj", string( cCube ) },
                                 { "cube.obj.import", string( cCubeImport ) }, { "crate.prefab", string( cCrate ) },
                                 { "lamp.prefab", string( cLamp ) },           { "room.scene", string( cRoom ) } };
    Scope<AudioDevice> mAudio = AudioDevice::Offline();
    Scope<EditorProject> mProject;

    Views()
    {
        const ProjectFiles files = ProjectFiles::InMemory( mFiles );
        auto projectMaybe =
            EditorProject::TryOpen( files, AssetRegistry::AtOnce( [files]( const AssetId& assetId, string_view kind ) {
                                        return files.TryRead( ImportedPath( assetId, kind ) );
                                    } ),
                                    *mAudio );
        REQUIRE_MESSAGE( projectMaybe, projectMaybe.error() );
        mProject = std::move( *projectMaybe );
    }

    // The document at `path` open and shown for `frames` frames, as the
    // editor shows the active one.
    DocumentWorld& Show( string_view path, int frames = 10 )
    {
        REQUIRE( mProject->Run( "document.open", *Json::TryParse( format( R"({{ "path": "{}" }})", path ) ) ) );
        DocumentWorld& view = ViewOf( mProject->GetApp(), *mProject->GetWorkspace().TryActive() );
        for ( int frame = 0; frame < frames; ++frame )
        {
            mProject->GetApp().GetJobs().RunMainQueue();
            view.Get().Frame( 1.0 / 60 );
            view.Gathered();
        }
        return view;
    }
};
}

TEST_CASE( "A prefab's view frames what it shows and lights it with the editor's sun while it has no light" )
{
    Views views;
    DocumentWorld& crate = views.Show( "crate.prefab" );
    const EditorCamera& camera = crate.Camera();
    CHECK( glm::length( camera.mFocus - vec3( 5, 0, 0 ) ) < 1e-4f );
    // The cube's corners fit the view.
    CHECK( camera.mDistance * sin( camera.mFov * 0.5f ) > sqrt( 3.0f ) * 0.5f );
    CHECK( camera.mDistance < 5.0f );
    const RenderScene& scene = crate.Get().Get<RenderScene>();
    REQUIRE( scene.mLights.size() == 1 );
    CHECK( scene.mLights[0].mKind == RenderLightKind::Directional );
    CHECK( scene.mLights[0].mShadows );
    // Never in the document's scene.
    CHECK_FALSE( views.mProject->GetWorkspace().TryActive()->GetScene().Has<DirectionalLight>(
        views.mProject->GetWorkspace().TryActive()->GetScene().Root() ) );

    // A prefab with a light of its own is lit by it alone; its camera is the
    // game's, and the view is not placed there.
    DocumentWorld& lamp = views.Show( "lamp.prefab" );
    const RenderScene& lit = lamp.Get().Get<RenderScene>();
    REQUIRE( lit.mLights.size() == 1 );
    CHECK( lit.mLights[0].mKind == RenderLightKind::Point );
    CHECK( lit.mCameras.size() == 1 );
    CHECK( lamp.Camera().mFocus == EditorCamera().mFocus );
    CHECK( lamp.Camera().mYaw == EditorCamera().mYaw );
}

TEST_CASE( "A level with no camera is framed around its models, and no sun is added to it" )
{
    Views views;
    DocumentWorld& room = views.Show( "room.scene" );
    CHECK( glm::length( room.Camera().mFocus - vec3( 0, 0, -3 ) ) < 1e-4f );
    CHECK( room.Get().Get<RenderScene>().mLights.empty() );
}
#endif
